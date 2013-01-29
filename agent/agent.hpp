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

typedef std::vector<std::pair<std::string, std::string> > AssetChangeList;

class Agent : public server::http_1a
{
  class ParameterError
  {
  public:
    ParameterError(const std::string &aCode, const std::string &aMessage) 
    {
      mCode = aCode;
      mMessage = aMessage;
    }
    ParameterError(const ParameterError &aError) {
      mCode = aError.mCode;
      mMessage = aError.mMessage;
    }
    ParameterError &operator=(const ParameterError &aError) {
      mCode = aError.mCode;
      mMessage = aError.mMessage;
      return *this;
    }
    std::string mCode;
    std::string mMessage;
  };
  
public:
  /* Slowest frequency allowed */
  static const int SLOWEST_FREQ = 2147483646;
  
  /* Fastest frequency allowed */
  static const int FASTEST_FREQ = 0;
  
  /* Default count for sample query */
  static const unsigned int DEFAULT_COUNT = 100;
  
  /* Code to return when a parameter has no value */
  static const int NO_VALUE32 = -1;
  static const uint64_t NO_VALUE64 = UINT64_MAX;
  
  /* Code to return for no frequency specified */
  static const int NO_FREQ = -2;
  
  /* Code to return for no heartbeat specified */
  static const int NO_HB = 0;

  /* Code for no start value specified */
  static const uint64_t NO_START = NO_VALUE64;

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
  Adapter * addAdapter(const std::string& device,
                       const std::string& host,
                       const unsigned int port,
                       bool start = false,
                       int aLegacyTimeout = 600);
  
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
  bool addAsset(Device *aDevice, const std::string &aId, const std::string &aAsset,
                const std::string &aType,
                const std::string &aTime = "");
  
  bool updateAsset(Device *aDevice, const std::string &aId, AssetChangeList &aList,
                   const std::string &aTime);
  
  /* Message when adapter has connected and disconnected */
  void disconnected(Adapter *anAdapter, std::vector<Device*> aDevices);
  void connected(Adapter *anAdapter, std::vector<Device*> aDevices);
  
  DataItem * getDataItemByName(
    const std::string& device,
    const std::string& name
  );
  
  ComponentEvent *getFromBuffer(uint64_t aSeq) const { return (*mSlidingBuffer)[aSeq]; }
  uint64_t getSequence() const { return mSequence; }
  unsigned int getBufferSize() const { return mSlidingBufferSize; }
  unsigned int getMaxAssets() const { return mMaxAssets; }
  unsigned int getAssetCount() const { return mAssets.size(); }
  int getAssetCount(const std::string &aType) const { 
    return const_cast<std::map<std::string, int>& >(mAssetCounts)[aType];
  }
  uint64_t getFirstSequence() const {
    if (mSequence > mSlidingBufferSize)
      return mSequence - mSlidingBufferSize;
    else
      return 1;
  }

  // For testing...
  void setSequence(uint64_t aSeq) { mSequence = aSeq; }
  
  // Starting
  virtual void start();
  
  // Shutdown
  void clear();

  void registerFile(const std::string &aUri, const std::string &aPath);
  
  // PUT and POST handling
  void enablePut(bool aFlag = true) { mPutEnabled = aFlag; }
  bool isPutEnabled() { return mPutEnabled; }
  void allowPutFrom(const std::string &aHost) { mPutAllowedHosts.insert(aHost); }
  bool isPutAllowedFrom(const std::string &aHost) { return mPutAllowedHosts.count(aHost) > 0; }
  
  // For debugging
  void setLogStreamData(bool aLog) { mLogStreamData = aLog; }
    
  /* Handle probe calls */
  std::string handleProbe(const std::string& device);
  
  // Update DOM when key changes
  void updateDom(Device *aDevice);
  
protected:
  /* HTTP methods to handle the 3 basic calls */
  std::string handleCall(std::ostream& out,
                         const std::string& path,
                         const key_value_map& queries,
                         const std::string& call,
                         const std::string& device);
  
  /* HTTP methods to handle the 3 basic calls */
  std::string handlePut(std::ostream& out,
                        const std::string& path,
                        const key_value_map& queries,
                        const std::string& call,
                        const std::string& device);

  /* Handle stream calls, which includes both current and sample */
  std::string handleStream(std::ostream& out,
                           const std::string& path,
                           bool current,  
                           unsigned int frequency,
                           uint64_t start = 0,
                           unsigned int count = 0,
                           unsigned int aHb = 10000);
  
  /* Asset related methods */
  std::string handleAssets(std::ostream& aOut,
			   const key_value_map& aQueries,
			   const std::string& aList);
			   
  std::string storeAsset(std::ostream& aOut,
                         const key_value_map& aQueries,
                         const std::string& aAsset,
                         const std::string& aBody);
  
  /* Stream the data to the user */
  void streamData(std::ostream& out,
                  std::set<std::string> &aFilterSet,
                  bool current,
                  unsigned int frequency,
                  uint64_t start = 1,
                  unsigned int count = 0,
                  unsigned int aHb = 10000);
  
  /* Fetch the current/sample data and return the XML in a std::string */
  std::string fetchCurrentData(std::set<std::string> &aFilter, uint64_t at);
  std::string fetchSampleData(std::set<std::string> &aFilterSet,
                              uint64_t start, unsigned int count, uint64_t &end,
                              bool &endOfBuffer, ChangeObserver *aObserver = NULL);
  
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
    const key_value_map& queries,
    const std::string& param,
    const int defaultValue,
    const int minValue = NO_VALUE32,
    bool minError = false,
    const int maxValue = NO_VALUE32
  );
  
  /* Perform a check on parameter and return a value or a code */
  uint64_t checkAndGetParam64(
    const key_value_map& queries,
    const std::string& param,
    const uint64_t defaultValue,
    const uint64_t minValue = NO_VALUE64,
    bool minError = false,
    const uint64_t maxValue = NO_VALUE64
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
  uint64_t mSequence;
  
  /* The sliding/circular buffer to hold all of the events/sample data */
  dlib::sliding_buffer_kernel_1<ComponentEventPtr> *mSlidingBuffer;
  unsigned int mSlidingBufferSize;

  /* Asset storage, circ buffer stores ids */
  std::list<AssetPtr> mAssets;
  AssetIndex mAssetMap;
  
  // Natural key indices for assets
  std::map<std::string, AssetIndex> mAssetIndices;
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
  std::map<std::string, int> mAssetCounts; 

  // For file handling, small files will be cached
  std::map<std::string, std::string> mFileMap;
  std::map<std::string, std::string> mFileCache;
  
  // Put handling controls
  bool mPutEnabled;
  std::set<std::string> mPutAllowedHosts;
  
  // For debugging
  bool mLogStreamData;
};

#endif

