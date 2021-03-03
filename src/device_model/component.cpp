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

#include "component.hpp"

#include "agent.hpp"
#include "composition.hpp"
#include "data_item/data_item.hpp"
#include "device_model/device.hpp"
#include "configuration/configuration.hpp"
#include "description.hpp"

#include <cstdlib>
#include <stdexcept>

using namespace std;

namespace mtconnect
{
  using namespace entity;
  namespace device_model
  {
    using namespace data_item;

    // Component public methods
    Component::Component(const std::string &name, const entity::Properties &props)
    : Entity(name, props)
    {
      m_id = get<string>("id");
      m_name = maybeGet<string>("name");
      m_uuid = maybeGet<string>("uuid");
      
      auto items = getList("DataItems");
      if (items)
      {
        for (auto &item : *items)
          dynamic_pointer_cast<DataItem>(item)->setComponent(getptr());
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
        factory = make_shared<Factory>(Requirements {
                                        // Attributes
                                        {"id", true},
          {"name", false},
          {"nativeName", false},
          {"sampleRate", DOUBLE, false},
                                        {"uuid", false},
          {"Description", ENTITY, description, false},
          {"DataItems", ENTITY_LIST, dataItems, false},
          {"Compositions", ENTITY_LIST, compositions, false},
          {"Configuration", ENTITY, configuration, false}
        },
                                       [](const std::string &name, Properties &props) -> EntityPtr {
          auto ptr = make_shared<Component>(name, props);
          return dynamic_pointer_cast<Entity>(ptr);
        });
        factory->setOrder({"Description", "Configuration", "DataItems", "Compositions"});
        auto component = make_shared<Factory>(
            Requirements {{"Component", ENTITY, factory, 1, Requirement::Infinite}});
        component->registerFactory(regex(".+"), factory);
        factory->addRequirements(Requirements{{"Components", ENTITY_LIST, component, false}});

      }
      return factory;
    }
    
    Component::~Component()
    {
    }
    
    void Component::resolveReferences()
    {
      DevicePtr device = getDevice();
#if 0
      for (auto &reference : m_references)
      {
        if (reference.m_type == Component::Reference::DATA_ITEM)
        {
          auto di = device->getDeviceDataItem(reference.m_id);
          if (!di)
            throw runtime_error("Cannot resolve Reference for component " + m_name +
                                " to data item " + reference.m_id);
          reference.m_dataItem = di;
        }
        else if (reference.m_type == Component::Reference::COMPONENT)
        {
          auto comp = device->getComponentById(reference.m_id);
          if (!comp)
            throw runtime_error("Cannot resolve Reference for component " + m_name +
                                " to component " + reference.m_id);
          
          reference.m_component = comp;
        }
      }
      
      for (const auto &childComponent : m_children)
        childComponent->resolveReferences();
#endif
    }
        
    void Component::addDataItem(DataItemPtr dataItem, entity::ErrorList &errors)
    {
      addToList<data_item::DataItem>("DataItems", dataItem, errors);
      dataItem->setComponent(getptr());
    }
  }
}  // namespace mtconnect
