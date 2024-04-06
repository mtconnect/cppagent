//
// Copyright Copyright 2009-2024, AMT � The Association For Manufacturing Technology (�AMT�)
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

#include <list>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      class DataItem;
    }
    class Component;
    class Device;
    using DevicePtr = std::shared_ptr<Device>;

    /// @brief Reference to a component or data item
    class AGENT_LIB_API Reference : public entity::Entity
    {
    public:
      using entity::Entity::Entity;
      ~Reference() override = default;

      /// @brief Reference relationship for this component
      enum RefernceType
      {
        COMPONENT,
        DATA_ITEM,
        UNKNOWN
      };

      /// @brief The Entity id this component is related to
      /// @return the `idRef` property
      const entity::Value &getIdentity() const override { return getProperty("idRef"); }

      static entity::FactoryPtr getFactory();
      static entity::FactoryPtr getRoot();

      /// @brief resolve the `idRef` using the device
      /// @param device the device
      void resolve(DevicePtr device);

      /// @brief get component for a component reference
      /// @return shared pointer to the component
      auto &getComponent() const { return m_component; }
      /// @brief get data item for a data item reference
      /// @return shared pointer to the data item
      auto &getDataItem() const { return m_dataItem; }
      /// @brief get the reference type
      /// @return data item or component relationship type
      auto getReferenceType() const { return m_type; }

    protected:
      std::weak_ptr<Component> m_component;
      std::weak_ptr<data_item::DataItem> m_dataItem;
      RefernceType m_type {UNKNOWN};
    };
  }  // namespace device_model
}  // namespace mtconnect
