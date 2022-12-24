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

#include <list>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "entity/entity.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      class DataItem;
    }
    class Component;
    class Device;
    using DevicePtr = std::shared_ptr<Device>;

    class Reference : public entity::Entity
    {
    public:
      using entity::Entity::Entity;
      ~Reference() override = default;

      enum RefernceType
      {
        COMPONENT,
        DATA_ITEM,
        UNKNOWN
      };

      const entity::Value &getIdentity() const override { return getProperty("idRef"); }

      static entity::FactoryPtr getFactory();
      static entity::FactoryPtr getRoot();

      void resolve(DevicePtr device);

      auto &getComponent() const { return m_component; }
      auto &getDataItem() const { return m_dataItem; }
      auto getReferenceType() const { return m_type; }

    protected:
      std::weak_ptr<Component> m_component;
      std::weak_ptr<data_item::DataItem> m_dataItem;
      RefernceType m_type {UNKNOWN};
    };
  }  // namespace device_model
}  // namespace mtconnect
