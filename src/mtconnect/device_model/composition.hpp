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

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"

namespace mtconnect {
  namespace device_model {
    class Component;

    /// @brief Composition entity
    class AGENT_LIB_API Composition : public entity::Entity
    {
    public:
      using entity::Entity::Entity;
      static entity::FactoryPtr getFactory();
      static entity::FactoryPtr getRoot();

      /// @brief create a topic name from the component
      /// @return topic name
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

      /// @brief component this composition is a part of
      /// @param[in] component the component
      void setComponent(std::shared_ptr<Component> component) { m_component = component; }
      /// @brief get the component this is a part of
      /// @return shared pointer to the component
      std::shared_ptr<Component> getComponent() const { return m_component.lock(); }
      /// @brief cover method for getComponent
      /// @return shared pointer to the component
      auto getParent() const { return getComponent(); }

    protected:
      std::optional<std::string> m_topicName;
      std::weak_ptr<Component> m_component;
    };

    using CompositionPtr = std::shared_ptr<Composition>;
  }  // namespace device_model
}  // namespace mtconnect
