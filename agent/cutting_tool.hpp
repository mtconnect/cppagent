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
  std::vector<CuttingToolValuePtr> mLives;
};

class CuttingTool : public Asset {
public:
  CuttingTool(const std::string &aAssetId, const std::string &aType, const std::string &aContent) 
    : Asset(aAssetId, aType, aContent) {}
  ~CuttingTool();
  
  void addIdentity(const std::string &aKey, const std::string &aValue);
  void addValue(const CuttingToolValuePtr aValue);
  void updateValue(const std::string &aKey, const std::string &aValue);
  
  virtual std::string &getContent();
  void changed() { mContent.clear(); }

public:
  std::vector<std::string> mStatus;
  std::map<std::string,std::string> mIdentity;
  std::map<std::string,CuttingToolValuePtr> mValues;
  std::map<std::string,CuttingToolValuePtr> mMeasurements;  
  std::string mItemCount;
  std::vector<CuttingItemPtr> mItems;
  std::vector<CuttingToolValuePtr> mLives;
};

#endif
