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

#include "adapter.hpp"

using namespace std;

/* Adapter public methods */
Adapter::Adapter(
    const string& device,
    const string& server,
    const unsigned int port
  )
: Connector(server, port), mDevice(device)
{
  // Will start threaded object: Adapter::thread()
  start();
}

Adapter::~Adapter()
{
  // Will stop threaded object gracefully Adapter::thread()
  stop();
  wait();
}

/**
 * Expected data to parse in SDHR format:
 *   Time|Alarm|Code|NativeCode|Severity|State|Description
 *   Time|Item|Value
 *   Time|Item1|Value1|Item2|Value2...
 */

void Adapter::processData(const string& data)
{
  istringstream toParse(data);
  string key;
  
  getline(toParse, key, '|');
  string time = key;
  
  getline(toParse, key, '|');
  string type = key;
  
  string value;
  getline(toParse, value, '|');

  DataItem *dataItem = mAgent->getDataItemByName(mDevice, key);
  if (dataItem == NULL)
  {
    logEvent("Agent", "Could not find data item: " + key);
  }
  else
  {
    string rest;
    if (dataItem->isCondition() || dataItem->getType() == "ALARM")
    {
      getline(toParse, rest);
      value = value + "|" + rest;
    }

    // Add key->value pairings
    dataItem->setDataSource(this);
    mAgent->addToBuffer(dataItem, value, time);
  }
  
  // Look for more key->value pairings in the rest of the data
  while (getline(toParse, key, '|') && getline(toParse, value, '|'))
  {
    dataItem = mAgent->getDataItemByName(mDevice, key);
    if (dataItem == NULL)
    {
      logEvent("Agent", "Could not find data item: " + key);
    }
    else
    {
      dataItem->setDataSource(this);
      mAgent->addToBuffer(dataItem, toUpperCase(value), time);
    }
  }
}

void Adapter::disconnected()
{
  mAgent->disconnected(this, mDevice);
}

/* Adapter private methods */
void Adapter::thread()
{
  // Start the connection to the socket
  while (true)
  {
    connect();
    // Try to reconnect every 10 seconds
    dlib::sleep(10 * 1000);
  }
}

