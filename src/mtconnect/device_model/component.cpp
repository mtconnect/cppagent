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

#include "component.hpp"

#include <cstdlib>
#include <stdexcept>

#include "composition.hpp"
#include "configuration/configuration.hpp"
#include "data_item/data_item.hpp"
#include "description.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/device_model/device.hpp"
#include "reference.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model {
    using namespace data_item;

    // Component public methods
    Component::Component(const std::string &name, const entity::Properties &props)
      : Entity(name, props)
    {
      m_id = get<string>("id");
      m_componentName = maybeGet<string>("name");
      m_uuid = maybeGet<string>("uuid");
    }

    void Component::connectDataItems()
    {
      auto items = getList("DataItems");
      if (items)
      {
        for (auto &item : *items)
          dynamic_pointer_cast<data_item::DataItem>(item)->setComponent(getptr());
      }
    }

    void Component::connectCompositions()
    {
      auto comps = getList("Compositions");
      if (comps)
      {
        for (auto &comp : *comps)
          dynamic_pointer_cast<Composition>(comp)->setComponent(getptr());
      }
    }

    void Component::buildDeviceMaps(DevicePtr device)
    {
      device->registerComponent(getptr());
      auto items = getList("DataItems");
      if (items)
      {
        for (auto &item : *items)
        {
          auto di = dynamic_pointer_cast<data_item::DataItem>(item);
          device->registerDataItem(di);
          di->makeTopic();
        }
      }

      auto children = getChildren();
      if (children)
      {
        for (auto &child : *children)
        {
          auto cp = dynamic_pointer_cast<Component>(child);
          cp->setParent(getptr());
          cp->setDevice(device);
          cp->buildDeviceMaps(device);
        }
      }
    }

    FactoryPtr Component::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        auto dataItems = DataItem::getFactory();
        auto compositions = Composition::getFactory();
        auto configuration = configuration::Configuration::getFactory();
        auto description = Description::getFactory();
        auto references = Reference::getFactory();
        factory = make_shared<Factory>(
            Requirements {// Attributes
                          {"id", true},
                          {"name", false},
                          {"nativeName", false},
                          {"sampleRate", DOUBLE, false},
                          {"sampleInterval", DOUBLE, false},
                          {"uuid", false},
                          {"Description", ENTITY, description, false},
                          {"DataItems", ENTITY_LIST, dataItems, false},
                          {"Compositions", ENTITY_LIST, compositions, false},
                          {"References", ENTITY_LIST, references, false},
                          {"Configuration", ENTITY, configuration, false}},
            [](const std::string &name, Properties &props) -> EntityPtr {
              auto ptr = make_shared<Component>(name, props);
              ptr->initialize();
              return dynamic_pointer_cast<Entity>(ptr);
            });
        factory->setOrder(
            {"Description", "Configuration", "DataItems", "Compositions", "References"});
        auto component = make_shared<Factory>(
            Requirements {{"Component", ENTITY, factory, 1, Requirement::Infinite}});
        component->registerFactory(regex(".+"), factory);
        component->registerMatchers();
        factory->addRequirements(Requirements {{"Components", ENTITY_LIST, component, false}});
      }
      return factory;
    }

    Component::~Component() {}

    void Component::resolveReferences(DevicePtr device)
    {
      auto references = getList("References");
      if (references)
      {
        for (auto &reference : *references)
        {
          dynamic_pointer_cast<Reference>(reference)->resolve(device);
        }
      }
      auto children = getChildren();
      if (children)
      {
        for (const auto &child : *children)
          dynamic_pointer_cast<Component>(child)->resolveReferences(device);
      }
    }

    void Component::addDataItem(DataItemPtr dataItem, entity::ErrorList &errors)
    {
      if (addToList("DataItems", Component::getFactory(), dataItem, errors))
      {
        dataItem->setComponent(getptr());
        auto device = getDevice();
        if (device)
          device->registerDataItem(dataItem);
      }
    }
  }  // namespace device_model
}  // namespace mtconnect
