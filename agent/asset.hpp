/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
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
