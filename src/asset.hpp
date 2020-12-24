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
#include <string>

namespace mtconnect
{
  class Asset : public entity::Entity
  {
  public:
    Asset(const std::string &name, const entity::Properties &props)
      : entity::Entity(name, props), m_removed(false)
    {
      auto &removed = getProperty("removed");
      if (std::holds_alternative<std::string>(removed) &&
          std::get<std::string>(removed) == "true")
        m_removed = true;
    }
    ~Asset() override = default;
    
    static entity::FactoryPtr getFactory();
    static entity::FactoryPtr getRoot();

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
    bool isRemoved() const { return m_removed; }
    void setRemoved()
    {
      addProperty({ "removed" , "true" });
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
  
  template<class T>
  struct RegisterAsset
  {
    RegisterAsset(const std::string &t)
    {
      Asset::registerAssetType(t, T::getFactory());
    }
  };
  
}  // namespace mtconnect
