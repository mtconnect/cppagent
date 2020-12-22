//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "globals.hpp"
#include "entity.hpp"

#include <map>
#include <memory>
#include <list>
#include <mutex>

namespace mtconnect
{
  class AssetEntity : public entity::Entity
  {
  public:
    const std::string &getType() const
    {
      return getName();
    }
    const std::string &getAssetId() const
    {
      if (m_assetId.empty())
      {
        const auto &v = getProperty("assetId");
        if (std::holds_alternative<std::string>(v))
          *const_cast<std::string*>(&m_assetId) = std::get<std::string>(v);
        else
          throw entity::PropertyError("Asset has no assetId");
      }
      return m_assetId;
    }

    const std::optional<std::string> getDeviceUuid() const
    {
      const auto &v = getProperty("deviceUuid");
      if (std::holds_alternative<std::string>(v))
        return std::get<std::string>(v);
      else
        return std::nullopt;
    }
    const std::optional<std::string> getTimestamp() const
    {
      const auto &v = getProperty("timestamp");
      if (std::holds_alternative<std::string>(v))
        return std::get<std::string>(v);
      else
        return std::nullopt;
    }
    bool isRemoved() const
    {
      const auto &v = getProperty("removed");
      if (std::holds_alternative<std::string>(v))
        return std::get<std::string>(v) == "true";
      else
        return false;
    }
    
    bool operator==(const AssetEntity &another) const { return getAssetId() == another.getAssetId(); }
    
  protected:
    std::string m_assetId;

  };
  
  using AssetEntityPtr = std::shared_ptr<AssetEntity>;
  
  class AssetBuffer
  {
  public:
    using Index = std::map<std::string, AssetEntityPtr>;
    using SecondaryIndex = std::map<std::string, Index>;
    using TypeCount = std::map<std::string, size_t>;

    AssetBuffer(size_t max)
    : m_maxAssets(max)
    {
    }
    ~AssetBuffer() = default;
    
    AssetEntityPtr addAsset(AssetEntityPtr asset);
    AssetEntityPtr removeAsset(AssetEntityPtr asset);
    AssetEntityPtr removeAsset(const std::string &id);

    AssetEntityPtr getAsset(const std::string &id)
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      auto idx = m_primaryIndex.find(id);
      if (idx != m_primaryIndex.end())
        return idx->second;
      else
        return nullptr;
    }
    const Index getAssetsForDevice(const std::string &id) const
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      auto idx = m_deviceIndex.find(id);
      if (idx != m_deviceIndex.end())
        return idx->second;
      else
        return Index{};
    }
    const Index getAssetsForType(const std::string &type) const
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      auto idx = m_typeIndex.find(type);
      if (idx != m_typeIndex.end())
        return idx->second;
      else
        return Index{};
    }
    TypeCount getCountsByType() const
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      TypeCount res;
      for (const auto &t : m_typeIndex)
        res[t.first] = t.second.size();
      return res;
    }
        
    // For mutex locking
    void lock() { m_bufferLock.lock(); }
    void unlock() { m_bufferLock.unlock(); }
    void try_lock() { m_bufferLock.try_lock(); }

  protected:
    using Buffer = std::list<AssetEntityPtr>;
    
    // Access control to the buffer
    mutable std::recursive_mutex m_bufferLock;
    
    size_t m_maxAssets;
    Buffer m_buffer;
    Index m_primaryIndex;
    SecondaryIndex m_deviceIndex;
    SecondaryIndex m_typeIndex;
  };
}

