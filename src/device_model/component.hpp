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
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "composition.hpp"
#include "configuration/configuration.hpp"
#include "entity/factory.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      class DataItem;
    }

    class Component;
    using ComponentPtr = std::shared_ptr<Component>;

    class Device;
    using DevicePtr = std::shared_ptr<Device>;

    using DataItemPtr = std::shared_ptr<data_item::DataItem>;
    class Component : public entity::Entity
    {
    public:
      Component(const std::string &name, const entity::Properties &props);
      static ComponentPtr make(const std::string &name, const entity::Properties &props,
                               entity::ErrorList &errors)
      {
        entity::Properties ps(props);
        auto ptr = getFactory()->make(name, ps, errors);
        return std::dynamic_pointer_cast<Component>(ptr);
      }

      static entity::FactoryPtr getFactory();
      auto getptr() const { return std::dynamic_pointer_cast<Component>(Entity::getptr()); }

      virtual void initialize()
      {
        connectCompositions();
        connectDataItems();
      }

      // Virtual destructor
      virtual ~Component();

      // Getter methods for the component ID/Name
      const auto &getId() const { return m_id; }
      const auto &getComponentName() const { return m_name; }
      const auto &getUuid() const { return m_uuid; }

      virtual const std::string getTopicName() const
      {
        if (!m_topicName)
        {
          auto *self = const_cast<Component *>(this);
          self->m_topicName.emplace(getName());
          if (m_name)
          {
            self->m_topicName->append("[").append(*m_name).append("]");
          }
        }
        return *m_topicName;
      }

      entity::EntityPtr getDescription()
      {
        if (hasProperty("Description"))
          return get<entity::EntityPtr>("Description");
        else
        {
          entity::ErrorList errors;
          auto desc = getFactory()->create("Description", {}, errors);
          if (desc)
            setProperty("Description", desc);
          return desc;
        }
      }

      void setManufacturer(const std::string &value)
      {
        auto desc = getDescription();
        desc->setProperty("manufacturer", value);
      }

      void setStation(const std::string &value)
      {
        auto desc = getDescription();
        desc->setProperty("station", value);
      }

      void setSerialNumber(const std::string &value)
      {
        auto desc = getDescription();
        desc->setProperty("serialNumber", value);
      }

      void setDescriptionValue(const std::string &value)
      {
        auto desc = getDescription();
        desc->setValue(value);
      }

      // Setter methods
      void setUuid(const std::string &uuid)
      {
        m_uuid = uuid;
        setProperty("uuid", uuid);
      }
      void setComponentName(const std::string &name)
      {
        m_name = name;
        setProperty("name", name);
      }

      // Get the device that any component is associated with
      virtual DevicePtr getDevice() const
      {
        DevicePtr device = m_device.lock();
        if (!device)
        {
          auto parent = m_parent.lock();
          if (parent)
          {
            const_cast<Component *>(this)->m_device = parent->getDevice();
            device = m_device.lock();
          }
        }
        return device;
      }

      // Set/Get the component's parent component
      ComponentPtr getParent() const { return m_parent.lock(); }

      // Add to/get the component's std::list of children
      auto getChildren() const { return getList("Components"); }
      void addChild(ComponentPtr child, entity::ErrorList &errors)
      {
        addToList("Components", Component::getFactory(), child, errors);
        child->setParent(getptr());
        auto device = getDevice();
        if (device)
          child->buildDeviceMaps(device);
      }

      // Add to/get the component's std::list of data items
      virtual void addDataItem(DataItemPtr dataItem, entity::ErrorList &errors);
      auto getDataItems() const { return getList("DataItems"); }

      bool operator<(const Component &comp) const { return m_id < comp.getId(); }
      bool operator==(const Component &comp) const { return m_id == comp.getId(); }

      // References
      void resolveReferences(DevicePtr device);

      // Connect data items
      void connectDataItems();
      void connectCompositions();
      void buildDeviceMaps(DevicePtr device);

      CompositionPtr getComposition(const std::string &id) const
      {
        auto comps = getList("Compositions");
        if (comps)
        {
          for (auto &comp : *comps)
          {
            const auto cid = comp->get<std::string>("id");
            if (cid == id)
              return std::dynamic_pointer_cast<Composition>(comp);
          }
        }

        return nullptr;
      }

    protected:
      void setParent(ComponentPtr parent) { m_parent = parent; }
      void setDevice(DevicePtr device) { m_device = device; }

    protected:
      // Unique ID for each component
      std::string m_id;

      // Name for itself
      std::optional<std::string> m_name;

      // Universal unique identifier
      std::optional<std::string> m_uuid;

      // Component relationships
      // Pointer to the parent component
      std::weak_ptr<Component> m_parent;
      std::weak_ptr<Device> m_device;

      // Topic
      std::optional<std::string> m_topicName;
    };

    struct ComponentComp
    {
      bool operator()(const Component *lhs, const Component *rhs) const { return *lhs < *rhs; }
    };
  }  // namespace device_model
}  // namespace mtconnect
