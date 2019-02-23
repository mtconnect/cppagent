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

namespace mtconnect {
  static dlib::logger g_logger("device");
  
  Device::Device(const std::map<std::string, std::string> &attributes) : 
  Component("Device", attributes),
  m_preserveUuid(false),
  m_availabilityAdded(false),
  m_iso841Class(-1)
  {
    const auto isoPos = attributes.find("iso841Class");
    if (isoPos != attributes.end())
    {
      m_iso841Class = atoi(isoPos->second.c_str());
      m_attributes["iso841Class"] = isoPos->second;
    }
  }
  
  
  Device::~Device()
  {
  }
  
  
  void Device::addDeviceDataItem(DataItem &dataItem)
  {
    if (!dataItem.getSource().empty())
      m_deviceDataItemsBySource[dataItem.getSource()] = &dataItem;
    
    if (!dataItem.getName().empty())
      m_deviceDataItemsByName[dataItem.getName()] = &dataItem;
    
    if(m_deviceDataItemsById.find(dataItem.getId()) != m_deviceDataItemsById.end())
    {
      g_logger << dlib::LERROR << "Duplicate data item id: " << dataItem.getId() << " for device "
      << m_name << ", skipping";
    }
    else
      m_deviceDataItemsById[dataItem.getId()] = &dataItem;
  }
  
  
  DataItem *Device::getDeviceDataItem(const std::string &name)
  {
    const auto sourcePos = m_deviceDataItemsBySource.find(name);
    if(sourcePos != m_deviceDataItemsBySource.end())
      return sourcePos->second;
    
    const auto namePos = m_deviceDataItemsByName.find(name);
    if(namePos != m_deviceDataItemsByName.end())
      return namePos->second;
    
    const auto &idPos = m_deviceDataItemsById.find(name);
    if(idPos != m_deviceDataItemsById.end())
      return idPos->second;
    
    return nullptr;
  }
}
