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

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <map>
#include <unordered_map>

#include "component.hpp"
#include "data_item/data_item.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  namespace mic = boost::multi_index;

  namespace source::adapter {
    class Adapter;
  }

  /// @brief Namespace for all Device Model Related entities
  namespace device_model {
    /// @brief Component entity representing a `Device`
    class AGENT_LIB_API Device : public Component
    {
    public:
      /// @brief multi-index tag: Data items indexed by name
      struct ByName
      {};
      /// @brief multi-index tag: Data items indexed by id
      struct ById
      {};
      /// @brief multi-index tag: Data items indexed by optional original id
      struct ByOriginalId
      {};
      /// @brief multi-index tag: Data items index by Source
      struct BySource
      {};
      /// @brief multi-index tag: Data items indexed by type
      struct ByType
      {};

      /// @brief multi-index data item id extractor
      struct ExtractId
      {
        using result_type = std::string;
        const result_type &operator()(const WeakDataItemPtr d) const { return d.lock()->getId(); }
      };
      /// @brief multi-index data item id extractor
      ///
      /// falls back to id if orginal id is not given

      struct ExtractOriginalId
      {
        using result_type = std::string;
        const result_type operator()(const WeakDataItemPtr &d) const
        {
          const static result_type none {};
          if (d.expired())
            return none;
          else
          {
            auto di = d.lock();
            auto id = di->getOriginalId();
            if (id)
              return *id;
            else
              return di->getId();
          }
        }
      };
      /// @brief multi-index data item name extractor
      ///
      /// falls back to id if name is not given
      struct ExtractName
      {
        using result_type = std::string;
        const result_type operator()(const WeakDataItemPtr &d) const
        {
          const static result_type none {};
          if (d.expired())
            return none;
          else
          {
            auto di = d.lock();
            auto name = di->getName();
            if (name)
              return *name;
            else
              return di->getId();
          }
        }
      };
      /// @brief multi-index data item source extractor
      ///
      /// falls back to id if name is not given
      struct ExtractSource
      {
        using result_type = std::string;
        const result_type &operator()(const WeakDataItemPtr &d) const
        {
          const static result_type none {};
          if (d.expired())
            return none;
          else
          {
            auto di = d.lock();
            if (di->hasProperty("Source") && di->getSource()->hasValue())
              return di->getSource()->getValue<std::string>();
            else
              return di->getId();
          }
        }
      };
      /// @brief multi-index data item type extractor
      struct ExtractType
      {
        using result_type = std::string;
        const result_type &operator()(const WeakDataItemPtr &d) const
        {
          const static result_type none {};
          if (d.expired())
            return none;
          else
          {
            auto di = d.lock();
            return di->getType();
          }
        }
      };

      /// @brief Mapping of device names to data items
      using DataItemIndex = mic::multi_index_container<
          WeakDataItemPtr,
          mic::indexed_by<mic::hashed_unique<mic::tag<ById>, ExtractId>,
                          mic::hashed_unique<mic::tag<ByOriginalId>, ExtractOriginalId>,
                          mic::hashed_non_unique<mic::tag<BySource>, ExtractSource>,
                          mic::hashed_non_unique<mic::tag<ByName>, ExtractName>,
                          mic::ordered_non_unique<mic::tag<ByType>, ExtractType>>>;

      struct ExtractComponentId
      {
        using result_type = std::string;
        const result_type &operator()(const std::weak_ptr<Component> &c) const
        {
          return c.lock()->getId();
        }
      };
      struct ExtractComponentType
      {
        using result_type = std::string;
        const result_type &operator()(const std::weak_ptr<Component> &c) const
        {
          return c.lock()->getName();
        }
      };
      struct ExtractComponentName
      {
        using result_type = std::string;
        const result_type operator()(const std::weak_ptr<Component> &c) const
        {
          auto comp = c.lock();
          if (comp->hasProperty("name"))
            return comp->get<std::string>("name");
          else
            return comp->getId();
        }
      };

      using ComponentIndex = mic::multi_index_container<
          std::weak_ptr<Component>,
          mic::indexed_by<mic::hashed_unique<mic::tag<ById>, ExtractComponentId>,
                          mic::hashed_non_unique<mic::tag<ByType>, ExtractComponentType>,
                          mic::hashed_non_unique<mic::tag<ByName>, ExtractComponentName>>>;

      /// @brief Constructor that sets variables from an attribute map
      /// @param[in] name the name of the device
      /// @param[in] props the device properties
      Device(const std::string &name, entity::Properties &props);
      ~Device() override = default;

      /// @brief get a shared pointer to the device
      auto getptr() const { return std::dynamic_pointer_cast<Device>(Entity::getptr()); }

      /// @brief Indexes all the entities in this device
      void initialize() override
      {
        m_dataItems.clear();
        m_componentIndex.clear();

        Component::initialize();
        buildDeviceMaps(getptr());
        resolveReferences(getptr());
      }

      static entity::FactoryPtr getFactory();
      static entity::FactoryPtr getRoot();

      /// @brief set any configuration options related to this device
      /// @param[in] options the options
      /// - `PreserveUUID` can be set to lock the uuid of this device
      void setOptions(const ConfigOptions &options);

      /// @brief Add a data item to the device
      /// @param[in] dataItem shared pointer to the data item
      void addDeviceDataItem(DataItemPtr dataItem);
      /// @brief get a data item by multiple indexes
      ///
      /// Looks for a data item using the following idexes in the following order:
      ///  1. id
      ///  2. original id (set when creating unique ids)
      ///  3. name
      ///  4. source
      /// @param[in] name the source, name, or id of the data item
      /// @return shared pointer to the data item if found
      DataItemPtr getDeviceDataItem(const std::string &name) const;

      /// @brief associate an adapter with the device
      /// @param[in] anAdapter an adapter
      void addAdapter(source::adapter::Adapter *anAdapter) { m_adapters.emplace_back(anAdapter); }
      /// @brief get a component by id
      /// @param[in] aId the component id
      /// @return shared pointer to the component if found
      ComponentPtr getComponentById(const std::string &aId) const
      {
        auto comp = m_componentIndex.get<ById>().find(aId);
        if (comp != m_componentIndex.get<ById>().end())
          return comp->lock();
        else
          return nullptr;
      }
      /// @brief get a component by name
      /// @param[in] name the component name
      /// @return shared pointer to the component if found
      ComponentPtr getComponentByName(const std::string &name) const
      {
        auto comp = m_componentIndex.get<ByName>().find(name);
        if (comp != m_componentIndex.get<ByName>().end())
          return comp->lock();
        else
          return nullptr;
      }
      /// @brief get a component by name
      /// @param[in] name the component name
      /// @return shared pointer to the component if found
      std::list<ComponentPtr> getComponentByType(const std::string &type) const
      {
        std::list<ComponentPtr> res;
        auto [first, last] = m_componentIndex.get<ByType>().equal_range(type);
        if (first != m_componentIndex.get<ByType>().end())
        {
          for (; first != last; first++)
            res.push_back(first->lock());
        }

        return res;
      }

      /// @brief Add a component to the device
      /// @param[in] component the component
      void addComponent(ComponentPtr component)
      {
        auto [id, added] = m_componentIndex.emplace(component);
        if (!added)
        {
          LOG(error) << "Duplicate component id: " << component->getId() << " for device "
                     << get<std::string>("name") << ", skipping";
        }
      }
      /// @brief get device returns itself
      /// @return shared pointer to this entity
      DevicePtr getDevice() const override { return getptr(); }

      /// @brief get the data item index by id
      /// @return data item index by id
      const auto &getDeviceDataItems() const { return m_dataItems.get<ById>(); }
      /// @brief get the multi-index for data items
      /// @return data item multi-index
      const auto &getDataItemIndex() const { return m_dataItems; }

      /// @brief
      /// @param[in] dataItem
      /// @param[in,out] errors
      void addDataItem(DataItemPtr dataItem, entity::ErrorList &errors) override;

      /// @brief get the version of this device
      /// @return mtconnet version
      auto getMTConnectVersion() const { maybeGet<std::string>("mtconnectVersion"); }

      /// @name Cached data items
      ///@{
      DataItemPtr getAvailability() const { return m_availability; }
      DataItemPtr getAssetChanged() const { return m_assetChanged; }
      DataItemPtr getAssetRemoved() const { return m_assetRemoved; }
      DataItemPtr getAssetCount() const { return m_assetCount; }
      ///@}

      /// @brief set the state of the preserve uuid flag
      /// @param v preserve uuid state
      void setPreserveUuid(bool v) { m_preserveUuid = v; }
      /// @brief get the preserve uuid state
      /// @return `true` if uuids are preserved
      bool preserveUuid() const { return m_preserveUuid; }

      /// @brief associate a data item with this device
      /// @param di the data item
      void registerDataItem(DataItemPtr di);
      /// @brief associate a component with the device
      /// @param c the component
      void registerComponent(ComponentPtr c) { addComponent(c); }

      /// @brief get the topic for this device
      /// @return the uuid of the device
      const std::string getTopicName() const override { return *m_uuid; }

      /// @brief Converts all the ids to unique ids by hasing the topics
      ///
      /// Converts the id attribute to a unique value and caches the original value
      /// in case it is required later
      void createUniqueIds(std::unordered_map<std::string, std::string> &idMap);

    protected:
      void cachePointers(DataItemPtr dataItem);

    protected:
      bool m_preserveUuid {false};

      DataItemPtr m_availability;
      DataItemPtr m_assetChanged;
      DataItemPtr m_assetRemoved;
      DataItemPtr m_assetCount;

      DataItemIndex m_dataItems;
      ComponentIndex m_componentIndex;
      std::vector<source::adapter::Adapter *> m_adapters;
    };

    using DevicePtr = std::shared_ptr<Device>;
  }  // namespace device_model
  using DevicePtr = std::shared_ptr<device_model::Device>;
}  // namespace mtconnect
