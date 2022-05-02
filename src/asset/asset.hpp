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
#include <string>

#include "entity/entity.hpp"
#include "entity/factory.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace asset {
    class Asset;
    using AssetPtr = std::shared_ptr<Asset>;
    using AssetList = std::list<AssetPtr>;

    class Asset : public entity::Entity
    {
    public:
      Asset(const std::string &name, const entity::Properties &props)
        : entity::Entity(name, props), m_removed(false)
      {
        auto removed = maybeGet<bool>("removed");
        m_removed = removed && *removed;
      }
      ~Asset() override = default;

      const entity::Value &getIdentity() const override { return getProperty("assetId"); }

      static entity::FactoryPtr getFactory();
      static entity::FactoryPtr getRoot();

      void setProperty(const std::string &key, const entity::Value &v) override
      {
        entity::Value r = v;
        if (key == "removed")
        {
          if (std::holds_alternative<bool>(r))
            m_removed = std::get<bool>(r);
          else
            entity::ConvertValueToType(r, entity::BOOL);
        }

        m_properties.insert_or_assign(key, r);
      }
      void setProperty(const entity::Property &property) { Entity::setProperty(property); }

      const auto &getType() const { return getName(); }
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
      void setAssetId(const std::string &id)
      {
        m_assetId = id;
        setProperty("assetId", id);
      }

      const std::optional<std::string> getDeviceUuid() const
      {
        const auto &v = getProperty("deviceUuid");
        if (std::holds_alternative<std::string>(v))
          return std::get<std::string>(v);
        else
          return std::nullopt;
      }
      const std::optional<Timestamp> getTimestamp() const
      {
        const auto &v = getProperty("timestamp");
        if (std::holds_alternative<Timestamp>(v))
          return std::get<Timestamp>(v);
        else
          return std::nullopt;
      }
      bool isRemoved() const { return m_removed; }
      void setRemoved()
      {
        setProperty("removed", true);
        m_removed = true;
      }
      static void registerAssetType(const std::string &t, entity::FactoryPtr factory);

      bool operator==(const Asset &another) const { return getAssetId() == another.getAssetId(); }

    protected:
      std::string m_assetId;
      bool m_removed;
    };

    class ExtendedAsset : public Asset
    {
    public:
      static entity::FactoryPtr getFactory();
    };
  }  // namespace asset
}  // namespace mtconnect
