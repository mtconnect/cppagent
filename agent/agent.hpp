/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this std::list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this std::list of conditions and the following disclaimer in the
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

#ifndef AGENT_HPP
#define AGENT_HPP

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>

#include "dlib/md5.h"
#include "dlib/server.h"
#include "dlib/sliding_buffer.h"

#include "adapter.hpp"
#include "globals.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"
#include "checkpoint.hpp"
#include "service.hpp"
#include "asset.hpp"

class Adapter;
class ComponentEvent;
class DataItem;
class Device;

using namespace dlib;

class Agent : public server::http_1a
{
public:
  /* Slowest frequency allowed */
  static const int SLOWEST_FREQ = 2147483646;
  
  /* Fastest frequency allowed */
  static const int FASTEST_FREQ = 100;
  
  /* Default count for sample query */
  static const unsigned int DEFAULT_COUNT = 100;
  
  /* Error code to return for a parameter error */
  static const int PARAM_ERROR = -1;
  
  /* Code to return when a parameter has no value */
  static const int NO_VALUE = -1;
  
  /* Code to return for no frequency specified */
  static const int NO_FREQ = 0;
  
  /* Code for no start value specified */
  static const int NO_START = -2;

  /* Small file size */
  static const int SMALL_FILE = 10 * 1024; // 10k is considered small
  
public:
  /* Load agent with the xml configuration */
  Agent(const std::string& configXmlPath, int aBufferSize, int aMaxAssets,
        int aCheckpointFreq = 1000);
  
  /* Virtual destructor */
  virtual ~Agent();
  
  /* Overridden method that is called per web request */  
  virtual const std::string on_request (
    const incoming_things& incoming,
    outgoing_things& outgoing
    );
  
  /* Add an adapter to the agent */
  Adapter * addAdapter(
    const std::string& device,
    const std::string& host,
    const unsigned int port,
    bool start = false
  );
  
  /* Get device from device map */
  Device * getDeviceByName(const std::string& name) { return mDeviceMap[name]; }
  const std::vector<Device *> &getDevices() { return mDevices; }
  
  /* Add component events to the sliding buffer */
  unsigned int addToBuffer(
    DataItem *dataItem,
    const std::string& value,
    std::string time = ""
  );
  
  // Add an asset to the agent
  void addAsset(Device *aDevice, const std::string &aId, const std::string &aAsset,
                const std::string &aType,
                const std::string &aTime = "");
  
  /* Message when adapter has connected and disconnected */
  void disconnected(Adapter *anAdapter, std::vector<Device*> aDevices);
  void connected(Adapter *anAdapter, std::vector<Device*> aDevices);
  
  DataItem * getDataItemByName(
    const std::string& device,
    const std::string& name
  );
  
  ComponentEvent *getFromBuffer(Int64 aSeq) const { return (*mSlidingBuffer)[aSeq]; }
  Int64 getSequence() const { return mSequence; }
  unsigned int getBufferSize() const { return mSlidingBufferSize; }
  unsigned int getMaxAssets() const { return mMaxAssets; }
  unsigned int getAssetCount() const { return mAssets.size(); }
  Int64 getFirstSequence() const {
    if (mSequence > mSlidingBufferSize)
      return mSequence - mSlidingBufferSize;
    else
      return 1;
  }

  // For testing...
  void setSequence(Int64 aSeq) { mSequence = aSeq; }
  
  // Starting
  virtual void start();

  void registerFile(const std::string &aUri, const std::string &aPath);
  
  // PUT and POST handling
  void enablePut(bool aFlag = true) { mPutEnabled = aFlag; }
  bool isPutEnabled() { return mPutEnabled; }
  void allowPutFrom(const std::string &aHost) { mPutAllowedHosts.insert(aHost); }
  bool isPutAllowedFrom(const std::string &aHost) { return mPutAllowedHosts.count(aHost) > 0; }
    
protected:
  /* HTTP methods to handle the 3 basic calls */
  std::string handleCall(
    std::ostream& out,
    const std::string& path,
    const key_value_map& queries,
    const std::string& call,
    const std::string& device
  );
  
  /* HTTP methods to handle the 3 basic calls */
  std::string handlePut(
    std::ostream& out,
    const std::string& path,
    const key_value_map& queries,
    const std::string& call,
    const std::string& device
  );

  /* Handle probe calls */
  std::string handleProbe(const std::string& device);
  
  /* Handle stream calls, which includes both current and sample */
  std::string handleStream(
    std::ostream& out,
    const std::string& path,
    bool current,  
    unsigned int frequency,
    Int64 start = 0,
    unsigned int count = 0
  );

  /* Asset related methods */
  std::string handleAssets(std::ostream& aOut,
			   const key_value_map& aQueries,
			   const std::string& aList);
			   
  std::string storeAsset(std::ostream& aOut,
                         const key_value_map& aQueries,
                         const std::string& aAsset,
                         const std::string& aBody);
  
  /* Stream the data to the user */
  void streamData(
    std::ostream& out,
    std::set<std::string> &aFilterSet,
    bool current,
    unsigned int frequency,
    Int64 start = 1,
    unsigned int count = 0
  );
  
  /* Fetch the current/sample data and return the XML in a std::string */
  std::string fetchCurrentData(std::set<std::string> &aFilter, Int64 at);
  std::string fetchSampleData(
    std::set<std::string> &aFilterSet,
    Int64 start,
    unsigned int count,
    unsigned int &items
  );
  
  /* Output an XML Error */
  std::string printError(const std::string& errorCode, const std::string& text);
  
  /* Handle the device/path parameters for the xpath search */
  std::string devicesAndPath(
    const std::string& path,
    const std::string& device
  );

  /* Get a file */
  std::string handleFile(const std::string& aUri, outgoing_things& aOutgoing);

  bool isFile(const std::string& aUri) { return mFileMap.count(aUri) > 0; }
  
  /* Perform a check on parameter and return a value or a code */
  int checkAndGetParam(
    std::string& result,
    const key_value_map& queries,
    const std::string& param,
    const int defaultValue,
    const int minValue = NO_VALUE,
    bool minError = false,
    const int maxValue = NO_VALUE
  );
  
  /* Perform a check on parameter and return a value or a code */
  Int64 checkAndGetParam64(
    std::string& result,
    const key_value_map& queries,
    const std::string& param,
    const Int64 defaultValue,
    const Int64 minValue = NO_VALUE,
    bool minError = false,
    const Int64 maxValue = NO_VALUE
  );
  
  /* Find data items by name/id */
  DataItem * getDataItemById(const std::string& id) { return mDataItemMap[id]; }
  
protected:
  /* Unique id based on the time of creation */
  unsigned int mInstanceId;
  
  /* Pointer to the configuration file for node access */
  XmlParser *mXmlParser;
  
  /* For access to the sequence number and sliding buffer, use the mutex */
  dlib::mutex *mSequenceLock;
  dlib::mutex *mAssetLock;
  
  /* Sequence number */
  Int64 mSequence;
  
  /* The sliding/circular buffer to hold all of the events/sample data */
  dlib::sliding_buffer_kernel_1<ComponentEventPtr> *mSlidingBuffer;
  unsigned int mSlidingBufferSize;

  /* Asset storage, circ buffer stores ids */
  std::list<AssetPtr> mAssets;
  std::map<std::string, AssetPtr> mAssetMap;
  unsigned int mMaxAssets;  
  
  /* Checkpoints */
  Checkpoint mLatest;
  Checkpoint mFirst;
  Checkpoint *mCheckpoints;

  int mCheckpointFreq, mCheckpointCount;
  
  /* Data containers */
  std::vector<Adapter *> mAdapters;
  std::vector<Device *> mDevices;
  std::map<std::string, Device *> mDeviceMap;
  std::map<std::string, DataItem *> mDataItemMap;

  // For file handling, small files will be cached
  std::map<std::string, std::string> mFileMap;
  std::map<std::string, std::string> mFileCache;
  
  // Put handling controls
  bool mPutEnabled;
  std::set<std::string> mPutAllowedHosts;
};

#endif

