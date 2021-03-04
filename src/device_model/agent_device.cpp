//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "agent_device.hpp"

#include "adapter/adapter.hpp"
#include "data_item/constraints.hpp"
#include "data_item/data_item.hpp"

#include <dlib/logger.h>

using namespace std;
using namespace std::literals;

namespace mtconnect
{
  static dlib::logger g_logger("agent_device");
  using namespace entity;
  namespace device_model
  {
    entity::FactoryPtr AgentDevice::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Device::getFactory());
        factory->setFunction([](const std::string &name, Properties &ps) -> EntityPtr
        {
          return make_shared<AgentDevice>("AgentDevice"s, ps);
        });
        Component::getFactory()->registerFactory("AgentDevice", factory);
      }
      return factory;
    }

    AgentDevice::AgentDevice(const std::string &name, entity::Properties &props) : Device(name, props)
    {
      addRequiredDataItems();
      
      ErrorList errors;
      m_adapters = Component::make("Adapters", {{"id", "__adapters__"s}}, errors);
      addChild(m_adapters, errors);
    }
    
    DataItemPtr AgentDevice::getConnectionStatus(const std::string &adapter)
    {
      return getDeviceDataItem(adapter + "_connection_status");
    }
    
    void AgentDevice::addAdapter(const adapter::Adapter *adapter)
    {
      using namespace entity;
      using namespace device_model::data_item;
      auto id = adapter->getIdentity();
      
      stringstream name;
      name << adapter->getServer() << ':' << adapter->getPort();
      
      ErrorList errors;
      auto comp = Component::make("Adapter", {{"id", id}, {"name", name.str()}}, errors);
      m_adapters->addChild(comp, errors);
      
      {
        ErrorList errors;
        auto di = DataItem::make({{"type", "CONNECTION_STATUS"s},
          {"id", id + "_connection_status"s},
          {"category", "EVENT"s}},
                                 errors);
        comp->addDataItem(di, errors);
      }
      
      {
        using namespace device_model::data_item;
        ErrorList errors;
        auto di = DataItem::make(
                                 Properties {
          {"type", "ADAPTER_URI"s}, {"id", id + "_adapter_uri"}, {"category", "EVENT"s}},
                                 errors);
        di->setConstantValue(adapter->getUrl());
        comp->addDataItem(di, errors);
      }
      
      {
        ErrorList errors;
        auto di = DataItem::make({{"type", "OBSERVATION_UPDATE_RATE"s},
          {"id", id + "_observation_update_rate"s},
          {"units", "COUNT/SECOND"s},
          {"statistic", "AVERAGE"s},
          {"category", "SAMPLE"s}},
                                 errors);
        comp->addDataItem(di, errors);
      }
      
      {
        ErrorList errors;
        auto di = DataItem::make({{"type", "ASSET_UPDATE_RATE"s},
          {"id", id + "_asset_update_rate"s},
          {"units", "COUNT/SECOND"s},
          {"statistic", "AVERAGE"s},
          {"category", "SAMPLE"s}},
                                 errors);
        comp->addDataItem(di, errors);
      }
      
      {
        ErrorList errors;
        auto di = DataItem::make({{"type", "ADAPTER_SOFTWARE_VERSION"s},
          {"id", id + "_adapter_software_version"s},
          {"category", "EVENT"s}},
                                 errors);
        comp->addDataItem(di, errors);
      }
      
      {
        ErrorList errors;
        auto di = DataItem::make(
                                 {{"type", "MTCONNECT_VERSION"s}, {"id", id + "_mtconnect_version"s}, {"category", "EVENT"s}},
                                 errors);
        comp->addDataItem(di, errors);
      }
    }
    
    void AgentDevice::addRequiredDataItems()
    {
      using namespace entity;
      using namespace device_model::data_item;
      ErrorList errors;
      
      // Add the required data items
      auto di = DataItem::make(
                               {{"type", "AVAILABILITY"s}, {"id", "agent_avail"s}, {"category", "EVENT"s}}, errors);
      addDataItem(di, errors);
      
      // Add the required data items
      auto add = DataItem::make(
                                {{"type", "DEVICE_ADDED"s}, {"id", "device_added"s}, {"category", "EVENT"s}}, errors);
      addDataItem(add, errors);
      
      auto removed = DataItem::make(
                                    {{"type", "DEVICE_REMOVED"s}, {"id", "device_removed"s}, {"category", "EVENT"s}}, errors);
      addDataItem(removed, errors);
      
      auto changed = DataItem::make(
                                    {{"type", "DEVICE_CHANGED"s}, {"id", "device_changed"s}, {"category", "EVENT"s}}, errors);
      addDataItem(changed, errors);
    }
  }
}  // namespace mtconnect
