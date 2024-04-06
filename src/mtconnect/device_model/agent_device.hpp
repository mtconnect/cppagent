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

#include <map>

#include "component.hpp"
#include "data_item/data_item.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  namespace source::adapter {
    class Adapter;
    using AdapterPtr = std::shared_ptr<Adapter>;
  }  // namespace source::adapter

  namespace device_model {
    /// @brief Agent Device entity
    class AGENT_LIB_API AgentDevice : public Device
    {
    public:
      /// @brief Constructor that sets variables from an attribute map
      ///
      /// Should not be used directly, always create using the factory
      AgentDevice(const std::string &name, entity::Properties &props);
      ~AgentDevice() override = default;
      static entity::FactoryPtr getFactory();
      static entity::FactoryPtr getRoot();

      /// @brief initialize the agent device and add the device changed and removed data items
      void initialize() override
      {
        addRequiredDataItems();
        entity::ErrorList errors;
        addChild(m_adapters, errors);
        Device::initialize();
      }

      /// @brief Add an adapter and create a component to track it
      /// @param adapter the adapter
      void addAdapter(const source::adapter::AdapterPtr adapter);

      /// @brief get the connection status data item for an addapter
      /// @param adapter the adapter name
      /// @return shared pointer to the data item
      DataItemPtr getConnectionStatus(const std::string &adapter)
      {
        return getDeviceDataItem(adapter + "_connection_status");
      }
      /// @brief Get all the adapter components
      /// @return shared pointer to the adapters component
      auto &getAdapters() { return m_adapters; }

    protected:
      void addRequiredDataItems();

    protected:
      ComponentPtr m_adapters;
    };
    using AgentDevicePtr = std::shared_ptr<AgentDevice>;
  }  // namespace device_model
}  // namespace mtconnect
