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

#include "requirement.hpp"
#include "entity.hpp"
#include "factory.hpp"
#include <cstdlib>
#include <cctype>
#include <dlib/logger.h>

using namespace std;

namespace mtconnect
{
  namespace entity {
    static dlib::logger g_logger("EntityRequirement");

    Requirement::Requirement(const std::string &name, Type type, FactoryPtr &f,
                             bool required)
    : m_name(name), m_type(type), m_upperMultiplicity(1),
    m_lowerMultiplicity(required ? 1 : 0)
    {
      if (type == ENTITY_LIST)
      {
        f->setList(true);
      }
      m_factory = f;
    }
    
    Requirement::Requirement(const std::string &name, Type type, FactoryPtr &f,
                             int lower, int upper)
    : m_name(name), m_type(type), m_upperMultiplicity(upper), m_lowerMultiplicity(lower)
    {
      if (type == ENTITY_LIST)
      {
        f->setList(true);
      }
      m_factory = f;
    }
    
    bool Requirement::isMetBy(const Value &value, bool isList) const
    {
      // Is this a multiple entry
      if ((m_type == ENTITY || m_type == ENTITY_LIST))
      {
        if (!m_factory)
        {
          throw PropertyRequirementError("For entity or list requirement " + m_name + ", no factory");
        }
        if (holds_alternative<EntityPtr>(value))
        {
          const auto e = get<EntityPtr>(value);
          if (!matches(e->getName()))
          {
            throw PropertyRequirementError("Requirement " + m_name +
                                           " does not have a matching entity name: " +
                                           e->getName());
          }
        }
        else if (holds_alternative<EntityList>(value))
        {
          const auto l = std::get<EntityList>(value);
          if (l.size() > m_upperMultiplicity || l.size() < m_lowerMultiplicity)
          {
            string upper;
            if (m_upperMultiplicity != Infinite)
              upper = " and no more than " + to_string(m_upperMultiplicity);
            throw PropertyRequirementError("Entity list requirement " + m_name +
                                           " must have at least " +
                                           to_string(m_lowerMultiplicity) +
                                           upper + " entries, " + to_string(l.size()) +
                                           " found");
          }
          for (const auto &e : l)
          {
            if (!matches(e->getName()))
            {
              throw PropertyRequirementError("Entity list requirement " + m_name +
                                             " does not match requirement with name " + e->getName());
            }
          }
        }
        else
        {
          throw PropertyRequirementError("Entity or list requirement " + m_name +
                                         " does not have correct type");
        }
      }
      else
      {
        if (value.index() != m_type)
        {
          throw PropertyTypeError("Incorrect type for property " + m_name);
        }
        if (std::holds_alternative<std::string>(value) &&
            std::get<std::string>(value).empty())
        {
          throw PropertyRequirementError("Value of " + m_name + " is empty");
        }
      }
      
      return true;
    }
    
    struct StringConverter
    {
      StringConverter(const string &s) : m_string(s)
      {
      }
      
      operator double() const
      {
        char *ep = nullptr;
        const char *sp = m_string.c_str();
        double r = strtod(sp, &ep);
        if (ep == sp)
          throw PropertyConversionError("cannot convert string '" + m_string + "' to double");
        
        return r;
      }
      
      operator int64_t() const
      {
        char *ep = nullptr;
        const char *sp = m_string.c_str();
        int64_t r = strtoll(sp, &ep, 10);
        if (ep == sp)
          throw PropertyConversionError("cannot convert string '" + m_string + "' to integer");
        
        return r;
      }
      
      operator Vector() const
      {
        char *np(nullptr);
        const char *cp = m_string.c_str();
        Vector r;
        
        while (cp && *cp != '\0')
        {
          if (isspace(*cp))
          {
            cp++;
            continue;
          }
          
          double v = strtod(cp, &np);
          if (cp != np)
          {
            r.emplace_back(v);
          }
          else
          {
            throw PropertyConversionError("cannot convert string '" + m_string + "' to vector");
          }
          cp = np;
        }

        if (r.size() == 0)
          throw PropertyConversionError("cannot convert string '" + m_string + "' to vector");

        return r;
      }
      
      const string &m_string;
    };
    
    bool Requirement::convertType(Value &value) const
    {
      bool converted = false;
      if (value.index() != m_type)
      {
        visit(overloaded {
          [&](const string &arg) {
            StringConverter s(arg);
            switch (m_type) {
              case INTEGER:
                value = int64_t(s);
                converted = true;
                break;

              case DOUBLE:
                value = double(s);
                converted = true;
                break;

              case VECTOR:
                value = Vector(s);
                converted = true;
                break;

              default:
                throw PropertyConversionError("Cannot convert a string to a non-scalar");
                break;
            }
          },
          [&](double arg) {
            switch (m_type) {
              case STRING:
              {
                stringstream s;
                s << arg;
                value = s.str();
                converted = true;
                break;
              }
              case INTEGER:
                value = int64_t(arg);
                converted = true;
                break;
                
              case VECTOR:
              {
                Vector v{arg};
                value = v;
                converted = true;
                break;
              }
                
              default:
                throw PropertyConversionError("Cannot convert a double to a non-scalar");
                break;
            }

          },
          [&](int64_t arg) {
            switch (m_type) {
              case STRING:
                value = to_string(arg);
                converted = true;
                break;
                
              case DOUBLE:
                value = double(arg);
                converted = true;
                break;
                
              case VECTOR:
              {
                Vector v{double(arg)};
                value = v;
                converted = true;
                break;
              }
                
              default:
                throw PropertyConversionError("Cannot convert a integer to a non-scalar");
                break;
            }
          },
          [&](const Vector &arg) {
            switch (m_type) {
              case STRING:
              {
                if (arg.size() > 0)
                {
                  stringstream s;
                  for (auto &v : arg)
                    s << v << ' ';
                  auto str = s.str();
                  str.erase(str.length() - 1);
                  value = str;
                  converted = true;
                  break;
                }
                else
                {
                  throw PropertyConversionError("Cannot convert a empty Vector to a string");
                }
              }
              
              default:
                throw PropertyConversionError("Cannot convert a Vector to anything other than a string");
                break;
            }
          },
          [&](auto arg) {
            converted = false;
          }
        }, value);
      }
      return converted;
    }
  }
}

