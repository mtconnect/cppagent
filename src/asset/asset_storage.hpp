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
#include <memory>
#include <mutex>
#include <unordered_map>

#include "asset/asset.hpp"
#include "entity/entity.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace asset {
    class AssetStorage
    {
    public:
      using TypeCount = std::map<std::string, size_t>;

      AssetStorage(size_t max) : m_maxAssets(max) {}
      virtual ~AssetStorage() = default;

      size_t getMaxAssets() const { return m_maxAssets; }
      virtual size_t getCount(bool active = true) const = 0;
      virtual TypeCount getCountsByType(bool active = true) const = 0;

      // Mutation
      virtual AssetPtr addAsset(AssetPtr asset) = 0;
      virtual AssetPtr removeAsset(const std::string &id,
                                   const std::optional<Timestamp> &time = std::nullopt) = 0;

      // Retrival
      virtual AssetPtr getAsset(const std::string &id) const = 0;
      virtual size_t getAssets(AssetList &list, size_t max, const bool active = true,
                               const std::optional<std::string> device = std::nullopt,
                               const std::optional<std::string> type = std::nullopt) const = 0;
      virtual size_t getAssets(AssetList &list, const std::list<std::string> &ids) const = 0;

      // Count
      virtual size_t getCountForDeviceAndType(const std::string &device, const std::string &type,
                                              bool active = true) const = 0;
      virtual size_t getCountForType(const std::string &type, bool active = true) const = 0;
      virtual size_t getCountForDevice(const std::string &device, bool active = true) const = 0;

      virtual TypeCount getCountsByTypeForDevice(const std::string &device,
                                                 bool active = true) const = 0;

      // Bulk remove
      virtual size_t removeAll(AssetList &list,
                               const std::optional<std::string> device = std::nullopt,
                               const std::optional<std::string> type = std::nullopt,
                               const std::optional<Timestamp> &time = std::nullopt) = 0;

      // For mutex locking
      auto lock() { return m_bufferLock.lock(); }
      auto unlock() { return m_bufferLock.unlock(); }
      auto try_lock() { return m_bufferLock.try_lock(); }

    protected:
      // Access control to the buffer
      mutable std::recursive_mutex m_bufferLock;
      size_t m_maxAssets;
    };
  }  // namespace asset
}  // namespace mtconnect
