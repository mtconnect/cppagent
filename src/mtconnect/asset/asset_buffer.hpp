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

#include <boost/lambda/lambda.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/any_range.hpp>
#include <boost/range/functions.hpp>
#include <boost/range/metafunctions.hpp>

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "asset.hpp"
#include "asset_storage.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::asset {
  namespace mic = boost::multi_index;

  class AGENT_LIB_API AssetBuffer : public AssetStorage
  {
  public:
    /// @brief Structure to store asset for boost multi index container
    struct AssetNode
    {
      AssetNode(AssetPtr &asset) : m_asset(asset), m_identity(asset->getAssetId()) {}
      ~AssetNode() = default;

      using element_type = AssetPtr;

      const std::string &getAssetId() const { return m_identity; }
      const std::string &getType() const { return m_asset->getType(); }
      const std::string &getDeviceUuid() const
      {
        static const std::string unknown {"UNKNOWN"};
        const auto &dev = m_asset->getProperty("deviceUuid");
        if (std::holds_alternative<std::string>(dev))
          return std::get<std::string>(dev);
        else
          return unknown;
      }
      bool isRemoved() const { return m_asset->isRemoved(); }

      bool operator<(const AssetNode &o) const { return m_identity < o.m_identity; }

      AssetPtr operator*() const { return m_asset; }

      AssetPtr m_asset;
      std::string m_identity;
    };

  public:
    using AssetId = std::string;
    using AssetType = std::string;
    using DeviceUuid = std::string;
    using RemoveCountByType = std::unordered_map<AssetType, size_t>;
    using RemoveCountByDeviceAndType = std::unordered_map<DeviceUuid, RemoveCountByType>;

    /// @brief Index by first in/first out sequence
    struct ByFifo
    {};
    /// @brief Index by assetId
    struct ByAssetId
    {};
    /// @brief Index by Device and Type
    struct ByDeviceAndType
    {};
    /// @brief Index by type
    struct ByType
    {};

    /// @brief The Multi-Index Container type
    using AssetIndex = mic::multi_index_container<
        AssetNode,
        mic::indexed_by<
            mic::sequenced<mic::tag<ByFifo>>,
            mic::hashed_unique<mic::tag<ByAssetId>, mic::key<&AssetNode::m_identity>>,
            mic::ordered_non_unique<mic::tag<ByDeviceAndType>,
                                    mic::key<&AssetNode::getDeviceUuid, &AssetNode::getType>>,
            mic::hashed_non_unique<mic::tag<ByType>, mic::key<&AssetNode::getType>>>>;

    /// @brief Create an asset buffer with a maximum size
    /// @param max the maximum size
    AssetBuffer(size_t max) : AssetStorage(max) {}
    ~AssetBuffer() = default;

    size_t getCount(bool active = true) const override
    {
      if (active)
        return m_index.size() - m_removedAssets;
      else
        return m_index.size();
    }

    AssetPtr addAsset(AssetPtr asset) override
    {
      AssetPtr old {};
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);

      if (!asset->getTimestamp())
      {
        asset->setProperty("timestamp", getCurrentTime(GMT_UV_SEC));
      }

      if (!asset->hasProperty("assetId"))
      {
        throw entity::PropertyError("Asset does not have an asset id");
      }

      auto added = m_index.emplace_front(asset);

      // Is duplicate
      if (!added.second)
      {
        old = added.first->m_asset;
        m_index.modify(added.first, [&asset](AssetNode &n) { n.m_asset = asset; });
        m_index.relocate(m_index.begin(), added.first);
        if (asset->isRemoved() && !old->isRemoved())
          adjustCount(asset, 1);
        else if (!asset->isRemoved() && old->isRemoved())
          adjustCount(old, -1);
      }
      else if (m_index.size() > m_maxAssets)
      {
        // Remove old asset from the end
        old = m_index.back().m_asset;
        m_index.pop_back();
        if (old->isRemoved())
        {
          adjustCount(old, -1);
        }
      }

      return old;
    }

    AssetPtr removeAsset(const std::string &id,
                         const std::optional<Timestamp> &time = std::nullopt) override
    {
      AssetPtr asset {};
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);

      auto &idx = m_index.get<ByAssetId>();
      auto it = idx.find(id);
      if (it != idx.end())
      {
        asset = it->m_asset;
        if (!asset->isRemoved())
        {
          asset->setProperty("removed", true);
          Timestamp ts = time ? *time : std::chrono::system_clock::now();
          asset->setProperty("timestamp", ts);
          adjustCount(asset, 1);
        }
      }

      return asset;
    }

    AssetPtr getAsset(const std::string &id) const override
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      const auto &idx = m_index.get<ByAssetId>();
      auto it = idx.find(id);
      if (it != idx.end())
        return it->m_asset;
      else
        return nullptr;
    }

    virtual size_t getAssets(AssetList &list, size_t max, const bool active = true,
                             const std::optional<std::string> device = std::nullopt,
                             const std::optional<std::string> type = std::nullopt) const override
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      boost::any_range<AssetNode, boost::forward_traversal_tag> range;

      if (device)
      {
        if (type)
          range = m_index.get<ByDeviceAndType>().equal_range(std::make_tuple(*device, *type));
        else
          range = m_index.get<ByDeviceAndType>().equal_range(std::make_tuple(*device));
      }
      else if (type)
      {
        range = m_index.get<ByType>().equal_range(*type);
      }
      else
      {
        auto &idx = m_index.get<ByFifo>();
        range = std::make_pair(idx.begin(), idx.end());
      }

      for (auto &a : range)
      {
        if (!active || !a.isRemoved())
          list.push_back(a.m_asset);
        if (list.size() >= max)
          break;
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

    size_t getCountForDeviceAndType(const std::string &device, const std::string &type,
                                    bool active = true) const override
    {
      using namespace boost::adaptors;

      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);

      return boost::count_if(
          m_index.get<ByDeviceAndType>().equal_range(std::make_tuple(device, type)),
          activePredicate(active));
    }

    size_t getCountForType(const std::string &type, bool active = true) const override
    {
      using namespace boost::adaptors;

      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);

      return boost::count_if(m_index.get<ByType>().equal_range(type), activePredicate(active));
    }

    size_t getCountForDevice(const std::string &device, bool active = true) const override
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);

      return boost::count_if(m_index.get<ByDeviceAndType>().equal_range(std::make_tuple(device)),
                             activePredicate(active));
    }

    TypeCount getCountsByType(bool active = true) const override
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      TypeCount res;
      auto &idx = m_index.get<ByType>();
      auto it = idx.begin();
      while (it != idx.end())
      {
        int delta = 0;
        auto &type = it->getType();
        auto rng = idx.equal_range(type);
        if (active)
        {
          auto cit = m_typeRemoveCount.find(type);
          delta = cit != m_typeRemoveCount.end() ? (int32_t)cit->second : 0;
        }
        auto count = distance(rng.first, rng.second) - delta;
        if (count > 0)
          res[type] = count;
        it = rng.second;
      }

      return res;
    }

    TypeCount getCountsByTypeForDevice(const std::string &device, bool active = true) const override
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      TypeCount res;
      auto &idx = m_index.get<ByDeviceAndType>();
      auto removes = m_deviceRemoveCount.find(device);
      const auto *ridx {removes == m_deviceRemoveCount.end() ? nullptr : &removes->second};

      auto range = idx.equal_range(std::make_tuple(device));
      auto it = range.first;
      while (it != range.second)
      {
        int delta = 0;
        auto &type = it->getType();
        auto rng = idx.equal_range(std::make_tuple(device, type));
        if (ridx != nullptr && active)
        {
          auto cit = ridx->find(type);
          delta = cit != ridx->end() ? int32_t(cit->second) : 0;
        }
        auto count = distance(rng.first, rng.second) - delta;
        if (count > 0)
          res[type] = count;
        it = rng.second;
      }

      return res;
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

    int32_t getIndex(const std::string &id) const
    {
      auto &idx = m_index.get<ByAssetId>();
      auto it = idx.find(id);
      if (it != idx.end())
      {
        auto &fifo = m_index.get<ByFifo>();
        auto pos = mic::project<ByFifo>(m_index, it);
        return int32_t(std::distance(fifo.begin(), pos));
      }

      return -1;
    }

  protected:
    void adjustCount(AssetPtr asset, int delta)
    {
      const auto &type = asset->getType();
      const auto &dev = asset->getDeviceUuid();
      bool found = false;

      if (dev)
      {
        auto it = m_deviceRemoveCount.find(*dev);
        if (it != m_deviceRemoveCount.end())
        {
          auto ti = it->second.find(type);
          if (ti != it->second.end())
          {
            ti->second += delta;
            found = true;
          }
        }

        if (!found && delta > 0)
        {
          m_deviceRemoveCount[*dev][type] = delta;
        }

        m_removedAssets += delta;
      }

      if (auto it = m_typeRemoveCount.find(type); it != m_typeRemoveCount.end())
      {
        it->second += delta;
      }
      else
      {
        m_typeRemoveCount[type] += delta;
      }
    }

    std::function<bool(const AssetNode &)> activePredicate(bool active) const
    {
      if (active)
        return [](const AssetNode &a) -> bool { return !a.isRemoved(); };
      else
        return [](const AssetNode &a) -> bool { return true; };
    }

  protected:
    size_t m_removedAssets {0};
    AssetIndex m_index;

    RemoveCountByDeviceAndType m_deviceRemoveCount;
    RemoveCountByType m_typeRemoveCount;
  };
}  // namespace mtconnect::asset
