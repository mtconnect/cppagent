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

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "asset.hpp"
#include "asset_storage.hpp"
#include "utilities.hpp"

namespace mtconnect {
namespace asset {
class AssetBuffer : public AssetStorage
{
public:
  using Index = std::map<std::string, AssetPtr>;
  using SecondaryIndex = std::unordered_map<std::string, Index>;
  using Buffer = AssetList;
  using RemoveCount = std::unordered_map<std::string, size_t>;

  AssetBuffer(size_t max) : AssetStorage(max) {}
  ~AssetBuffer() = default;

  size_t getCount(bool active = true) const override
  {
    if (active)
      return m_buffer.size() - m_removedAssets;
    else
      return m_buffer.size();
  }

  AssetPtr addAsset(AssetPtr asset) override;
  AssetPtr removeAsset(const std::string &id,
                       const std::optional<Timestamp> &time = std::nullopt) override;

  AssetPtr getAsset(const std::string &id) const override
  {
    std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
    auto idx = m_primaryIndex.find(id);
    if (idx != m_primaryIndex.end())
      return idx->second;
    else
      return nullptr;
  }

  virtual size_t getAssets(AssetList &list, size_t max, const bool removed = false,
                           const std::optional<std::string> device = std::nullopt,
                           const std::optional<std::string> type = std::nullopt) const override
  {
    std::lock_guard<std::recursive_mutex> lock(m_bufferLock);

    if (device)
    {
      auto idx = m_deviceIndex.find(*device);
      if (idx != m_deviceIndex.end())
      {
        for (auto &p : idx->second)
        {
          auto &a = p.second;
          if ((removed || !a->isRemoved()) && (!type || (type && a->getType() == *type)))
          {
            list.emplace_back(a);
          }

          if (list.size() >= max)
            break;
        }
      }
      else
        return 0;
    }
    else if (type)
    {
      auto idx = m_typeIndex.find(*type);
      if (idx != m_typeIndex.end())
      {
        for (auto &p : idx->second)
        {
          auto &a = p.second;
          if (removed || !a->isRemoved())
            list.emplace_back(p.second);
          if (list.size() >= max)
            break;
        }
      }
      else
        return 0;
    }
    else
    {
      for (auto &a : reverse(m_buffer))
      {
        if (removed || !a->isRemoved())
          list.emplace_back(a);
        if (list.size() >= max)
          break;
      }
    }
    return list.size();
  }

  virtual size_t getAssets(AssetList &list, const std::list<std::string> &ids) const override
  {
    for (auto id : ids)
    {
      if (auto asset = AssetBuffer::getAsset(id); asset)
        list.emplace_back(asset);
    }

    return list.size();
  }

  TypeCount getCountsByType(bool active = true) const override
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
  size_t getCountForType(const std::string &type, bool active = true) const override
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
  size_t getCountForDevice(const std::string &device, bool active = true) const override
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

  size_t removeAll(AssetList &list, const std::optional<std::string> device = std::nullopt,
                   const std::optional<std::string> type = std::nullopt,
                   const std::optional<Timestamp> &time = std::nullopt) override
  {
    std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
    getAssets(list, std::numeric_limits<size_t>().max(), false, device, type);
    for (auto &a : list)
      removeAsset(a->getAssetId(), time);

    return list.size();
  }

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

protected:
  AssetPtr updateAsset(const std::string &id, Index::iterator &it, AssetPtr asset);
  void adjustCount(AssetPtr asset, int delta);

protected:
  size_t m_removedAssets {0};
  Buffer m_buffer;
  Index m_primaryIndex;
  SecondaryIndex m_deviceIndex;
  SecondaryIndex m_typeIndex;
  RemoveCount m_deviceRemoveCount;
  RemoveCount m_typeRemoveCount;
};
}  // namespace asset
}  // namespace mtconnect
