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

using namespace std;

namespace mtconnect
{
  namespace entity {
    static dlib::logger g_logger("EntityRequirement");

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
            throw PropertyRequirementError("Entity list requirement " + m_name +
                                           " must have at least " +
                                           to_string(m_lowerMultiplicity) +
                                           " and no more than " +
                                           to_string(m_upperMultiplicity) +
                                           "entries, " + to_string(l.size()) +
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
  }
}

