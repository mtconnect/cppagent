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

#include "device.hpp"
#include "entity/factory.hpp"

#include "config_options.hpp"

#include <dlib/logger.h>

using namespace std;

namespace mtconnect
{
  using namespace entity;
  
  static dlib::logger g_logger("device");
  
  namespace device_model
  {
    entity::FactoryPtr Device::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = Component::getFactory()->deepCopy();
        factory->getRequirement("name")->setMultiplicity(1, 1);
        factory->getRequirement("uuid")->setMultiplicity(1, 1);
        factory->addRequirements({{"iso841Class", false}});
        factory->setFunction([](const std::string &name, Properties &ps) -> EntityPtr
        {
          return make_shared<Device>("Device"s, ps);
        });
        Component::getFactory()->registerFactory("Device", factory);
      }
      return factory;
    }
  
    Device::Device(const std::string &name, entity::Properties &props)
    : Component(name, props)
    {
      auto items = getList("DataItems");
      if (items)
      {
        for (auto &item : *items)
        {
          auto di = dynamic_pointer_cast<data_item::DataItem>(item);
          cachePointers(di);
        }
      }
    }
    
    void Device::setOptions(const ConfigOptions &options)
    {
      if (auto opt = GetOption<bool>(options, configuration::PreserveUUID))
        m_preserveUuid = *opt;
    }
    
    // TODO: Clean up these initialization methods for data items
    void Device::addDeviceDataItem(DataItemPtr dataItem)
    {
      if (dataItem->hasProperty("Source") && dataItem->getSource()->hasValue())
        m_deviceDataItemsBySource[dataItem->getSource()->getValue<string>()] = dataItem;
      
      if (dataItem->getName())
        m_deviceDataItemsByName[*dataItem->getName()] = dataItem;
      
      if (m_deviceDataItemsById.find(dataItem->getId()) != m_deviceDataItemsById.end())
      {
        g_logger << dlib::LERROR << "Duplicate data item id: " << dataItem->getId() << " for device "
        << get<string>("name") << ", skipping";
      }
      else
        m_deviceDataItemsById[dataItem->getId()] = dataItem;
    }
    
    void Device::addDataItem(DataItemPtr dataItem, entity::ErrorList &errors)
    {
      Component::addDataItem(dataItem, errors);
      cachePointers(dataItem);
    }
    
    void Device::cachePointers(DataItemPtr dataItem)
    {
      if (dataItem->getType() == "AVAILABILITY")
        m_availability = dataItem;
      else if (dataItem->getType() == "ASSET_CHANGED")
        m_assetChanged = dataItem;
      else if (dataItem->getType() == "ASSET_REMOVED")
        m_assetRemoved = dataItem;
    }
    
    DataItemPtr Device::getDeviceDataItem(const std::string &name) const
    {
      const auto sourcePos = m_deviceDataItemsBySource.find(name);
      if (sourcePos != m_deviceDataItemsBySource.end())
        return sourcePos->second;
      
      const auto namePos = m_deviceDataItemsByName.find(name);
      if (namePos != m_deviceDataItemsByName.end())
        return namePos->second;
      
      const auto &idPos = m_deviceDataItemsById.find(name);
      if (idPos != m_deviceDataItemsById.end())
        return idPos->second;
      
      return nullptr;
    }
  }
}  // namespace mtconnect
