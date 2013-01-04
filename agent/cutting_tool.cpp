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

#include "cutting_tool.hpp"
#include "xml_printer.hpp"

#include <sstream>

using namespace std;

CuttingTool::~CuttingTool()
{
}

void CuttingTool::addValue(const CuttingToolValuePtr aValue)
{
  // Check for keys...
  if (aValue->mKey == "Location") {
    mKeys[aValue->mKey] = aValue->mValue;
  }
  
  mValues[aValue->mKey] = aValue;
}

inline static bool splitKey(string &key, string &sel, string &val) 
{
  size_t found = key.find_first_of('@');
  if (found != string::npos) {
    sel = key;
    key.erase(found);
    sel.erase(0, found + 1);
    size_t found = sel.find_first_of('=');
    if (found != string::npos) {
      val = sel;
      sel.erase(found);
      val.erase(0, found + 1);
      return true;
    }
  }
  return false;
}

void CuttingTool::updateValue(const std::string &aKey, const std::string &aValue)
{
  if (aKey == "Location") {
    mKeys[aKey] = aValue;
  }
  
  // Split into path and parts and update the asset bits.
  string key = aKey, sel, val;
  if (splitKey(key, sel, val)) {
    if (key == "ToolLife") {
      for (size_t i = 0; i < mLives.size(); i++)
      {
        CuttingToolValuePtr life = mLives[i];
        if (life->mProperties.count(sel) > 0 && life->mProperties[sel] == val)
        {
          life->mValue = aValue;
          break;
        }
      }
    } else {
      for (size_t i = 0; i < mItems.size(); i++)
      {
        CuttingItemPtr item = mItems[i];
        if (item->mIdentity.count(sel) > 0 && val == item->mIdentity[sel])
        {
          if (item->mValues.count(key) > 0)
            item->mValues[key]->mValue = aValue;
          else if (item->mMeasurements.count(key) > 0)
            item->mMeasurements[key]->mValue = aValue;
          break;
        }
      }      
    }
  } else {
    if (aKey == "CutterStatus") {
      mStatus.clear();
      istringstream stream(aValue);
      string val;
      while (getline(stream, val, ',')) {
        mStatus.push_back(val);
      }
    } else {
      if (mValues.count(aKey) > 0)
        mValues[aKey]->mValue = aValue;
      else if (mMeasurements.count(aKey) > 0)
        mMeasurements[aKey]->mValue = aValue;
    }
  }
}

void CuttingTool::addIdentity(const std::string &aKey, const std::string &aValue)
{
  if (aKey == "toolId") {
    mKeys[aKey] = aValue;
    mIdentity[aKey] = aValue;
  } else if (aKey == "deviceUuid") {
    mDeviceUuid = aValue;
  } else if (aKey == "timestamp") {
    mTimestamp = aValue;
  } else {
    mIdentity[aKey] = aValue;
  }
}

std::string &CuttingTool::getContent() 
{ 
  if (mContent.empty())
    mContent = XmlPrinter::printCuttingTool(this);
  
  return mContent; 
}


CuttingToolValue::~CuttingToolValue()
{
}

CuttingItem::~CuttingItem()
{
}
