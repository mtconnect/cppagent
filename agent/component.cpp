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

#include "component.hpp"
#include "data_item.hpp"
#include <stdlib.h>

using namespace std;

/* Component static constants */
const string Component::SComponentSpecs[NumComponentSpecs] = {
  // Component parts
  "Device",
  // Component details
  "Components",
  "DataItem",
  "DataItems",
  "Configuration",
  "Description",
  "Source",
  "text"
};

/* Component public methods */
Component::Component(const string& cls, map<string, string> attributes,
                     const string &aPrefix)
{
  mId = attributes["id"];
  mName = attributes["name"];
  mNativeName = attributes["nativeName"];
  
  mUuid = attributes["uuid"];
  mSampleRate = (float) (attributes["sampleRate"].empty()) ?
    0.0f : atof(attributes["sampleRate"].c_str());
  
  mParent = NULL;
  mDevice = NULL;
  mAvailability = NULL;
  mAssetChanged = NULL;
  mClass = cls;
  mPrefix = aPrefix;
  mPrefixedClass = aPrefix + ":" + cls;
  mAttributes = buildAttributes();
}

Component::~Component()
{
}

std::map<string, string> Component::buildAttributes() const
{
  std::map<string, string> attributes;
  
  attributes["id"] = mId;
  attributes["name"] = mName;
  
  if (mSampleRate != 0.0f)
  {
    attributes["sampleRate"] = floatToString(mSampleRate);
  }
  
  if (!mUuid.empty())
  {
    attributes["uuid"] = mUuid;
  }
  
  if (!mNativeName.empty())
  {
    attributes["nativeName"] = mNativeName;
  }

  return attributes;
}

void Component::addDescription(string body, map<string, string> attributes)
{
  if (!attributes["manufacturer"].empty())
  {
    mManufacturer = attributes["manufacturer"];
  }
  
  if (!attributes["serialNumber"].empty())
  {
    mSerialNumber = attributes["serialNumber"];
  }
  
  if (!attributes["station"].empty())
  {
    mStation = attributes["station"];
  }
  
  if (!body.empty())
  {
    mDescriptionBody = body;
  }
}

std::map<string, string> Component::getDescription() const
{
  std::map<string, string> description;
  
  if (!mManufacturer.empty())
  {
    description["manufacturer"] = mManufacturer;
  }
  
  if (!mSerialNumber.empty())
  {
    description["serialNumber"] = mSerialNumber;
  }
  
  if (!mStation.empty())
  {
    description["station"] = mStation;
  }
  
  return description;
}

Device * Component::getDevice()
{
  if (mDevice == NULL) 
  {
    if (getClass() == "Device")
    {
      mDevice = (Device*) this;
    }
    else if (mParent != NULL)
    {
      mDevice = mParent->getDevice();
    }
  }  
  return mDevice;
}

void Component::addDataItem(DataItem& dataItem) 
{ 
  if (dataItem.getType() == "AVAILABILITY")
    mAvailability = &dataItem;
  else if (dataItem.getType() == "ASSET_CHANGED")
    mAssetChanged = &dataItem;
    
  mDataItems.push_back(&dataItem); 
}

