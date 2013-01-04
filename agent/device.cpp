
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

#include "device.hpp"
#include "dlib/logger.h"
#include <dlib/misc_api.h>

using namespace std;

static dlib::logger sLogger("device");


/* Device public methods */
Device::Device(std::map<std::string, std::string> attributes)
  : Component("Device", attributes), mPreserveUuid(false), mAvailabilityAdded(false)
{
  if (!attributes["iso841Class"].empty())
  {
    mIso841Class = atoi(attributes["iso841Class"].c_str());
    mAttributes["iso841Class"] = attributes["iso841Class"];
  } else {
    mIso841Class = -1;
  }
}

Device::~Device()
{
}

void Device::addDeviceDataItem(DataItem& dataItem) {
  if (!dataItem.getSource().empty())
    mDeviceDataItemsBySource[dataItem.getSource()] = &dataItem;
  if (!dataItem.getName().empty())
    mDeviceDataItemsByName[dataItem.getName()] = &dataItem;
  
  if (mDeviceDataItemsById[dataItem.getId()] != NULL) {
    sLogger << dlib::LERROR << "Duplicate data item id: " << dataItem.getId() << " for device " 
            << mName << ", skipping";
  } else {
    mDeviceDataItemsById[dataItem.getId()] = &dataItem;
  }
}

DataItem * Device::getDeviceDataItem(const std::string& aName) {
  DataItem * di;
  if (mDeviceDataItemsBySource.count(aName) > 0 && (di = mDeviceDataItemsBySource[aName]))
    return di;
  else if (mDeviceDataItemsByName.count(aName)> 0 && (di = mDeviceDataItemsByName[aName]))
    return di;
  else if (mDeviceDataItemsById.count(aName) > 0)
    di = mDeviceDataItemsById[aName];
  else
    di = NULL;
  return di;
}

