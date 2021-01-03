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

#include <string>
#include <regex>
#include <list>
#include <strstream>

namespace mtconnect 
{
  namespace http_server
  {
    class Routing
    {
    public:
      using ParameterList = std::list<std::string>;
      using ParameterMap = std::map<std::string,std::string>;
      using Function = std::function<void(const ParameterMap &)>;

      Routing(const Routing &r ) = default;
      Routing(const std::string &verb,
              const std::string &pattern,
              const Function function = nullptr)
      : m_verb(verb), m_function(function)
      {
        std::regex reg("\\{([^}]+)\\}");
        std::smatch match;
        std::strstream pat;
        std::string s(pattern);
        
        pat << "/?";
        while (regex_search(s, match, reg))
        {
          pat << match.prefix() << "([^/]+)";
          m_parameters.emplace_back(match[1]);
          s = match.suffix().str();
        }
        pat << s;
        
        m_pattern = std::regex(pat.str());
      }
      
      const ParameterList &getParameters() const { return m_parameters; }
      
      std::optional<ParameterMap> matches(const std::string &verb,
                                          const std::string &url)
      {
        std::smatch m;
        if (m_verb == verb && std::regex_match(url, m, m_pattern))
        {
          ParameterMap res;
          
          auto s = m.begin();
          s++;
          for (auto &p : m_parameters)
          {
            if (s != m.end())
            {
              res.emplace(p, s->str());
              s++;
            }
          }
          
          if (m_function)
            m_function(res);
          
          return res;
        }
        
        return std::nullopt;
      }
      
    protected:
      std::string m_verb;
      std::regex m_pattern;
      ParameterList m_parameters;
      Function m_function;
    };
  }    
}
