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

#include "requirement.hpp"

namespace mtconnect
{
  namespace entity
  {
    using ErrorList = std::list<PropertyError>;
    using Properties = std::map<std::string, Value>;

    class Entity : public std::enable_shared_from_this<Entity>
    {
    public:
      Entity(const std::string &name, const Properties &props)
        : m_name(name), m_properties(props)
      {
      }
      Entity(const Entity &entity) = default;
      virtual ~Entity()
      {
      }
      
      std::shared_ptr<Entity> getptr() {
        return shared_from_this();
      }

      const std::string &getName() const { return m_name; }
      const Properties &getProperties() const
      {
        return m_properties;
      }
      const Value &getProperty(const std::string &n) const
      {
        static Value noValue{nullptr};
        auto it = m_properties.find(n);
        if (it == m_properties.end())
          return noValue;
        else
          return it->second;
      }

      const Value &getValue() const { return getProperty("value"); }
      std::optional<EntityList> getList(const std::string &name) const {
        auto &v = getProperty(name);
        auto *p = std::get_if<EntityPtr>(&v);
        if (p)
        {
          auto &lv = (*p)->getValue();
          auto *l = std::get_if<EntityList>(&lv);
          if (l)
            return *l;
        }
        
        return std::nullopt;
      }
      
      // Entity Factory
    protected:
      std::string m_name;
      Properties m_properties;
    };
  }
}
