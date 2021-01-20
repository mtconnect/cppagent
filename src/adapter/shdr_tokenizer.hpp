//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "adapter.hpp"
#include "entity/entity.hpp"

#include <regex>
#include <chrono>

namespace mtconnect
{
  class Agent;
  
  namespace adapter
  {
    using TokenList = std::list<std::string>;

    class ShdrTokenizer
    {
    public:
      ShdrTokenizer() = default;
      
      template<typename T>
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

      
      static TokenList tokenize(const std::string &data)
      {
        using namespace std;
        
        TokenList tokens;
        string text(data);

        while (!text.empty())
        {
          smatch match;
          auto res = regex_search(text.cbegin(), text.cend(), match,
                                  m_pattern);
          if (res)
          {
            // Match 2 is a quoted string with escaped \| and match 5 is
            // a simple pipe delimeter. Must be terminated with end of string or
            // trailing pipe. Trim the text
            if (match[2].matched)
              tokens.emplace_back(trim(remove(match[2], '\\')));
            else if (match[5].matched)
              tokens.emplace_back(trim(match[5]));

            // Check the suffix to see what we need to do next
            auto &suff = match.suffix();
            if (suff.first == suff.second && match[6].matched && match[6] == "|")
            {
              // If the text has just a pipe at the end, then we have an
              // empty trailing token
              text.clear();
              tokens.emplace_back("");
            }
            else if (suff.first != suff.second)
            {
              // Normal next token, grab the suffix after the '|' character
              auto f = suff.first;
              if (*f == '|')
                f++;
              // Check for an empty trailing token
              text = string(f, suff.second);
              if (text.empty())
                tokens.emplace_back("");
            }
            else
            {
              // Nothing left
              text.clear();
            }
          }
          else
          {
            cout << "Cound not match: " << text << endl;
            text.clear();
          }
        }
        
        return tokens;
      }
      
    protected:
      static std::regex m_pattern;
    };    
  }
}
