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

#include "logging.hpp"

#include "configuration/config_options.hpp"
#include "entity/factory.hpp"

using namespace std;

namespace mtconnect
{
  using namespace entity;
  using namespace configuration;

  namespace device_model
  {
    entity::FactoryPtr Device::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Component::getFactory());
        factory->getRequirement("name")->setMultiplicity(1, 1);
        factory->getRequirement("uuid")->setMultiplicity(1, 1);
        factory->addRequirements({{"iso841Class", false}, {"mtconnectVersion", false}});
        factory->setFunction([](const std::string &name, Properties &ps) -> EntityPtr {
          auto device = make_shared<Device>("Device"s, ps);
          device->initialize();
          return device;
        });
        Component::getFactory()->registerFactory("Device", factory);
      }
      return factory;
    }

    entity::FactoryPtr Device::getRoot()
    {
      static auto factory = make_shared<Factory>(
          Requirements {{"Device", ENTITY, getFactory(), 1, Requirement::Infinite}});

      return factory;
    }

    void Device::registerDataItem(DataItemPtr di)
    {
      m_deviceDataItemsById[di->getId()] = di;
      if (di->hasProperty("name"))
        m_deviceDataItemsByName[di->get<string>("name")] = di;
      if (di->hasProperty("Source") && di->get<EntityPtr>("Source")->hasValue())
        m_deviceDataItemsBySource[di->get<EntityPtr>("Source")->getValue<string>()] = di;
    }

    Device::Device(const std::string &name, entity::Properties &props) : Component(name, props)
    {
      BOOST_LOG_NAMED_SCOPE("device");
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
      if (auto opt = GetOption<bool>(options, PreserveUUID))
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
        BOOST_LOG_TRIVIAL(error) << "Duplicate data item id: " << dataItem->getId()
                 << " for device " << get<string>("name") << ", skipping";
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
        return sourcePos->second.lock();

      const auto namePos = m_deviceDataItemsByName.find(name);
      if (namePos != m_deviceDataItemsByName.end())
        return namePos->second.lock();

      const auto &idPos = m_deviceDataItemsById.find(name);
      if (idPos != m_deviceDataItemsById.end())
        return idPos->second.lock();

      return nullptr;
    }
  }  // namespace device_model
}  // namespace mtconnect
