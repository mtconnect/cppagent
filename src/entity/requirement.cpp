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

#include <dlib/logger.h>

#include <cctype>
#include <cstdlib>

using namespace std;

namespace mtconnect
{
  namespace entity
  {
    static dlib::logger g_logger("EntityRequirement");

    Requirement::Requirement(const std::string &name, ValueType type, FactoryPtr &f, bool required)
      : m_name(name), m_upperMultiplicity(1), m_lowerMultiplicity(required ? 1 : 0), m_type(type)
    {
      if (type == ENTITY_LIST)
      {
        f->setList(true);
      }
      m_factory = f;
    }

    Requirement::Requirement(const std::string &name, ValueType type, FactoryPtr &f, int lower,
                             int upper)
      : m_name(name), m_upperMultiplicity(upper), m_lowerMultiplicity(lower), m_type(type)
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
          throw PropertyError("For entity or list requirement " + m_name + ", no factory", m_name);
        }
        if (holds_alternative<EntityPtr>(value))
        {
          const auto e = get<EntityPtr>(value);
          if (!matches(e->getName()))
          {
            throw PropertyError(
                "Requirement " + m_name + " does not have a matching entity name: " + e->getName(),
                m_name);
          }
        }
        else if (holds_alternative<EntityList>(value))
        {
          const auto l = std::get<EntityList>(value);
          if (int(l.size()) > m_upperMultiplicity || int(l.size()) < m_lowerMultiplicity)
          {
            string upper;
            if (m_upperMultiplicity != Infinite)
              upper = " and no more than " + to_string(m_upperMultiplicity);
            throw PropertyError("Entity list requirement " + m_name + " must have at least " +
                                    to_string(m_lowerMultiplicity) + upper + " entries, " +
                                    to_string(l.size()) + " found",
                                m_name);
          }
          for (const auto &e : l)
          {
            if (!matches(e->getName()))
            {
              throw PropertyError("Entity list requirement " + m_name +
                                      " does not match requirement with name " + e->getName(),
                                  m_name);
            }
          }
        }
        else
        {
          throw PropertyError(
              "Entity or list requirement " + m_name + " does not have correct type", m_name);
        }
      }
      else
      {
        if (value.index() != m_type)
        {
          throw PropertyError("Incorrect type for property " + m_name);
        }
        if (std::holds_alternative<std::string>(value))
        {
          auto &v = std::get<std::string>(value);
          if (v.empty())
          {
            throw PropertyError("Value of " + m_name + " is empty", m_name);
          }
          if (m_pattern && !std::regex_match(v, *m_pattern))
          {
            throw PropertyError("Invalid value for '" + m_name + "': '" + v + "' is not allowed",
                                m_name);
          }
        }
      }

      return true;
    }

    struct StringConverter
    {
      StringConverter(const string &s) : m_string(s) {}

      operator double() const
      {
        char *ep = nullptr;
        const char *sp = m_string.c_str();
        double r = strtod(sp, &ep);
        if (ep == sp)
          throw PropertyError("cannot convert string '" + m_string + "' to double");

        return r;
      }

      operator int64_t() const
      {
        char *ep = nullptr;
        const char *sp = m_string.c_str();
        int64_t r = strtoll(sp, &ep, 10);
        if (ep == sp)
          throw PropertyError("cannot convert string '" + m_string + "' to integer");

        return r;
      }

      operator bool() const { return m_string == "true"; }

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
            throw PropertyError("cannot convert string '" + m_string + "' to vector");
          }
          cp = np;
        }

        if (r.size() == 0)
          throw PropertyError("cannot convert string '" + m_string + "' to vector");

        return r;
      }

      const string &m_string;
    };

    bool ConvertValueToType(Value &value, ValueType type)
    {
      bool converted = false;
      if (value.index() != type)
      {
        visit(overloaded{[&](const string &arg) {
                           StringConverter s(arg);
                           switch (type)
                           {
                             case INTEGER:
                               value = int64_t(s);
                               converted = true;
                               break;

                             case DOUBLE:
                               value = double(s);
                               converted = true;
                               break;

                             case BOOL:
                               value = bool(s);
                               converted = true;
                               break;

                             case VECTOR:
                               value = Vector(s);
                               converted = true;
                               break;

                             default:
                               throw PropertyError("Cannot convert a string to a non-scalar");
                               break;
                           }
                         },
                         [&](double arg) {
                           switch (type)
                           {
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

                             case BOOL:
                               value = arg != 0.0;
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
                               throw PropertyError("Cannot convert a double to a non-scalar");
                               break;
                           }
                         },
                         [&](int64_t arg) {
                           switch (type)
                           {
                             case STRING:
                               value = to_string(arg);
                               converted = true;
                               break;

                             case DOUBLE:
                               value = double(arg);
                               converted = true;
                               break;

                             case BOOL:
                               value = arg != 0;
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
                               throw PropertyError("Cannot convert a integer to a non-scalar");
                               break;
                           }
                         },
                         [&](const Vector &arg) {
                           switch (type)
                           {
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
                                 throw PropertyError("Cannot convert a empty Vector to a string");
                               }
                             }

                             default:
                               throw PropertyError(
                                   "Cannot convert a Vector to anything other than a string");
                               break;
                           }
                         },
                         [&](const bool b) {
                           switch (type)
                           {
                             case INTEGER:
                               value = int64_t(b);
                               converted = true;
                               break;

                             case DOUBLE:
                               value = double(b);
                               converted = true;
                               break;

                             case STRING:
                               value = b ? string("true") : string("false");
                               converted = true;
                               break;

                             case VECTOR:
                               value = Vector(double(b));
                               converted = true;
                               break;

                             default:
                               throw PropertyError("Cannot convert a string to a non-scalar");
                               break;
                           }
                         },
                         [&](const auto &arg) { converted = false; }},
              value);
      }
      else
      {
        converted = true;
      }
      return converted;
    }
  }  // namespace entity
}  // namespace mtconnect
