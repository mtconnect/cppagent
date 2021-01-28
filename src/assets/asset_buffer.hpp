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

#include "asset.hpp"
#include "entity.hpp"
#include "globals.hpp"

#include <list>
#include <map>
#include <memory>
#include <mutex>

namespace mtconnect
{
  class AssetBuffer
  {
  public:
    using Index = std::map<std::string, AssetPtr>;
    using SecondaryIndex = std::map<std::string, Index>;
    using TypeCount = std::map<std::string, int>;
    using Buffer = AssetList;
    using RemoveCount = std::map<std::string, size_t>;

    AssetBuffer(size_t max) : m_maxAssets(max) {}
    ~AssetBuffer() = default;

    auto getMaxAssets() const { return m_maxAssets; }
    auto getCount(bool active = true) const
    {
      if (active)
        return m_buffer.size() - m_removedAssets;
      else
        return m_buffer.size();
    }

    AssetPtr addAsset(AssetPtr asset);
    AssetPtr removeAsset(AssetPtr asset, const std::string time = "")
    {
      return removeAsset(asset->getAssetId(), time);
    }
    AssetPtr removeAsset(const std::string &id, const std::string time = "");

    AssetPtr getAsset(const std::string &id)
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      auto idx = m_primaryIndex.find(id);
      if (idx != m_primaryIndex.end())
        return idx->second;
      else
        return nullptr;
    }
    const std::optional<const Index *> getAssetsForDevice(const std::string &id) const
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      auto idx = m_deviceIndex.find(id);
      if (idx != m_deviceIndex.end())
        return std::make_optional(&idx->second);
      else
        return std::nullopt;
    }
    const std::optional<const Index *> getAssetsForType(const std::string &type) const
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      auto idx = m_typeIndex.find(type);
      if (idx != m_typeIndex.end())
        return std::make_optional(&idx->second);
      else
        return std::nullopt;
    }
    TypeCount getCountsByType(bool active = true) const
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      TypeCount res;
      for (const auto &t : m_typeIndex)
      {
        int delta = 0;
        if (active)
        {
          auto cit = m_typeRemoveCount.find(t.first);
          delta = cit != m_typeRemoveCount.end() ? cit->second : 0;
        }
        res[t.first] = t.second.size() - delta;
      }
      return res;
    }
    size_t getCountForType(const std::string &type, bool active = true) const
    {
      auto index = m_typeIndex.find(type);
      if (index != m_typeIndex.end())
      {
        int delta = 0;
        if (active)
        {
          auto cit = m_typeRemoveCount.find(type);
          delta = cit != m_typeRemoveCount.end() ? cit->second : 0;
        }
        return index->second.size() - delta;
      }
      else
        return 0;
    }
    size_t getCountForDevice(const std::string &device, bool active = true) const
    {
      auto index = m_deviceIndex.find(device);
      if (index != m_deviceIndex.end())
      {
        int delta = 0;
        if (active)
        {
          auto cit = m_deviceRemoveCount.find(device);
          delta = cit != m_deviceRemoveCount.end() ? cit->second : 0;
        }
        return index->second.size() - delta;
      }
      else
        return 0;
    }
    const auto &getAssets() const { return m_buffer; }
    int getIndex(const std::string &id) const
    {
      int i = 0;
      for (auto &a : m_buffer)
      {
        if (a->getAssetId() == id)
          return i;
        i++;
      }
      return -1;
    }

    size_t removeAllByType(const std::string &type)
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      auto &index = m_typeIndex[type];
      auto count = index.size();
      for (auto &asset : index)
        removeAsset(asset.first);

      return count;
    }

    size_t removeAllByDevice(const std::string &uuid)
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      auto &index = m_deviceIndex[uuid];
      auto count = index.size();
      for (auto &asset : index)
        removeAsset(asset.first);

      return count;
    }

    // For mutex locking
    auto lock() { return m_bufferLock.lock(); }
    auto unlock() { return m_bufferLock.unlock(); }
    auto try_lock() { return m_bufferLock.try_lock(); }

  protected:
    AssetPtr updateAsset(const std::string &id, Index::iterator &it, AssetPtr asset);
    void adjustCount(AssetPtr asset, int delta);

  protected:
    // Access control to the buffer
    mutable std::recursive_mutex m_bufferLock;

    size_t m_removedAssets{0};
    size_t m_maxAssets;
    Buffer m_buffer;
    Index m_primaryIndex;
    SecondaryIndex m_deviceIndex;
    SecondaryIndex m_typeIndex;
    RemoveCount m_deviceRemoveCount;
    RemoveCount m_typeRemoveCount;
  };
}  // namespace mtconnect
