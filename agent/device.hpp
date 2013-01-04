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

#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <map>

#include "component.hpp"
#include "data_item.hpp"

class Component;
class Adapter;

class Device : public Component
{ 
public:
  /* Constructor that sets variables from an attribute map */
  Device(std::map<std::string, std::string> attributes);
  
  /* Destructor */
  ~Device();
    
  /* Add/get items to/from the device name to data item mapping */
  void addDeviceDataItem(DataItem& dataItem);
  DataItem * getDeviceDataItem(const std::string& aName);
  void addAdapter(Adapter *anAdapter) { mAdapters.push_back(anAdapter); }
  
  /* Return the mapping of Device to data items */
  const std::map<std::string, DataItem *> &getDeviceDataItems() const {
    return mDeviceDataItemsById;
  }

  std::vector<Adapter*> mAdapters;
  bool mPreserveUuid;
  bool mAvailabilityAdded;

protected:
  /* The iso841Class of the device */
  unsigned int mIso841Class;
  
  /* Mapping of device names to data items*/
  std::map<std::string, DataItem *> mDeviceDataItemsByName;
  std::map<std::string, DataItem *> mDeviceDataItemsById;
  std::map<std::string, DataItem *> mDeviceDataItemsBySource;
};

#endif

