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
#include <map>

#include "component.hpp"
#include "data_item/data_item.hpp"
#include "device_model/device.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace source::adapter {
    class Adapter;
    using AdapterPtr = std::shared_ptr<Adapter>;
  }  // namespace source::adapter

  namespace device_model {
    class AgentDevice : public Device
    {
    public:
      // Constructor that sets variables from an attribute map
      AgentDevice(const std::string &name, entity::Properties &props);
      ~AgentDevice() override = default;
      static entity::FactoryPtr getFactory();

      void initialize() override
      {
        addRequiredDataItems();
        entity::ErrorList errors;
        addChild(m_adapters, errors);
        Device::initialize();
      }

      void addAdapter(const source::adapter::AdapterPtr adapter);

      DataItemPtr getConnectionStatus(const std::string &adapter)
      {
        return getDeviceDataItem(adapter + "_connection_status");
      }
      auto &getAdapters() { return m_adapters; }

    protected:
      void addRequiredDataItems();

    protected:
      ComponentPtr m_adapters;
    };
    using AgentDevicePtr = std::shared_ptr<AgentDevice>;
  }  // namespace device_model
}  // namespace mtconnect
