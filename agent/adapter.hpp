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

#ifndef ADAPTER_HPP
#define ADAPTER_HPP

#include <string>
#include <sstream>
#include <stdexcept>
#include <set>

#include "dlib/sockets.h"
#include "dlib/threads.h"

#include "agent.hpp"
#include "connector.hpp"
#include "globals.hpp"
#include "data_item.hpp"

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
  Device *getDevice() const { return mDevice; }

  bool isAutoAvailable() { return mAutoAvailable; }
  void setAutoAvailable(bool aFlag) { mAutoAvailable = aFlag; }

  bool isIgnoringTimestamps() { return mIgnoreTimestamps; }
  void setIgnoreTimestamps(bool aFlag) { mIgnoreTimestamps = aFlag; }
  
  void setReconnectInterval(int aInterval) { mReconnectInterval = aInterval; }
  int getReconnectInterval() const { return mReconnectInterval; }
  
  void setRelativeTime(bool aFlag) { mRelativeTime = aFlag; }
  bool getrelativeTime() { return mRelativeTime; }
  
  void setConversionRequired(bool aFlag) { mConversionRequired = aFlag; }
  bool conversionRequired() const { return mConversionRequired; }
  
  void setUpcaseValue(bool aFlag) { mUpcaseValue = aFlag; }
  bool upcaseValue() const { return mUpcaseValue; }
  
  uint64_t getBaseTime() { return mBaseTime; }
  uint64_t getBaseOffset() { return mBaseOffset; }
  
  bool isParsingTime() { return mParseTime; }
  void setParseTime(bool aFlag) { mParseTime = aFlag; }
  
  /* For testing... */
  void setBaseOffset(uint64_t aOffset) { mBaseOffset = aOffset; }
  void setBaseTime(uint64_t aOffset) { mBaseTime = aOffset; }
  static void getEscapedLine(std::istringstream &stream, std::string &store);
  
  /* Inherited method to incoming data from the server */
  virtual void processData(const std::string& data);
  virtual void protocolCommand(const std::string& data);
  
  /* Method called when connection is lost. */
  virtual void disconnected();
  virtual void connected();
  
  bool isDuplicate(DataItem *aDataItem, const std::string &aValue, double aTimeOffset) {
    if (!aDataItem->isDiscrete()) {
      if (aDataItem->hasMinimumDelta() || aDataItem->hasMinimumPeriod())
        return aDataItem->isFiltered(aDataItem->convertValue(atof(aValue.c_str())), aTimeOffset);
      else
        return  mDupCheck && aDataItem->isDuplicate(aValue);
    } else {
      return false;
    }      
  }

  // Stop 
  void stop();

  /* For the additional devices associated with this adapter */
  void addDevice(std::string &aDevice);
  
protected:
  void parseCalibration(const std::string &aString);
  void processAsset(std::istringstream &toParse, const std::string &key, const std::string &value,
                    const std::string &time);
  bool processDataItem(std::istringstream &toParse, const std::string &aLine, const std::string &aKey, const std::string &aValue,
                           const std::string &aTime, double anOffset, bool aFirst = false);
  std::string extractTime(const std::string &time, double &anOffset);
  
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
  bool mRelativeTime;
  bool mConversionRequired;
  bool mUpcaseValue;
  
  // For relative times
  uint64_t mBaseTime;
  uint64_t mBaseOffset;
  
  bool mParseTime;
  
  // For multiline asset parsing...
  bool mGatheringAsset;
  std::string mTerminator;
  std::string mAssetId;
  std::string mAssetType;
  std::string mTime;
  std::ostringstream mBody;
  Device *mAssetDevice;
  std::set<std::string> mLogOnce;
  
  // Timeout for reconnection attempts, given in milliseconds
  int mReconnectInterval;
  
private:
  /* Inherited and is run as part of the threaded_object */
  void thread();
};

#endif

