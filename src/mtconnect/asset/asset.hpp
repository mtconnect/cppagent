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
#include <string>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/factory.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  /// @brief Asset namespace
  namespace asset {
    class Asset;
    using AssetPtr = std::shared_ptr<Asset>;
    using AssetList = std::list<AssetPtr>;

    /// @brief An abstract MTConnect Asset
    ///
    /// The asset provides a common factory to create all known asset types. It can
    /// support raw assets and convert unknown XML documents to entities.
    ///
    /// Each known asset type must register itself with the asset factory.
    class AGENT_LIB_API Asset : public entity::Entity
    {
    public:
      /// @brief Abstract Asset constructor
      /// @param name asset name, sometimes referred to as the asset type
      /// @param props asset properties
      Asset(const std::string &name, const entity::Properties &props)
        : entity::Entity(name, props), m_removed(false)
      {
        auto removed = maybeGet<bool>("removed");
        m_removed = removed && *removed;
      }
      ~Asset() override = default;

      /// @brief an assets identity is its `assetId` property
      /// @return the `assetId`
      const entity::Value &getIdentity() const override { return getProperty("assetId"); }

      /// @brief get the static asset factory
      /// @return shared pointer to the factory
      static entity::FactoryPtr getFactory();
      /// @brief get the root node of the asset hierarchy. This is the `Assets` entity.
      /// @return shared pointer to the root factory
      static entity::FactoryPtr getRoot();

      /// @brief Sets a property of the asset
      ///
      /// Special handling of `removed`. If `true` sets the asset state to removed.
      /// @param key property `key`
      /// @param v property value
      void setProperty(const std::string &key, const entity::Value &v) override
      {
        entity::Value r = v;
        if (key == "removed")
        {
          if (std::holds_alternative<bool>(r))
            m_removed = std::get<bool>(r);
          else
            entity::ConvertValueToType(r, entity::ValueType::BOOL);
        }

        m_properties.insert_or_assign(key, r);
      }
      /// @brief Set a property
      /// @param property the property
      void setProperty(const entity::Property &property) { Entity::setProperty(property); }

      /// @brief Cover method for `getName()`
      const auto &getType() const { return getName(); }

      /// @brief gets the asset id
      ///
      /// Every asset must have an asset id.
      /// @return the assets identity
      /// @throws PropertyError if there is no assetId
      const std::string &getAssetId() const
      {
        if (m_assetId.empty())
        {
          const auto &v = getProperty("assetId");
          if (std::holds_alternative<std::string>(v))
            *const_cast<std::string *>(&m_assetId) = std::get<std::string>(v);
          else
            throw entity::PropertyError("Asset has no assetId");
        }
        return m_assetId;
      }
      /// @brief Set the asset id
      /// @param id the id
      void setAssetId(const std::string &id)
      {
        m_assetId = id;
        setProperty("assetId", id);
      }

      /// @brief get the device uuid
      ///
      /// In version 1.8 and later, all assets must have a device uuid.
      /// @return optional device uuid
      const std::optional<std::string> getDeviceUuid() const
      {
        const auto &v = getProperty("deviceUuid");
        if (std::holds_alternative<std::string>(v))
          return std::get<std::string>(v);
        else
          return std::nullopt;
      }
      /// @brief gets the timestamp if set
      /// @return optional timestamp if available
      const std::optional<Timestamp> getTimestamp() const
      {
        const auto &v = getProperty("timestamp");
        if (std::holds_alternative<Timestamp>(v))
          return std::get<Timestamp>(v);
        else
          return std::nullopt;
      }
      bool isRemoved() const { return m_removed; }
      /// @brief sets the removed state of the asset
      void setRemoved()
      {
        setProperty("removed", true);
        m_removed = true;
      }
      /// @brief register the factory for an asset type
      /// @param t the type or name of the asset
      /// @param factory the factory to create assets
      static void registerAssetType(const std::string &t, entity::FactoryPtr factory);

      /// @brief compares two asset ids
      /// @param another other asset
      /// @return `true` if they have the same asset id
      bool operator==(const Asset &another) const { return getAssetId() == another.getAssetId(); }

    protected:
      /// @brief The virtual method that covers `hash(boost::uuids::detail::sha1&,
      /// boost::unordered_set<std::string> skip)`
      ///
      /// Override to skip the `hash`, `timestamp`, and `removed` properties.
      ///
      /// @param[in,out] sha1 The boost sha1 accumulator
      void hash(::boost::uuids::detail::sha1 &sha1) const override
      {
        static const ::boost::unordered_set<std::string> skip {"hash", "timestamp", "removed"};
        entity::Entity::hash(sha1, skip);
      }

    protected:
      std::string m_assetId;
      bool m_removed;
    };

    /// @brief A simple `RAW` asset that just carries the data associated
    ///        with the top node.
    class AGENT_LIB_API ExtendedAsset : public Asset
    {
    public:
      static entity::FactoryPtr getFactory();
    };
  }  // namespace asset
}  // namespace mtconnect
