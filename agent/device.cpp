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
#include "device.hpp"
#include "dlib/logger.h"
#include <dlib/misc_api.h>

using namespace std;

static dlib::logger sLogger("device");


/* Device public methods */
Device::Device(std::map<std::string, std::string> attributes)
  : Component("Device", attributes), m_preserveUuid(false), m_availabilityAdded(false)
{
  if (!attributes["iso841Class"].empty())
  {
    m_iso841Class = atoi(attributes["iso841Class"].c_str());
    m_attributes["iso841Class"] = attributes["iso841Class"];
  } else {
    m_iso841Class = -1;
  }
}

Device::~Device()
{
}

void Device::addDeviceDataItem(DataItem& dataItem) {
  if (!dataItem.getSource().empty())
    m_deviceDataItemsBySource[dataItem.getSource()] = &dataItem;
  if (!dataItem.getName().empty())
    m_deviceDataItemsByName[dataItem.getName()] = &dataItem;
  
  if (m_deviceDataItemsById[dataItem.getId()] != NULL) {
    sLogger << dlib::LERROR << "Duplicate data item id: " << dataItem.getId() << " for device " 
            << m_name << ", skipping";
  } else {
    m_deviceDataItemsById[dataItem.getId()] = &dataItem;
  }
}

DataItem * Device::getDeviceDataItem(const std::string& aName) {
  DataItem * di;
  if (m_deviceDataItemsBySource.count(aName) > 0 && (di = m_deviceDataItemsBySource[aName]))
    return di;
  else if (m_deviceDataItemsByName.count(aName)> 0 && (di = m_deviceDataItemsByName[aName]))
    return di;
  else if (m_deviceDataItemsById.count(aName) > 0)
    di = m_deviceDataItemsById[aName];
  else
    di = NULL;
  return di;
}

