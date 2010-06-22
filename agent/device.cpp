
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

#include "device.hpp"

using namespace std;

/* Device public methods */
Device::Device(std::map<std::string, std::string> attributes)
  : Component("Device", attributes)
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
  mDeviceDataItemsById[dataItem.getId()] = &dataItem;
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

