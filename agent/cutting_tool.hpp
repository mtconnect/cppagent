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


#ifndef CUTTING_TOOL_HPP
#define CUTTING_TOOL_HPP

#include "asset.hpp"
#include <vector>
#include <map>

class CuttingTool;
typedef RefCountedPtr<CuttingTool> CuttingToolPtr;

class CuttingToolValue;
typedef RefCountedPtr<CuttingToolValue> CuttingToolValuePtr;

class CuttingItem;
typedef RefCountedPtr<CuttingItem> CuttingItemPtr;


class CuttingToolValue : public RefCounted {
public:  
  CuttingToolValue(const std::string &aKey, const std::string &aValue) 
    : mKey(aKey), mValue(aValue) {}
  CuttingToolValue() {}
  CuttingToolValue(const CuttingToolValue &aOther) 
    : mProperties(aOther.mProperties), mKey(aOther.mKey), mValue(aOther.mValue)  {}
  virtual ~CuttingToolValue();
    
public:
  std::map<std::string, std::string> mProperties;
  std::string mKey;
  std::string mValue;
};

class CuttingItem : public RefCounted {
public:
  virtual ~CuttingItem();

public:
  std::map<std::string,std::string> mIdentity;
  std::map<std::string,CuttingToolValuePtr> mValues;
  std::map<std::string,CuttingToolValuePtr> mMeasurements;
};

class CuttingTool : public Asset {
public:
  CuttingTool(const std::string &aAssetId, const std::string &aType, const std::string &aContent) 
    : Asset(aAssetId, aType, aContent) {}
  
  void addIdentity(const std::string &aKey, const std::string &aValue);
  void addValue(const CuttingToolValuePtr aValue);
  void updateValue(const std::string &aKey, const std::string &aValue);

  std::vector<std::string> mStatus;
  std::map<std::string,std::string> mIdentity;
  std::map<std::string,CuttingToolValuePtr> mValues;
  std::map<std::string,CuttingToolValuePtr> mMeasurements;  
  std::string mItemCount;
  std::vector<CuttingItemPtr> mItems;  
};

#endif
