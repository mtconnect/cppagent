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

#include "entity/entity.hpp"

namespace mtconnect {
  namespace device_model {
    class Component;
    class Composition : public entity::Entity
    {
    public:
      using entity::Entity::Entity;
      static entity::FactoryPtr getFactory();
      static entity::FactoryPtr getRoot();

      const std::string getTopicName() const
      {
        using namespace std;
        using namespace entity;
        if (!m_topicName)
        {
          optional<string> opt;
          string topicName = pascalize(get<string>("type"), opt);
          auto name = maybeGet<std::string>("name");
          if (name)
            topicName.append("[").append(*name).append("]");

          auto *self = const_cast<Composition *>(this);
          self->m_topicName.emplace(topicName);
        }
        return *m_topicName;
      }

      void setComponent(std::shared_ptr<Component> component) { m_component = component; }
      std::shared_ptr<Component> getComponent() const { return m_component.lock(); }
      auto getParent() const { return getComponent(); }

    protected:
      std::optional<std::string> m_topicName;
      std::weak_ptr<Component> m_component;
    };

    using CompositionPtr = std::shared_ptr<Composition>;
  }  // namespace device_model
}  // namespace mtconnect
