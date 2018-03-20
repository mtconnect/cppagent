/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

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
  std::string mAssetId;
  std::string mContent;
  std::string mType;
  std::string mDeviceUuid;
  std::string mTimestamp;
  std::string mDescription;
  XmlAttributes mArchetype;
  
  bool        mRemoved;
  AssetKeys   mKeys;
  AssetKeys   mIdentity;
  
public:
  Asset();
  Asset(const Asset &aOther) {
    mAssetId = aOther.mAssetId;
    mContent = aOther.mContent;
    mType = aOther.mType;
    mRemoved = aOther.mRemoved;
  }
  Asset(const std::string &aAssetId, const std::string &aType, const std::string &aContent,
        const bool aRemoved = false);
  virtual ~Asset();

  const std::string &getAssetId() const { return mAssetId; }
  virtual std::string &getContent() { return mContent; }
  const std::string &getType() const { return mType; }
  AssetKeys   &getKeys() { return mKeys; }
  const std::string &getDeviceUuid() const { return mDeviceUuid; }
  const std::string &getTimestamp() const { return mTimestamp; }
  const std::string &getDescription() const { return mDescription; }
  const XmlAttributes &getArchetype() const { return mArchetype; }
  bool isRemoved() const { return mRemoved; }
  
  AssetKeys &getIdentity() { return mIdentity; }
  
  bool operator==(const Asset &aOther) {
    return mAssetId == aOther.mAssetId;
  }
  
  void setAssetId(const std::string &aId) { mAssetId = aId; }
  void setDeviceUuid(const std::string &aId) { mDeviceUuid = aId; }
  void setTimestamp(const std::string &aTs) { mTimestamp = aTs; }
  void setRemoved(bool aRemoved) { mRemoved = aRemoved; }
  void setDescription(const std::string &aDesc) { mDescription = aDesc; }
  void setArchetype(const XmlAttributes &anArch) { mArchetype = anArch; }
  
  virtual void changed() { }
  virtual void addIdentity(const std::string &aKey, const std::string &aValue);  
};

typedef std::map<std::string, AssetPtr> AssetIndex;

