//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/beast/http/verb.hpp>

#include <list>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <variant>

#include "mtconnect/config.hpp"
#include "mtconnect/logging.hpp"
#include "parameter.hpp"
#include "request.hpp"
#include "session.hpp"

namespace mtconnect::sink::rest_sink {
  class Session;
  using SessionPtr = std::shared_ptr<Session>;

  /// @brief A REST routing that parses a URI pattern and associates a lambda when it is matched
  /// against a request
  class AGENT_LIB_API Routing
  {
  public:
    using Function = std::function<bool(SessionPtr, RequestPtr)>;

    Routing(const Routing &r) = default;
    /// @brief Create a routing
    ///
    /// Creates a REGEXP to match against the path
    /// @param verb The `GET`, `PUT`, `POST`, and `DELETE` version of the HTTP request
    /// @param pattern the URI pattern to parse and match
    /// @param function the function to call if matches
    Routing(boost::beast::http::verb verb, const std::string &pattern, const Function function)
      : m_verb(verb), m_function(function)
    {
      std::string s(pattern);

      auto qp = s.find_first_of('?');
      if (qp != std::string::npos)
      {
        auto query = s.substr(qp + 1);
        s.erase(qp);

        queryParameters(query);
      }

      pathParameters(s);
    }
    /// @brief
    /// @param verb
    /// @param pattern
    /// @param function
    Routing(boost::beast::http::verb verb, const std::regex &pattern, const Function function)
      : m_verb(verb), m_pattern(pattern), m_function(function)
    {}

    /// @brief Get the list of path position in order
    /// @return the parameter list
    const ParameterList &getPathParameters() const { return m_pathParameters; }
    /// @brief get the unordered set of query parameters
    const QuerySet &getQueryParameters() const { return m_queryParameters; }

    /// @brief match the session's request against the this routing
    ///
    /// Call the associated lambda when matched
    ///
    /// @param[in] session the session making the request to pass to the Routing if matched
    /// @param[in,out] request the incoming request with a verb and a path
    /// @return `true` if the request was matched
    bool matches(SessionPtr session, RequestPtr request)
    {
      try
      {
        request->m_parameters.clear();
        std::smatch m;
        if (m_verb == request->m_verb && std::regex_match(request->m_path, m, m_pattern))
        {
          auto s = m.begin();
          s++;
          for (auto &p : m_pathParameters)
          {
            if (s != m.end())
            {
              ParameterValue v(s->str());
              request->m_parameters.emplace(make_pair(p.m_name, v));
              s++;
            }
          }

          for (auto &p : m_queryParameters)
          {
            auto q = request->m_query.find(p.m_name);
            if (q != request->m_query.end())
            {
              try
              {
                auto v = convertValue(q->second, p.m_type);
                request->m_parameters.emplace(make_pair(p.m_name, v));
              }
              catch (ParameterError &e)
              {
                std::string msg =
                    std::string("for query parameter '") + p.m_name + "': " + e.what();
                throw ParameterError(msg);
              }
            }
            else if (!std::holds_alternative<std::monostate>(p.m_default))
            {
              request->m_parameters.emplace(make_pair(p.m_name, p.m_default));
            }
          }
          return m_function(session, request);
        }
      }

      catch (ParameterError &e)
      {
        LOG(debug) << "Pattern error: " << e.what();
        throw e;
      }

      return false;
    }

  protected:
    void pathParameters(std::string s)
    {
      std::regex reg("\\{([^}]+)\\}");
      std::smatch match;
      std::stringstream pat;

      while (regex_search(s, match, reg))
      {
        pat << match.prefix() << "([^/]+)";
        m_pathParameters.emplace_back(match[1]);
        s = match.suffix().str();
      }
      pat << s;
      pat << "/?";

      m_patternText = pat.str();
      m_pattern = std::regex(m_patternText);
    }

    void queryParameters(std::string s)
    {
      std::regex reg("([^=]+)=\\{([^}]+)\\}&?");
      std::smatch match;

      while (regex_search(s, match, reg))
      {
        Parameter qp(match[1]);
        qp.m_part = QUERY;

        getTypeAndDefault(match[2], qp);

        m_queryParameters.emplace(qp);
        s = match.suffix().str();
      }
    }

    void getTypeAndDefault(const std::string &type, Parameter &par)
    {
      std::string t(type);
      auto dp = t.find_first_of(':');
      std::string def;
      if (dp != std::string::npos)
      {
        def = t.substr(dp + 1);
        t.erase(dp);
      }

      if (t == "string")
      {
        par.m_type = STRING;
      }
      else if (t == "integer")
      {
        par.m_type = INTEGER;
      }
      else if (t == "unsigned_integer")
      {
        par.m_type = UNSIGNED_INTEGER;
      }
      else if (t == "double")
      {
        par.m_type = DOUBLE;
      }

      if (!def.empty())
      {
        par.m_default = convertValue(def, par.m_type);
      }
    }

    ParameterValue convertValue(const std::string &s, ParameterType t) const
    {
      switch (t)
      {
        case STRING:
          return s;

        case NONE:
          throw ParameterError("Cannot convert to NONE");

        case DOUBLE:
        {
          char *ep = nullptr;
          const char *sp = s.c_str();
          double r = strtod(sp, &ep);
          if (ep == sp)
            throw ParameterError("cannot convert string '" + s + "' to double");
          return r;
        }

        case INTEGER:
        {
          char *ep = nullptr;
          const char *sp = s.c_str();
          int32_t r = int32_t(strtoll(sp, &ep, 10));
          if (ep == sp)
            throw ParameterError("cannot convert string '" + s + "' to integer");

          return r;
        }

        case UNSIGNED_INTEGER:
        {
          char *ep = nullptr;
          const char *sp = s.c_str();
          uint64_t r = strtoull(sp, &ep, 10);
          if (ep == sp)
            throw ParameterError("cannot convert string '" + s + "' to unsigned integer");

          return r;
        }
      }

      throw ParameterError("Unknown type for conversion: " + std::to_string(int(t)));

      return ParameterValue();
    }

  protected:
    boost::beast::http::verb m_verb;
    std::regex m_pattern;
    std::string m_patternText;
    ParameterList m_pathParameters;
    QuerySet m_queryParameters;
    Function m_function;
  };
}  // namespace mtconnect::sink::rest_sink
