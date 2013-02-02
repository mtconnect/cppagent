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

using namespace std;

static dlib::logger sLogger("input.adapter");

/* Adapter public methods */
Adapter::Adapter(const string& device,
                 const string& server,
                 const unsigned int port,
                 int aLegacyTimeout)
  : Connector(server, port, aLegacyTimeout), mDeviceName(device), mRunning(true),
    mDupCheck(false), mAutoAvailable(false), mIgnoreTimestamps(false), mRelativeTime(false),
    mBaseTime(0), mBaseOffset(0), mParseTime(false), mGatheringAsset(false),  mReconnectInterval(10 * 1000)
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
  string key, value, dev;
  Device *device;
  
  getline(toParse, key, '|');
  string time = key;

  // Check how to handle time. If the time is relative, then we need to compute the first
  // offsets, otherwise, if this function is being used as an API, add the current time.
  if (mRelativeTime) {
    uint64_t offset;
    if (mBaseTime == 0) {
      mBaseTime = getCurrentTimeInMicros();

      if (time.find('T') != string::npos) {
        mParseTime = true;
        mBaseOffset = parseTimeMicro(time);
      } else {
        mBaseOffset = strtoull(time.c_str(), 0, 10);
      }
      offset = 0;
    } else if (mParseTime) {
      offset = parseTimeMicro(time) - mBaseOffset;
    } else {
      offset = (strtoull(time.c_str(), 0, 10) - mBaseOffset) * 1000;
    }
    time = getRelativeTimeString(mBaseTime + offset);
  } else if (mIgnoreTimestamps || time.empty()) {
    time = getCurrentTime(GMT_UV_SEC);
  }
  
  getline(toParse, key, '|');
  getline(toParse, value, '|');
  
  if (splitKey(key, dev)) {
    device = mAgent->getDeviceByName(dev);
  } else {
    device = mDevice;
  }

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
      mAssetId = value;
      mBody.str("");
      mBody.clear();
    }
    else
    {
      mAgent->addAsset(device, value, rest, type, time);
    }
    
    return;
  } 
  else if (key == "@UPDATE_ASSET@")
  {
    string assetId = value;
    AssetChangeList list;
    getline(toParse, key, '|');
    if (key[0] == '<')
    {
      do {
        pair<string,string> kv("xml", key);
        list.push_back(kv);        
      } while (getline(toParse, key, '|'));
      
    } 
    else
    {
      while (getline(toParse, value, '|'))
      {
        pair<string,string> kv(key, value);
        list.push_back(kv);      
        
        if (!getline(toParse, key, '|'))
          break;
      } 
    }
    mAgent->updateAsset(device, assetId, list, time);
    return;
  }

    
  DataItem *dataItem;
  if (device != NULL) {
    dataItem = device->getDeviceDataItem(key);    
  
    if (dataItem == NULL)
    {
      sLogger << LWARN << "(" << device->getName() << ") Could not find data item: " << key <<
        " from line '" << data << "'";
    } else {
      string rest;
      if (dataItem->isCondition() || dataItem->isAlarm() || dataItem->isMessage() ||
          dataItem->isTimeSeries())
      {
        getline(toParse, rest);
        value = value + "|" + rest;
      }

      // Add key->value pairings
      dataItem->setDataSource(this);
      trim(value);
            
      // Check for duplication
      if (!mDupCheck || !dataItem->isDuplicate(value)) 
      {
        mAgent->addToBuffer(dataItem, toUpperCase(value), time);
      } 
      else if (mDupCheck)
      {
        //sLogger << LDEBUG << "Dropping duplicate value for " << key << " of " << value;
      }
    }
  } else {
    sLogger << LDEBUG << "Could not find device: " << dev;
  }
  
  // Look for more key->value pairings in the rest of the data
  while (getline(toParse, key, '|') && getline(toParse, value, '|'))
  {
    if (splitKey(key, dev)) {
      device = mAgent->getDeviceByName(dev);
    } else {
      device = mDevice;
    }
    if (device == NULL) {
      sLogger << LDEBUG << "Could not find device: " << dev;
      continue;
    }
    
    dataItem = device->getDeviceDataItem(key);    
    if (dataItem == NULL)
    {
      sLogger << LWARN << "Could not find data item: " << key << " for device " << mDeviceName;
    }
    else
    {
      dataItem->setDataSource(this);
      trim(value);
      if (!mDupCheck || !dataItem->isDuplicate(value)) 
      {
        mAgent->addToBuffer(dataItem, toUpperCase(value), time);
      } 
      else if (mDupCheck)
      {
        //sLogger << LDEBUG << "Dropping duplicate value for " << key << " of " << value;
      }
    }
  }
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
      else {
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

