//
// Copyright Copyright 2012, System Insights, Inc.
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

#include <string>
#include <map>
#include "ref_counted.hpp"

class Asset;
typedef RefCountedPtr<Asset> AssetPtr;

// An association of the index type to the value.
typedef std::map<std::string,std::string> AssetKeys;
typedef std::map<std::string,std::string> XmlAttributes;

class Asset : public RefCounted
{
protected:
  std::string m_assetId;
  std::string m_content;
  std::string m_type;
  std::string m_deviceUuid;
  std::string m_timestamp;
  std::string m_description;
  XmlAttributes m_archetype;
  
  bool        m_removed;
  AssetKeys   m_keys;
  AssetKeys   m_identity;
  
public:
  Asset();
  Asset(const Asset &aOther) {
    m_assetId = aOther.m_assetId;
    m_content = aOther.m_content;
    m_type = aOther.m_type;
    m_removed = aOther.m_removed;
  }
  Asset(const std::string &aAssetId, const std::string &aType, const std::string &aContent,
        const bool aRemoved = false);
  virtual ~Asset();

  const std::string &getAssetId() const { return m_assetId; }
  virtual std::string &getContent() { return m_content; }
  const std::string &getType() const { return m_type; }
  AssetKeys   &getKeys() { return m_keys; }
  const std::string &getDeviceUuid() const { return m_deviceUuid; }
  const std::string &getTimestamp() const { return m_timestamp; }
  const std::string &getDescription() const { return m_description; }
  const XmlAttributes &getArchetype() const { return m_archetype; }
  bool isRemoved() const { return m_removed; }
  
  AssetKeys &getIdentity() { return m_identity; }
  
  bool operator==(const Asset &aOther) {
    return m_assetId == aOther.m_assetId;
  }
  
  void setAssetId(const std::string &aId) { m_assetId = aId; }
  void setDeviceUuid(const std::string &aId) { m_deviceUuid = aId; }
  void setTimestamp(const std::string &aTs) { m_timestamp = aTs; }
  void setRemoved(bool aRemoved) { m_removed = aRemoved; }
  void setDescription(const std::string &aDesc) { m_description = aDesc; }
  void setArchetype(const XmlAttributes &anArch) { m_archetype = anArch; }
  
  virtual void changed() { }
  virtual void addIdentity(const std::string &aKey, const std::string &aValue);  
};

typedef std::map<std::string, AssetPtr> AssetIndex;

