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
#include "utilities.hpp"

namespace mtconnect {
  namespace mic = boost::multi_index;

  namespace source::adapter {
    class Adapter;
  }

  namespace device_model {
    class Device : public Component
    {
    public:
      struct ByName
      {};
      struct ById
      {};
      struct BySource
      {};

      struct ExtractId
      {
        using result_type = std::string;
        const result_type &operator()(const WeakDataItemPtr d) const { return d.lock()->getId(); }
      };

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

      // Mapping of device names to data items
      using DataItemIndex = mic::multi_index_container<
          WeakDataItemPtr, mic::indexed_by<mic::hashed_unique<mic::tag<ById>, ExtractId>,
                                           mic::hashed_unique<mic::tag<BySource>, ExtractName>,
                                           mic::hashed_unique<mic::tag<ByName>, ExtractSource>>>;

      // Constructor that sets variables from an attribute map
      Device(const std::string &name, entity::Properties &props);
      ~Device() override = default;

      auto getptr() const { return std::dynamic_pointer_cast<Device>(Entity::getptr()); }

      void initialize() override
      {
        m_dataItems.clear();
        m_componentsById.clear();

        Component::initialize();
        buildDeviceMaps(getptr());
        resolveReferences(getptr());
      }

      static entity::FactoryPtr getFactory();
      static entity::FactoryPtr getRoot();

      void setOptions(const ConfigOptions &options);

      // Add/get items to/from the device name to data item mapping
      void addDeviceDataItem(DataItemPtr dataItem);
      DataItemPtr getDeviceDataItem(const std::string &name) const;

      void addAdapter(source::adapter::Adapter *anAdapter) { m_adapters.emplace_back(anAdapter); }
      ComponentPtr getComponentById(const std::string &aId) const
      {
        auto comp = m_componentsById.find(aId);
        if (comp != m_componentsById.end())
          return comp->second.lock();
        else
          return nullptr;
      }
      void addComponent(ComponentPtr aComponent)
      {
        m_componentsById.insert(make_pair(aComponent->getId(), aComponent));
      }

      DevicePtr getDevice() const override { return getptr(); }

      // Return the mapping of Device to data items
      const auto &getDeviceDataItems() const { return m_dataItems.get<ById>(); }

      void addDataItem(DataItemPtr dataItem, entity::ErrorList &errors) override;

      std::vector<source::adapter::Adapter *> m_adapters;

      auto getMTConnectVersion() const { maybeGet<std::string>("mtconnectVersion"); }

      // Cached data items
      DataItemPtr getAvailability() const { return m_availability; }
      DataItemPtr getAssetChanged() const { return m_assetChanged; }
      DataItemPtr getAssetRemoved() const { return m_assetRemoved; }
      DataItemPtr getAssetCount() const { return m_assetCount; }

      void setPreserveUuid(bool v) { m_preserveUuid = v; }
      bool preserveUuid() const { return m_preserveUuid; }

      void registerDataItem(DataItemPtr di);
      void registerComponent(ComponentPtr c) { m_componentsById[c->getId()] = c; }

      const std::string getTopicName() const override { return *m_uuid; }

    protected:
      void cachePointers(DataItemPtr dataItem);

    protected:
      bool m_preserveUuid {false};

      DataItemPtr m_availability;
      DataItemPtr m_assetChanged;
      DataItemPtr m_assetRemoved;
      DataItemPtr m_assetCount;

      DataItemIndex m_dataItems;
      std::unordered_map<std::string, std::weak_ptr<Component>> m_componentsById;
    };

    using DevicePtr = std::shared_ptr<Device>;
  }  // namespace device_model
  using DevicePtr = std::shared_ptr<device_model::Device>;
}  // namespace mtconnect
