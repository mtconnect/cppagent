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

#ifndef ADAPTER_HPP
#define ADAPTER_HPP

#include <string>
#include <sstream>

#include "dlib/sockets.h"
#include "dlib/threads.h"

#include "agent.hpp"
#include "connector.hpp"
#include "globals.hpp"

class Agent;
class Device;

using namespace dlib;

class Adapter : public Connector, public threaded_object
{
public:
  /* Associate adapter with a device & connect to the server & port */
  Adapter(const std::string& device,
          const std::string& server, 
          const unsigned int port,
          int aLegacyTimeout = 600);
  
  /* Virtual destructor */
  virtual ~Adapter();
  
  /* Set pointer to the agent */
  void setAgent(Agent& agent);
  bool isDupChecking() { return mDupCheck; }
  void setDupCheck(bool aFlag) { mDupCheck = aFlag; }

  bool isAutoAvailable() { return mAutoAvailable; }
  void setAutoAvailable(bool aFlag) { mAutoAvailable = aFlag; }

  bool isIgnoringTimestamps() { return mIgnoreTimestamps; }
  void setIgnoreTimestamps(bool aFlag) { mIgnoreTimestamps = aFlag; }
  
  void setReconnectInterval(int aInterval) { mReconnectInterval = aInterval; }
  int getReconnectInterval() const { return mReconnectInterval; }
  
  /* Inherited method to incoming data from the server */
  virtual void processData(const std::string& data);
  virtual void protocolCommand(const std::string& data);
  
  /* Method called when connection is lost. */
  virtual void disconnected();
  virtual void connected();

  /* For the additional devices associated with this adapter */
  void addDevice(std::string &aDevice);
  
protected:
  /* Pointer to the agent */
  Agent *mAgent;
  Device *mDevice;
  std::vector<Device*> mAllDevices;

  /* Name of device associated with adapter */
  std::string mDeviceName;
  
  /* If the connector has been running */
  bool mRunning;

  /* Check for dups */
  bool mDupCheck;
  bool mAutoAvailable;
  bool mIgnoreTimestamps;
  
  // For multiline asset parsing...
  bool mGatheringAsset;
  std::string mTerminator;
  std::string mAssetId;
  std::string mAssetType;
  std::string mTime;
  std::ostringstream mBody;
  Device *mAssetDevice;
  
  // Timeout for reconnection attempts, given in milliseconds
  int mReconnectInterval;
  
private:
  /* Inherited and is run as part of the threaded_object */
  void thread();
};

#endif

