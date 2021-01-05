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
#include <set>
#include <strstream>
#include <variant>

#include <dlib/logger.h>

namespace mtconnect 
{
  namespace http_server
  {
    class ParameterError : public std::logic_error
    {
      using std::logic_error::logic_error;
    };
    
    class Response;
    
    class Routing
    {
    public:
      enum ParameterType {
        NONE = 0,
        STRING = 1,
        INTEGER = 2,
        UNSIGNED_INTEGER = 3,
        DOUBLE = 4
      };
      enum UrlPart {
        PATH,
        QUERY
      };
      using ParameterValue = std::variant<std::monostate,
                         std::string, int32_t, uint64_t, double>;
      struct Parameter {
        Parameter() = default;
        Parameter(const std::string &n, ParameterType t = STRING,
                  UrlPart p = PATH) : m_name(n), m_type(t), m_part(p) {}
        Parameter(const Parameter &o) = default;
        
        std::string m_name;
        ParameterType m_type { STRING };
        ParameterValue m_default;
        UrlPart m_part { PATH };
        
        bool operator <(const Parameter &o) const { return m_name < o.m_name; }
      };
      
      using ParameterList = std::list<Parameter>;
      using QuerySet = std::set<Parameter>;
      using ParameterMap = std::map<std::string,ParameterValue>;
      using QueryMap = std::map<std::string,std::string>;
      
      struct Request
      {
        std::string m_body;
        std::string m_accepts;
        std::string m_contentType;
        std::string m_verb;
        std::string m_path;
        QueryMap    m_query;
        ParameterMap m_parameters;
      };
      
      using Function = std::function<bool(const Request &, Response &response)>;
      
      Routing(const Routing &r ) = default;
      Routing(const std::string &verb,
              const std::string &pattern,
              const Function function)
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
      Routing(const std::string &verb,
              const std::regex &pattern,
              const Function function)
      : m_verb(verb), m_pattern(pattern), m_function(function)
      {
        
      }

      
      const ParameterList &getPathParameters() const { return m_pathParameters; }
      const QuerySet &getQueryParameters() const { return m_queryParameters; }
      
      bool matches(Request &request, Response &response) const
      {
        try
        {
          request.m_parameters.clear();
          std::smatch m;
          if (m_verb == request.m_verb && std::regex_match(request.m_path, m, m_pattern))
          {
            auto s = m.begin();
            s++;
            for (auto &p : m_pathParameters)
            {
              if (s != m.end())
              {
                ParameterValue v(s->str());
                request.m_parameters.emplace(make_pair(p.m_name, v));
                s++;
              }
            }
            
            for (auto &p : m_queryParameters)
            {
              auto q = request.m_query.find(p.m_name);
              if (q != request.m_query.end())
              {
                request.m_parameters.emplace(make_pair(p.m_name, convertValue(q->second, p.m_type)));
              }
              else if (!std::holds_alternative<std::monostate>(p.m_default))
              {
                request.m_parameters.emplace(make_pair(p.m_name, p.m_default));
              }
            }
            return m_function(request, response);
          }
        }
          
        catch (ParameterError &e)
        {
          m_logger << dlib::LDEBUG << "Pattern error: " << e.what();
        }
        
        return false;
      }
      
    protected:
      void pathParameters(std::string s)
      {
        std::regex reg("\\{([^}]+)\\}");
        std::smatch match;
        std::strstream pat;
        
        while (regex_search(s, match, reg))
        {
          pat << match.prefix() << "([^/]+)";
          m_pathParameters.emplace_back(match[1]);
          s = match.suffix().str();
        }
        pat << s;

        std::string l(pat.str());
        m_pattern = std::regex(pat.str());
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
      
      ParameterValue convertValue(const std::string &s,
                                  ParameterType t) const
      {
        switch(t)
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
            int32_t r = strtoll(sp, &ep, 10);
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
      }
      
    protected:
      std::string m_verb;
      std::regex m_pattern;
      ParameterList m_pathParameters;
      QuerySet m_queryParameters;
      Function m_function;
      static dlib::logger m_logger;

    };
  }    
}
