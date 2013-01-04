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

#ifndef ASSET_HPP
#define ASSET_HPP

#include <string>
#include <map>
#include "ref_counted.hpp"

class Asset;
typedef RefCountedPtr<Asset> AssetPtr;

// An association of the index type to the value.
typedef std::map<std::string,std::string> AssetKeys;

class Asset : public RefCounted
{
protected:
  std::string mAssetId;
  std::string mContent;
  std::string mType;
  std::string mDeviceUuid;
  std::string mTimestamp;
  AssetKeys   mKeys;
  
public:
  Asset();
  Asset(const Asset &aOther) {
    mAssetId = aOther.mAssetId;
    mContent = aOther.mContent;
    mType = aOther.mType;
  }
  Asset(const std::string &aAssetId, const std::string &aType, const std::string &aContent);
  virtual ~Asset();

  std::string &getAssetId() { return mAssetId; }
  virtual std::string &getContent() { return mContent; }
  std::string &getType() { return mType; }
  AssetKeys   &getKeys() { return mKeys; }
  std::string &getDeviceUuid() { return mDeviceUuid; }
  std::string &getTimestamp() { return mTimestamp; }
  
  bool operator==(const Asset &aOther) {
    return mAssetId == aOther.mAssetId;
  }
  
  void setAssetId(const std::string &aId) { mAssetId = aId; }
  void setDeviceUuid(const std::string &aId) { mDeviceUuid = aId; }
  void setTimestamp(const std::string &aTs) { mTimestamp = aTs; }
};

typedef std::map<std::string, AssetPtr> AssetIndex;


#endif
