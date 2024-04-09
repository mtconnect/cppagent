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
#include <memory>
#include <mutex>
#include <unordered_map>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  namespace asset {
    /// @brief Abstract asset storage
    ///
    /// Assets can be stored in memory or persisted. The agent uses
    /// the `AssetStorage` to abstract storage and retrieval of
    /// assets by assetId, type, and device.
    ///
    /// When an asset it is added or updated it moves to the beginning of the asset list.
    /// Assets are deleted from storage when there are `max` assets in the buffer and another
    /// is added. The oldest assets are removed first.
    ///
    /// Removal does not change the asset position and marks the asset as removed.
    class AGENT_LIB_API AssetStorage
    {
    public:
      /// @brief A map of types to counts
      using TypeCount = std::map<std::string, size_t>;

      /// @brief Constructor with a max count
      /// @param[in] max Max count can be max size_t if there are no limits to storage
      ///        in memory storage is assumed to be finite.
      AssetStorage(size_t max) : m_maxAssets(max) {}
      virtual ~AssetStorage() = default;

      /// @brief Get the maximum number of assets that con be stored
      /// @return The maximum number of assets.
      size_t getMaxAssets() const { return m_maxAssets; }
      /// @brief Get the number of assets in storage
      /// @param[in] active `true` if only assets that are not removed are counted
      /// @return total count
      virtual size_t getCount(bool active = true) const = 0;

      /// @brief Get an associative array of counts by type
      /// @param[in] active `true` if only assets that are not removed are counted
      /// @return the counts by type
      virtual TypeCount getCountsByType(bool active = true) const = 0;

      /// @name Create, Update, and Removal
      ///@{

      /// @brief add an asset to the storage
      /// @param[in] asset an asset
      /// @return shared pointer to the old asset if changed
      virtual AssetPtr addAsset(AssetPtr asset) = 0;

      /// @brief mark an asset as removed by assetId
      /// @param[in] id the assetId
      /// @param[in] time the timestamp for the removal
      /// @return shared pointer to the removed asset if found
      virtual AssetPtr removeAsset(const std::string &id,
                                   const std::optional<Timestamp> &time = std::nullopt) = 0;
      /// @brief Remove assets by device and type
      /// @param[out] list list of assets removed
      /// @param[in] device optional device to filter assets
      /// @param[in] type optional type to filter assets
      /// @param[in] time optional timestamp, defaults to now
      /// @return the number of assets removed
      virtual size_t removeAll(AssetList &list,
                               const std::optional<std::string> device = std::nullopt,
                               const std::optional<std::string> type = std::nullopt,
                               const std::optional<Timestamp> &time = std::nullopt) = 0;
      ///@}

      /// @name Retrival
      ///@{

      /// @brief get an asset by its assetId
      /// @param[in] id the assetId
      /// @return shared point to the asset if found
      virtual AssetPtr getAsset(const std::string &id) const = 0;
      /// @brief get a list of assets with optional filters
      /// @param[out] list returned list of assets
      /// @param[in] max maximum number of assets to find
      /// @param[in] active `false` to skip removed assets
      /// @param[in] device optional device uuid to select
      /// @param[in] type optional type to select
      /// @return the number of assets found
      virtual size_t getAssets(AssetList &list, size_t max, const bool active = true,
                               const std::optional<std::string> device = std::nullopt,
                               const std::optional<std::string> type = std::nullopt) const = 0;
      /// @brief get a list of assets given a list of asset ids
      /// @param[out] list list of assets
      /// @param[in] ids assetIds to find
      /// @return the number of assets found
      virtual size_t getAssets(AssetList &list, const std::list<std::string> &ids) const = 0;
      ///@}

      /// @name Count related methods
      ///@{

      /// @brief gets counts of assets by device and type
      /// @param[in] device a device uuid
      /// @param[in] type the type of asset
      /// @param[in] active `false` to skip removed assets
      /// @return the number of assets
      virtual size_t getCountForDeviceAndType(const std::string &device, const std::string &type,
                                              bool active = true) const = 0;

      /// @brief get count of a type of asset for all devices
      /// @param[in] type the type
      /// @param[in] active `false` to skip removed assets
      /// @return the number of assets
      virtual size_t getCountForType(const std::string &type, bool active = true) const = 0;
      /// @brief get count of all types of assets for a device
      /// @param[in] device the device uuid
      /// @param[in] active `false` to skip removed assets
      /// @return the number of assets
      virtual size_t getCountForDevice(const std::string &device, bool active = true) const = 0;
      /// @brief get the count by types for a device
      /// @param[in] device the device uuid
      /// @param[in] active `false` to skip removed assets
      /// @return a map of types and their counts
      virtual TypeCount getCountsByTypeForDevice(const std::string &device,
                                                 bool active = true) const = 0;
      ///@}

      /// @name For mutex locking
      ///@{

      /// @brief lock the storage
      auto lock() { return m_bufferLock.lock(); }
      /// @brief unlock the storage
      auto unlock() { return m_bufferLock.unlock(); }
      /// @brief try to lock the storage
      auto try_lock() { return m_bufferLock.try_lock(); }
      ///@}

    protected:
      // Access control to the buffer
      mutable std::recursive_mutex m_bufferLock;
      size_t m_maxAssets;
    };
  }  // namespace asset
}  // namespace mtconnect
