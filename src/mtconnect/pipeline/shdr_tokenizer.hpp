//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#pragma once

#include <chrono>
#include <regex>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  /// @brief A list of strings
  using TokenList = std::list<std::string>;
  /// @brief An entity that has carries list of tokens
  class AGENT_LIB_API Tokens : public entity::Entity
  {
  public:
    using entity::Entity::Entity;
    Tokens(const Tokens &) = default;
    Tokens() = default;
    Tokens(const Tokens &ts, TokenList list) : Entity(ts), m_tokens(list) {}

    TokenList m_tokens;
  };

  /// @brief Splits a line of SHDR into fields using a pipe (`|`) delimeter
  class AGENT_LIB_API ShdrTokenizer : public Transform
  {
  public:
    ShdrTokenizer(const ShdrTokenizer &) = default;
    ShdrTokenizer() : Transform("ShdrTokenizer") { m_guard = EntityNameGuard("Data", RUN); }
    ~ShdrTokenizer() = default;

    entity::EntityPtr operator()(entity::EntityPtr &&data) override
    {
      auto &body = data->getValue<std::string>();
      entity::Properties props;
      if (auto source = data->maybeGet<std::string>("source"))
        props["source"] = *source;
      auto result = std::make_shared<Tokens>("Tokens", props);
      tokenize(body, result->m_tokens);
      return next(result);
    }

    template <typename T>
    inline static std::string remove(const T &range, const char c)
    {
      using namespace std;
      string res;
      copy_if(range.first, range.second, std::back_inserter(res),
              [&c](const char m) { return c != m; });

      return res;
    }

    inline static std::string trim(const std::string &str)
    {
      using namespace std;

      auto first = str.find_first_not_of(" \r\n\t");
      auto last = str.find_last_not_of(" \r\n\t");

      if (first == string::npos)
        return "";
      else if (first == 0 && last == str.length() - 1)
        return str;
      else
        return str.substr(first, last - first + 1);
    }

    static inline void tokenize(const std::string &data, TokenList &tokens)
    {
      using namespace std;
      auto cp = data.c_str();
      std::string token;
      bool copied {false};
      while (*cp != '\0')
      {
        while (*cp != '\0' && isspace(*cp))
          cp++;

        auto start = cp, orig = cp;
        const char *end = 0;
        if (*cp == '"')
        {
          cp = ++start;
          while (*cp != '\0')
          {
            if (*cp == '\\')
            {
              if (!copied)
              {
                token = start;
                size_t dist = cp - start;
                start = token.c_str();
                cp = start + dist;
                copied = true;
              }
              memmove(const_cast<char *>(cp), cp + 1, strlen(cp));
            }
            else if (*cp == '|')
            {
              break;
            }
            else if (*cp == '"')
            {
              // Make sure there is a | or the string ends after the
              // terminal ". Skip spaces.
              auto nc = cp + 1;
              while (*nc != '\0' && isspace(*nc))
                nc++;
              if (*nc == '|' || *nc == '\0')
                end = cp;
              else
                break;
            }

            if (*cp != '\0')
              cp++;
          }
          // If there was no terminating '"'
          if (end == 0 && copied)
          {
            // Undo copy
            cp = start = orig;
            while (*cp != '|' && *cp != '\0')
              cp++;
          }
        }
        else
        {
          while (*cp != '|' && *cp != '\0')
            cp++;
        }

        if (end == 0)
          end = cp;

        while (end > start && isspace(*(end - 1)))
          end--;

        tokens.emplace_back(start, end);

        // Handle terminal '|'
        if (*cp == '|' && *(cp + 1) == '\0')
          tokens.emplace_back("");
        if (*cp != '\0')
          cp++;
      }
    }
  };
}  // namespace mtconnect::pipeline
