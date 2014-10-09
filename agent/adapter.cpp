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

#define __STDC_LIMIT_MACROS 1
#include "adapter.hpp"
#include "device.hpp"
#include "globals.hpp"
#include "dlib/logger.h"
#include <algorithm>

using namespace std;

static dlib::logger sLogger("input.adapter");

/* Adapter public methods */
Adapter::Adapter(const string& device,
                 const string& server,
                 const unsigned int port,
                 int aLegacyTimeout)
  : Connector(server, port, aLegacyTimeout), mDeviceName(device), mRunning(true),
    mDupCheck(false), mAutoAvailable(false), mIgnoreTimestamps(false), mRelativeTime(false), mConversionRequired(true),
    mUpcaseValue(true), mBaseTime(0), mBaseOffset(0), mParseTime(false), mGatheringAsset(false),
    mAssetDevice(NULL), mReconnectInterval(10 * 1000)
{
}

Adapter::~Adapter()
{
  if (mRunning) stop();
}

void Adapter::stop()
{
  // Will stop threaded object gracefully Adapter::thread()
  mRunning = false;
  close();
  wait();
}

void Adapter::setAgent(Agent &aAgent)
{
  mAgent = &aAgent;
  mDevice = mAgent->getDeviceByName(mDeviceName);
  mDevice->addAdapter(this);

  if (mDevice != NULL)
    mAllDevices.push_back(mDevice);
}

void Adapter::addDevice(string &aName)
{
  Device *dev = mAgent->getDeviceByName(aName);
  if (dev != NULL) {
    mAllDevices.push_back(dev);
    dev->addAdapter(this);
  }
}

inline static bool splitKey(string &key, string &dev) 
{
  size_t found = key.find_first_of(':');
  if (found == string::npos) {
    return false;
  } else {
    dev = key;
    dev.erase(found);
    key.erase(0, found + 1);
    return true;
  }
}

inline static void trim(std::string &str)
{
  size_t index = str.find_first_not_of(" \r\t");
  if (index != string::npos && index > 0)
    str.erase(0, index);
  index = str.find_last_not_of(" \r\t");
  if (index != string::npos)
    str.erase(index + 1);
}

inline string Adapter::extractTime(const string &time)
{
  // Check how to handle time. If the time is relative, then we need to compute the first
  // offsets, otherwise, if this function is being used as an API, add the current time.
  string result;
  if (mRelativeTime) {
    uint64_t offset;
    if (mBaseTime == 0) {
      mBaseTime = getCurrentTimeInMicros();
      
      if (time.find('T') != string::npos) {
        mParseTime = true;
        mBaseOffset = parseTimeMicro(time);
      } else {
        mBaseOffset = (uint64_t) (atof(time.c_str()) * 1000.0);
      }
      offset = 0;
    } else if (mParseTime) {
      offset = parseTimeMicro(time) - mBaseOffset;
    } else {
      offset = ((uint64_t) (atof(time.c_str()) * 1000.0)) - mBaseOffset;
    }
    result = getRelativeTimeString(mBaseTime + offset);
  } else if (mIgnoreTimestamps || time.empty()) {
    result = getCurrentTime(GMT_UV_SEC);
  }
  else
  {
    result = time;
  }
  
  return result;
}

/**
 * Expected data to parse in SDHR format:
 *   Time|Alarm|Code|NativeCode|Severity|State|Description
 *   Time|Item|Value
 *   Time|Item1|Value1|Item2|Value2...
 * 
 * Support for assets:
 *   Time|@ASSET@|id|type|<...>...</...>
 */

void Adapter::processData(const string& data)
{
  if (mGatheringAsset)
  {
    if (data == mTerminator)
    {
      mAgent->addAsset(mAssetDevice, mAssetId, mBody.str(), mAssetType, mTime);
      mGatheringAsset = false;
    }
    else
    {
      mBody << data << endl;
    }
    
    return;
  }
  
  istringstream toParse(data);
  string key, value;
  
  getline(toParse, key, '|');
  string time = extractTime(key);

  
  getline(toParse, key, '|');
  getline(toParse, value, '|');
  
  // Data item name has a @, it is an asset special prefix.
  if (key.find('@') != string::npos)
  {
    processAsset(toParse, key, value, time);
  }
  else
  {
    if (processDataItem(toParse, data, key, value, time, true))
    {
      // Look for more key->value pairings in the rest of the data
      while (getline(toParse, key, '|'))
      {
        value.clear();
        getline(toParse, value, '|');
        processDataItem(toParse, data, key, value, time);
      }
    }
  }
}

bool Adapter::processDataItem(istringstream &toParse, const string &aLine, const string &aKey, const string &aValue,
                              const string &aTime, bool aFirst)
{
  string dev, key = aKey;
  Device *device;
  DataItem *dataItem;
  bool more = true;
  if (splitKey(key, dev)) {
    device = mAgent->getDeviceByName(dev);
  } else {
    device = mDevice;
  }
  
  if (device != NULL) {
    dataItem = device->getDeviceDataItem(key);
    if (dataItem == NULL)
    {
      sLogger << LWARN << "(" << device->getName() << ") Could not find data item: " << key <<
      " from line '" << aLine << "'";
    }
    else
    {
      string rest, value;
      if (aFirst && (dataItem->isCondition() || dataItem->isAlarm() || dataItem->isMessage() ||
                     dataItem->isTimeSeries()))
      {
        getline(toParse, rest);
        value = aValue + "|" + rest;
        more = false;
      }
      else
      {
        if (mUpcaseValue)
        {
          value.resize(aValue.length());
          transform(aValue.begin(), aValue.end(), value.begin(), ::toupper);
        }
        else
          value = aValue;
      }
      
      dataItem->setDataSource(this);
      trim(value);
      if (!isDuplicate(dataItem, value))
      {
        mAgent->addToBuffer(dataItem, value, aTime);
      }
      else if (mDupCheck)
      {
        sLogger << LTRACE << "Dropping duplicate value for " << key << " of " << value;
      }
    }
  }
  else
  {
    sLogger << LDEBUG << "Could not find device: " << dev;
    // Continue on processing the rest of the fields. Assume key/value pairs...
  }
  
  return more;
}

void Adapter::processAsset(istringstream &toParse, const string &aKey, const string &value,
                           const string &time)
{
  Device *device;
  string key = aKey, dev;
  if (splitKey(key, dev)) {
    device = mAgent->getDeviceByName(dev);
  } else {
    device = mDevice;
  }

  string assetId;
  if (value[0] == '@')
    assetId = device->getUuid() + value.substr(1);
  else
    assetId = value;

  if (key == "@ASSET@") {
    string type, rest;
    getline(toParse, type, '|');
    getline(toParse, rest);
    
    // Chck for an update and parse key value pairs. If only a type
    // is presented, then assume the remainder is a complete doc.
    
    // if the rest of the line begins with --multiline--... then
    // set multiline and accumulate until a completed document is found
    if (rest.find("--multiline--") != rest.npos)
    {
      mAssetDevice = device;
      mGatheringAsset = true;
      mTerminator = rest;
      mTime = time;
      mAssetType = type;
      mAssetId = assetId;
      mBody.str("");
      mBody.clear();
    }
    else
    {
      mAgent->addAsset(device, assetId, rest, type, time);
    }
  }
  else if (key == "@UPDATE_ASSET@")
  {
    string assetKey, assetValue;
    AssetChangeList list;
    getline(toParse, assetKey, '|');
    if (assetKey[0] == '<')
    {
      do {
        pair<string,string> kv("xml", assetKey);
        list.push_back(kv);
      } while (getline(toParse, assetKey, '|'));
    }
    else
    {
      while (getline(toParse, assetValue, '|'))
      {
        pair<string,string> kv(assetKey, assetValue);
        list.push_back(kv);
        
        if (!getline(toParse, assetKey, '|'))
          break;
      }
    }
    
    mAgent->updateAsset(device, assetId, list, time);
  }
  else if (key == "@REMOVE_ASSET@")
  {
    mAgent->removeAsset(device, assetId, time);
  }
  else if (key == "@REMOVE_ALL_ASSETS@")
  {
    mAgent->removeAllAssets(device, value, time);
  }
  
}

static inline bool is_true(const string &aValue)
{
  return(aValue == "yes" || aValue == "true" || aValue == "1");
}

void Adapter::protocolCommand(const std::string& data)
{
  // Handle initial push of settings for uuid, serial number and manufacturer. 
  // This will override the settings in the device from the xml
  if (data == "* PROBE") {
    string response = mAgent->handleProbe(mDeviceName);
    string probe = "* PROBE LENGTH=";
    probe.append(intToString(response.length()));
    probe.append("\n");
    probe.append(response);
    probe.append("\n");
    mConnection->write(probe.c_str(), probe.length());
  } else {
    size_t index = data.find(':', 2);
    if (index != string::npos)
    {
      // Slice from the second character to the :, without the colon
      string key = data.substr(2, index - 2);
      trim(key);        
      string value = data.substr(index + 1);
      trim(value);
    
      bool updateDom = true;
      if (key == "uuid") {
        if (!mDevice->mPreserveUuid) mDevice->setUuid(value);
      } else if (key == "manufacturer")
        mDevice->setManufacturer(value);
      else if (key == "station")
        mDevice->setStation(value);
      else if (key == "serialNumber")
        mDevice->setSerialNumber(value);
      else if (key == "description")
        mDevice->setDescription(value);
      else if (key == "nativeName")
        mDevice->setNativeName(value);
      else if (key == "calibration")
        parseCalibration(value);
      else if (key == "conversionRequired")
        mConversionRequired = is_true(value);
      else if (key == "relativeTime")
        mRelativeTime = is_true(value);
      else if (key == "realTime")
        mRealTime = is_true(value);
      else
      {
        sLogger << LWARN << "Unknown command '" << data << "' for device '" << mDeviceName;
        updateDom = false;
      }
      
      if (updateDom) {
        mAgent->updateDom(mDevice);
      }
    }
  }  
}

void Adapter::parseCalibration(const std::string &aLine)
{
  istringstream toParse(aLine);

  // Look for name|factor|offset triples
  string name, factor, offset;
  while (getline(toParse, name, '|') &&
         getline(toParse, factor, '|') &&
         getline(toParse, offset, '|')) {
    // Convert to a floating point number
    DataItem *di = mDevice->getDeviceDataItem(name);
    if (di == NULL) {
      sLogger << LWARN << "Cannot find data item to calibrate for " << name;
    } else {
      double fact_value = strtod(factor.c_str(), NULL);
      double off_value = strtod(offset.c_str(), NULL);
      di->setConversionFactor(fact_value, off_value);
    }
  }
}

void Adapter::disconnected()
{
  mBaseTime = 0;
  mAgent->disconnected(this, mAllDevices);
}

void Adapter::connected()
{
  mAgent->connected(this, mAllDevices);
}

/* Adapter private methods */
void Adapter::thread()
{
  while (mRunning)
  {
    try
    {
      // Start the connection to the socket
      connect();
      
      // make sure we're closed...
      close();
    }
    catch (...)
    {
      sLogger << LERROR << "Thread for adapter " << mDeviceName << "'s thread threw an unhandled exception";
    }

    if (!mRunning) break;

    // Try to reconnect every 10 seconds
    sLogger << LINFO << "Will try to reconnect in " << mReconnectInterval << " milliseconds";
    dlib::sleep(mReconnectInterval);
  }
  sLogger << LINFO << "Adapter thread stopped";
}

