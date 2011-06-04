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

