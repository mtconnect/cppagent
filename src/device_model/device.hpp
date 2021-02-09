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
#include "component.hpp"
#include "data_item.hpp"
#include "utilities.hpp"
#include <unordered_map>

#include <map>

namespace mtconnect
{
  class Component;
  namespace adapter
  {
    class Adapter;
  }

  class Device : public Component
  {
  public:
    // Constructor that sets variables from an attribute map
    Device(const Attributes &attributes, const std::string block = "Device");
    ~Device() override;
    
    void setOptions(const ConfigOptions &options);

    // Add/get items to/from the device name to data item mapping
    void addDeviceDataItem(DataItem *dataItem);
    DataItem *getDeviceDataItem(const std::string &name) const;
    void addAdapter(adapter::Adapter *anAdapter) { m_adapters.emplace_back(anAdapter); }
    Component *getComponentById(const std::string &aId) const
    {
      auto comp = m_componentsById.find(aId);
      if (comp != m_componentsById.end())
        return comp->second;
      else
        return nullptr;
    }
    void addComponent(Component *aComponent)
    {
      m_componentsById.insert(make_pair(aComponent->getId(), aComponent));
    }

    // Return the mapping of Device to data items
    const std::unordered_map<std::string, DataItem *> &getDeviceDataItems() const
    {
      return m_deviceDataItemsById;
    }

    void addDataItem(DataItem *dataItem) override;

    std::vector<adapter::Adapter *> m_adapters;
    bool m_preserveUuid;
    bool m_availabilityAdded;

    const std::string &getMTConnectVersion() const { return m_mtconnectVersion; }

    // Cached data items
    DataItem *getAvailability() const { return m_availability; }
    DataItem *getAssetChanged() const { return m_assetChanged; }
    DataItem *getAssetRemoved() const { return m_assetRemoved; }

  protected:
    // The iso841Class of the device
    unsigned int m_iso841Class;
    std::string m_mtconnectVersion;

    DataItem *m_availability;
    DataItem *m_assetChanged;
    DataItem *m_assetRemoved;

    // Mapping of device names to data items
    std::unordered_map<std::string, DataItem *> m_deviceDataItemsByName;
    std::unordered_map<std::string, DataItem *> m_deviceDataItemsById;
    std::unordered_map<std::string, DataItem *> m_deviceDataItemsBySource;
    std::unordered_map<std::string, Component *> m_componentsById;
  };
}  // namespace mtconnect
