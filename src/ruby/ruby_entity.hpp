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

#pragma once

#include "entity/entity.hpp"
#include "device_model/device.hpp"
#include "device_model/data_item/data_item.hpp"
#include "ruby_type.hpp"

#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>


namespace mtconnect::ruby {
  using namespace mtconnect;
  using namespace device_model;
  using namespace data_item;
  using namespace entity;
  using namespace std;
  using namespace Rice;
  
  struct RubyEntity {
    void create(Rice::Module &module)
    {
      m_entity = make_unique<Class>(define_class_under<Entity>(module, "Entity"));
      c_Entity = m_entity->value();
      m_component = make_unique<Class>(define_class_under<Component, Entity>(module, "Component"));
      m_device = make_unique<Class>(define_class_under<device_model::Device, device_model::Component>(module, "Device"));
      m_dataItem = make_unique<Class>(define_class_under<data_item::DataItem, entity::Entity>(module, "DataItem"));
    }
    
    void methods()
    {
      m_entity->define_method("value", [](entity::EntityPtr entity) { return entity->getValue(); }).
        define_method("property", [](entity::EntityPtr entity, std::string name) {
          return entity->getProperty(name);
        }, Arg("name"));

      
      m_dataItem->define_method("name", [](data_item::DataItem *di) { return di->getName(); }).
        define_method("observation_name", [](data_item::DataItem *di) { return di->getObservationName().str(); }).
        define_method("id", [](data_item::DataItem *di) { return di->getId(); }).
        define_method("type", [](data_item::DataItem *di) { return di->getType(); }).
        define_method("sub_type", [](data_item::DataItem *di) { return di->getSubType(); });
      
      m_component->define_method("component_name", [](Component *c) { return c->getComponentName(); }).
        define_method("uuid", [](Component *c) { return c->getUuid(); }).
        define_method("id", [](Component *c) { return c->getId(); }).
        define_method("children", [](Component *c) {
          Rice::Array ary;
          const auto &list = c->getChildren();
          if (list)
          {
            for (auto const &s : *list)
            {
              auto cp = dynamic_cast<Component*>(s.get());
              ary.push(cp);
            }
          }
          return ary;
        }).
      define_method("data_items", [](Component *c) {
          Rice::Array ary;
          const auto &list = c->getDataItems();
          if (list)
          {
            for (auto const &s : *list)
            {
              auto di = dynamic_cast<data_item::DataItem*>(s.get());
              ary.push(di);
            }
          }
          return ary;
        });
    }
    
    std::unique_ptr<Rice::Class> m_entity;
    std::unique_ptr<Rice::Class> m_component;
    std::unique_ptr<Rice::Class> m_device;
    std::unique_ptr<Rice::Class> m_dataItem;    
  };
}
