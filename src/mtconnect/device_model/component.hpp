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

#include <list>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "composition.hpp"
#include "configuration/configuration.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/entity/factory.hpp"
#include "mtconnect/utilities.hpp"

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
    using WeakDataItemPtr = std::weak_ptr<data_item::DataItem>;

    /// @brief MTConnect Component Entity
    class AGENT_LIB_API Component : public entity::Entity
    {
    public:
      /// @brief Create a component with a type and properties
      /// @param[in] name the name of the component (o type)
      /// @param[in] props properties of the component
      Component(const std::string &name, const entity::Properties &props);
      static ComponentPtr make(const std::string &name, const entity::Properties &props,
                               entity::ErrorList &errors)
      {
        entity::Properties ps(props);
        auto ptr = getFactory()->make(name, ps, errors);
        return std::dynamic_pointer_cast<Component>(ptr);
      }

      static entity::FactoryPtr getFactory();

      /// @brief get the shared pointer for the component
      /// @return shared pointer
      auto getptr() const { return std::dynamic_pointer_cast<Component>(Entity::getptr()); }

      /// @brief associate compositions and data items with the component
      virtual void initialize()
      {
        connectCompositions();
        connectDataItems();
      }

      virtual ~Component();

      /// @brief get the component id
      /// @return component id
      const auto &getId() const { return m_id; }
      /// @brief get the name property of the component
      /// @return name if it exists
      const auto &getComponentName() const { return m_componentName; }
      /// @brief get the component uuid
      /// @return uuid if it exists
      const auto &getUuid() const { return m_uuid; }

      /// @brief Get the topic name for the component
      /// @return topic name
      virtual const std::string getTopicName() const
      {
        if (!m_topicName)
        {
          auto *self = const_cast<Component *>(this);
          self->m_topicName.emplace(getName());
          if (m_componentName)
          {
            self->m_topicName->append("[").append(*m_componentName).append("]");
          }
        }
        return *m_topicName;
      }

      /// @brief get the description entity
      /// @return shared pointer description
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

      /// @brief set the manufacturer in the description
      /// @param value the manufacturer
      void setManufacturer(const std::string &value)
      {
        auto desc = getDescription();
        desc->setProperty("manufacturer", value);
      }

      /// @brief set the station in the description
      /// @param value the description
      void setStation(const std::string &value)
      {
        auto desc = getDescription();
        desc->setProperty("station", value);
      }

      /// @brief set the serial number in the description
      /// @param value the serial number
      void setSerialNumber(const std::string &value)
      {
        auto desc = getDescription();
        desc->setProperty("serialNumber", value);
      }

      /// @brief set the description value in the description
      /// @param value the description value
      void setDescriptionValue(const std::string &value)
      {
        auto desc = getDescription();
        desc->setValue(value);
      }

      /// @brief set the uuid
      /// @param uuid the uuid
      void setUuid(const std::string &uuid)
      {
        m_uuid = uuid;
        setProperty("uuid", uuid);
      }
      /// @brief set the compoent name property, not the compoent type
      /// @param name name property
      void setComponentName(const std::string &name)
      {
        m_componentName = name;
        setProperty("name", name);
      }
      /// @brief set the compoent name property, not the compoent type
      /// @param name name property
      void setComponentName(const std::optional<std::string> &name)
      {
        m_componentName = name;
        if (name)
          setProperty("name", *name);
        else
          m_properties.erase("name");
      }

      /// @brief get the device (top level component)
      /// @return shared pointer to the device
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

      /// @brief get the parent component
      /// @return shared pointer to the parent component
      ComponentPtr getParent() const { return m_parent.lock(); }

      /// @brief get the children of this component
      /// @return list of component pointers
      auto getChildren() const { return getList("Components"); }
      /// @brief add a child to this component
      /// @param[in] child the child component
      /// @param[in,out] errors errors that occurred when adding the child
      void addChild(ComponentPtr child, entity::ErrorList &errors)
      {
        addToList("Components", Component::getFactory(), child, errors);
        child->setParent(getptr());
        auto device = getDevice();
        if (device)
          child->buildDeviceMaps(device);
      }

      /// @brief add a data item to the component
      /// @param[in] dataItem the data item
      /// @param[in,out] errors errors that occurred when adding the data item
      virtual void addDataItem(DataItemPtr dataItem, entity::ErrorList &errors);
      /// @brief get the list of data items
      /// @return the data item list
      auto getDataItems() const { return getList("DataItems"); }

      bool operator<(const Component &comp) const { return m_id < comp.getId(); }
      bool operator==(const Component &comp) const { return m_id == comp.getId(); }

      /// @brief connected references by looking them up in the device
      /// @param device the device to use as an index
      void resolveReferences(DevicePtr device);

      /// @brief connect data items to this component
      void connectDataItems();
      /// @brief connect compositions to this component
      void connectCompositions();
      /// @brief index components to data items in the device
      ///
      /// Also builds the topics.
      ///
      /// @param device the device
      void buildDeviceMaps(DevicePtr device);

      /// @brief get the composition by its id
      /// @param id the composition id
      /// @return shared pointer to the composition
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

      /// @brief Get the component topic path as a list
      ///
      /// Recurses to root and then appends getTopicName
      /// @param[in,out] pth the path list to append to
      void path(std::list<std::string> &pth)
      {
        auto p = getParent();
        if (p)
          p->path(pth);

        pth.push_back(getTopicName());
      }

      std::optional<std::string> createUniqueId(std::unordered_map<std::string, std::string> &idMap,
                                                const boost::uuids::detail::sha1 &sha1) override
      {
        auto newId = Entity::createUniqueId(idMap, sha1);
        m_id = *newId;
        return newId;
      }

    protected:
      void setParent(ComponentPtr parent) { m_parent = parent; }
      void setDevice(DevicePtr device) { m_device = device; }

    protected:
      std::string m_id;                            //< Unique ID for each component
      std::optional<std::string> m_componentName;  //< Name property for the component
      std::optional<std::string> m_uuid;           //< Universal unique identifier

      // Component relationships

      std::weak_ptr<Component> m_parent;       //< Pointer to the parent component
      std::weak_ptr<Device> m_device;          //< Pointer to the device related to the component
      std::optional<std::string> m_topicName;  //< The cached topic name
    };

    /// @brief Comparison lambda to sort components
    struct ComponentComp
    {
      bool operator()(const Component *lhs, const Component *rhs) const { return *lhs < *rhs; }
    };
  }  // namespace device_model
}  // namespace mtconnect
