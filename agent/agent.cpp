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

#include "agent.hpp"
#include "dlib/logger.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <dlib/tokenizer.h>
#include <dlib/misc_api.h>
#include <dlib/array.h>
#include <dlib/dir_nav.h>
#include <dlib/config_reader.h>
#include <dlib/queue.h>
#include <functional>

using namespace std;

static const string sUnavailable("UNAVAILABLE");
static const string sConditionUnavailable("UNAVAILABLE|||");

static const string sAvailable("AVAILABLE");
static dlib::logger sLogger("agent");

/* Agent public methods */
Agent::Agent(const string& configXmlPath, int aBufferSize, int aMaxAssets, int aCheckpointFreq)
  : mPutEnabled(false), mLogStreamData(false)
{
  mMimeTypes["xsl"] = "text/xsl";
  mMimeTypes["xml"] = "text/xml";
  mMimeTypes["css"] = "text/css";
  mMimeTypes["xsd"] = "text/xml";
  mMimeTypes["jpg"] = "image/jpeg";
  mMimeTypes["jpeg"] = "image/jpeg";
  mMimeTypes["png"] = "image/png";
  mMimeTypes["ico"] = "image/x-icon";
  
  try
  {
    // Load the configuration for the Agent
    mXmlParser = new XmlParser();
    mDevices = mXmlParser->parseFile(configXmlPath);  
    std::vector<Device *>::iterator device;
    std::set<std::string> uuids;
    for (device = mDevices.begin(); device != mDevices.end(); ++device) 
    {
      if (uuids.count((*device)->getUuid()) > 0)
          throw runtime_error("Duplicate UUID: " + (*device)->getUuid());
      
      uuids.insert((*device)->getUuid());
      (*device)->resolveReferences();
    }
  }
  catch (runtime_error & e)
  {
    sLogger << LFATAL << "Error loading xml configuration: " + configXmlPath;
    sLogger << LFATAL << "Error detail: " << e.what();
    cerr << e.what() << endl;
    throw e;
  }
  catch (exception &f)
  {
    sLogger << LFATAL << "Error loading xml configuration: " + configXmlPath;
    sLogger << LFATAL << "Error detail: " << f.what();
    cerr << f.what() << endl;
    throw f;    
  }
  
  // Grab data from configuration
  string time = getCurrentTime(GMT_UV_SEC);
  
  // Unique id number for agent instance
  mInstanceId = getCurrentTimeInSec();
  
  // Sequence number and sliding buffer for data
  mSequence = 1;
  mSlidingBufferSize = 1 << aBufferSize;
  mSlidingBuffer = new sliding_buffer_kernel_1<ComponentEventPtr>();
  mSlidingBuffer->set_size(aBufferSize);
  mCheckpointFreq = aCheckpointFreq;
  mCheckpointCount = (mSlidingBufferSize / aCheckpointFreq) + 1;
  
  // Asset sliding buffer
  mMaxAssets = aMaxAssets;
  
  // Create the checkpoints at a regular frequency
  mCheckpoints = new Checkpoint[mCheckpointCount];
  
  // Mutex used for synchronized access to sliding buffer and sequence number
  mSequenceLock = new dlib::mutex;
  mAssetLock = new dlib::mutex;
  
  // Add the devices to the device map and create availability and 
  // asset changed events if they don't exist
  std::vector<Device *>::iterator device;
  for (device = mDevices.begin(); device != mDevices.end(); ++device) 
  {
    mDeviceMap[(*device)->getName()] = *device;
    
    // Make sure we have two device level data items:
    // 1. Availability
    // 2. AssetChanged
    if ((*device)->getAvailability() == NULL)
    {
      // Create availability data item and add it to the device.
      std::map<string,string> attrs;
      attrs["type"] = "AVAILABILITY";
      attrs["id"] = (*device)->getId() + "_avail";
      attrs["category"] = "EVENT";
      
      DataItem *di = new DataItem(attrs);
      di->setComponent(*(*device));
      (*device)->addDataItem(*di);
      (*device)->addDeviceDataItem(*di);
      (*device)->mAvailabilityAdded = true;
    }
    
    int major, minor;
    char c;
    stringstream ss(XmlPrinter::getSchemaVersion());
    ss >> major >> c >> minor;
    if ((*device)->getAssetChanged() == NULL && (major > 1 || (major == 1 && minor >= 2)))
    {
      // Create asset change data item and add it to the device.
      std::map<string,string> attrs;
      attrs["type"] = "ASSET_CHANGED";
      attrs["id"] = (*device)->getId() + "_asset_chg";
      attrs["category"] = "EVENT";
      
      DataItem *di = new DataItem(attrs);
      di->setComponent(*(*device));
      (*device)->addDataItem(*di);
      (*device)->addDeviceDataItem(*di);
    }
  
    if ((*device)->getAssetRemoved() == NULL && (major > 1 || (major == 1 && minor >= 3)))
    {
      // Create asset removed data item and add it to the device.
      std::map<string,string> attrs;
      attrs["type"] = "ASSET_REMOVED";
      attrs["id"] = (*device)->getId() + "_asset_rem";
      attrs["category"] = "EVENT";
      
      DataItem *di = new DataItem(attrs);
      di->setComponent(*(*device));
      (*device)->addDataItem(*di);
      (*device)->addDeviceDataItem(*di);
    }
  }
  
  // Reload the document for path resolution
  mXmlParser->loadDocument(XmlPrinter::printProbe(mInstanceId, mSlidingBufferSize, 
                                                  mMaxAssets,
                                                  mAssets.size(),
                                                  mSequence, mDevices));
   
  /* Initialize the id mapping for the devices and set all data items to UNAVAILABLE */
  for (device = mDevices.begin(); device != mDevices.end(); ++device) 
  {
    const std::map<string, DataItem*> &items = (*device)->getDeviceDataItems();
    std::map<string, DataItem *>::const_iterator item;

    for (item = items.begin(); item != items.end(); ++item)
    {
      // Check for single valued constrained data items.
      DataItem *d = item->second;
      const string *value = &sUnavailable;
      if (d->isCondition()) {
        value = &sConditionUnavailable;
      } else if (d->hasConstraints()) { 
        std::vector<std::string> &values = d->getConstrainedValues();
        if (values.size() == 1)
          value = &values[0];
      }
      
      addToBuffer(d, *value, time);
      if (mDataItemMap.count(d->getId()) == 0)
        mDataItemMap[d->getId()] = d;
      else {
        sLogger << LFATAL << "Duplicate DataItem id " << d->getId() <<
                  " for device: " << (*device)->getName() << " and data item name: " <<
                  d->getName();
        exit(1);
      }
    }
  }
}

Device *Agent::findDeviceByUUIDorName(const std::string& aId)
{
  Device *device = NULL;
  
  std::vector<Device *>::iterator it;
  for (it = mDevices.begin(); device == NULL && it != mDevices.end(); it++)
  {
    if ((*it)->getUuid() == aId || (*it)->getName() == aId)
      device = *it;
  }
  
  return device;
}


Agent::~Agent()
{
  delete mSlidingBuffer;
  delete mSequenceLock;
  delete mAssetLock;
  delete mXmlParser;
  delete[] mCheckpoints;
}

void Agent::start()
{
  try {
    // Start all the adapters
    std::vector<Adapter*>::iterator iter;
    for (iter = mAdapters.begin(); iter != mAdapters.end(); iter++) {
      (*iter)->start();
    }
    
    // Start the server. This blocks until the server stops.
    server_http::start();
  }
  catch (dlib::socket_error &e) {
    sLogger << LFATAL << "Cannot start server: " << e.what();
    exit(1);
  }
}

void Agent::clear()
{
  // Stop all adapter threads...
  std::vector<Adapter *>::iterator iter;
  
  sLogger << LINFO << "Shutting down adapters";
  // Deletes adapter and waits for it to exit.
  for (iter = mAdapters.begin(); iter != mAdapters.end(); iter++) {
    (*iter)->stop();
  }
    
  sLogger << LINFO << "Shutting down server";
  server::http_1a::clear();
  sLogger << LINFO << "Shutting completed";

  for (iter = mAdapters.begin(); iter != mAdapters.end(); iter++) {
    delete (*iter);
  }
  
  mAdapters.clear();

}

// Register a file
void Agent::registerFile(const string &aUri, const string &aPath)
{
  try {
    directory dir(aPath);
    queue<file>::kernel_1a files;
    dir.get_files(files);
    files.reset();
    string baseUri = aUri;
    if (*baseUri.rbegin() != '/') baseUri.append(1, '/');
    while (files.move_next()) {
      file &file = files.element();
      string name = file.name();
      string uri = baseUri + name;
      mFileMap.insert(pair<string,string>(uri, file.full_name()));
      
      // Check if the file name maps to a standard MTConnect schema file.
      if (name.find("MTConnect") == 0 && name.substr(name.length() - 4, 4) == ".xsd" &&
          XmlPrinter::getSchemaVersion() == name.substr(name.length() - 7, 3)) {
        string version = name.substr(name.length() - 7, 3);
        if (name.substr(9, 5) == "Error") {
          string urn = "urn:mtconnect.org:MTConnectError:" + XmlPrinter::getSchemaVersion();
          XmlPrinter::addErrorNamespace(urn, uri, "m");
        } else if (name.substr(9, 7) == "Devices") {
          string urn = "urn:mtconnect.org:MTConnectDevices:" + XmlPrinter::getSchemaVersion();
          XmlPrinter::addDevicesNamespace(urn, uri, "m");
        } else if (name.substr(9, 6) == "Assets") {
          string urn = "urn:mtconnect.org:MTConnectAssets:" + XmlPrinter::getSchemaVersion();
          XmlPrinter::addAssetsNamespace(urn, uri, "m");
        } else if (name.substr(9, 7) == "Streams") {
          string urn = "urn:mtconnect.org:MTConnectStreams:" + XmlPrinter::getSchemaVersion();
          XmlPrinter::addStreamsNamespace(urn, uri, "m");
        }      
      }
    }
  }
  catch (directory::dir_not_found e) {
    sLogger << LDEBUG << "registerFile: Path " << aPath << " is not a directory: "
            << e.what() << ", trying as a file";
    try {
      file file(aPath);
      mFileMap.insert(pair<string,string>(aUri, aPath));
    } catch (file::file_not_found e) {
      sLogger << LERROR << "Cannot register file: " << aPath << ": " << e.what();
    }
    
  }
}

// Methods for service
const string Agent::on_request (const incoming_things& incoming,
                                outgoing_things& outgoing)
{
  string result;
  outgoing.headers["Content-Type"] = "text/xml";
  try 
  {
    sLogger << LDEBUG << "Request: " << incoming.request_type << " " << 
      incoming.path << " from " << incoming.foreign_ip << ":" << incoming.foreign_port;
    
    if (mPutEnabled)
    {
      if ((incoming.request_type == "PUT" || incoming.request_type == "POST") && 
          !mPutAllowedHosts.empty() && mPutAllowedHosts.count(incoming.foreign_ip) == 0)
      {
        return printError("UNSUPPORTED",
                          "HTTP PUT is not allowed from " + incoming.foreign_ip);        
      }
      
      if (incoming.request_type != "GET" && incoming.request_type != "PUT" &&
          incoming.request_type != "POST") {
        return printError("UNSUPPORTED",
                          "Only the HTTP GET and PUT requests are supported");
      }
    }
    else
    {
      if (incoming.request_type != "GET") {
        return printError("UNSUPPORTED",
                          "Only the HTTP GET request is supported");
      }      
    }
    
    // Parse the URL path looking for '/'
    string path = incoming.path;
    size_t qm = path.find_last_of('?');
    if (qm != string::npos)
      path = path.substr(0, qm);
    
    if (isFile(path)) {
      return handleFile(path, outgoing);
    }
    
    string::size_type loc1 = path.find("/", 1);
    string::size_type end = (path[path.length()-1] == '/') ?
                              path.length() - 1 : string::npos;
    
    string first =  path.substr(1, loc1-1);
    string call, device;
    
    if (first == "assets" || first == "asset")
    {
      string list;
      if (loc1 != string::npos)
        list = path.substr(loc1 + 1);
      if (incoming.request_type == "GET")
        result = handleAssets(*outgoing.out, incoming.queries, list);
      else
        result = storeAsset(*outgoing.out, incoming.queries, list, incoming.body);
    }
    else
    {
      // If a '/' was found
      if (loc1 < end)
      {
        // Look for another '/'
        string::size_type loc2 = path.find("/", loc1+1);
        
        if (loc2 == end)
        {
          device = first;
          call = path.substr(loc1+1, loc2-loc1-1);
        }
        else
        {
          // Path is too long
          return printError("UNSUPPORTED", "The following path is invalid: " + path);
        }
      }
      else
      {
        // Try to handle the call
        call = first;
      }
      
      if (incoming.request_type == "GET")
        result = handleCall(*outgoing.out, path, incoming.queries, call, device);
      else
        result = handlePut(*outgoing.out, path, incoming.queries, call, device);
    }
  }
  catch (exception & e)
  {
    printError("SERVER_EXCEPTION",(string) e.what()); 
  }
  
  return result;
}

Adapter * Agent::addAdapter(const string& aDeviceName,
                            const string& aHost,
                            const unsigned int aPort,
                            bool aStart,
                            int aLegacyTimeout
                            )
{
  Adapter *adapter = new Adapter(aDeviceName, aHost, aPort, aLegacyTimeout);
  adapter->setAgent(*this);
  mAdapters.push_back(adapter);
  
  Device *dev = mDeviceMap[aDeviceName];
  if (dev != NULL && dev->mAvailabilityAdded)
    adapter->setAutoAvailable(true);
  
  if (aStart)
    adapter->start();
  
  return adapter;
}

unsigned int Agent::addToBuffer(DataItem *dataItem,
                                const string& value,
                                string time
                                )
{
  if (dataItem == NULL) return 0;
  
  dlib::auto_mutex lock(*mSequenceLock);
  
  uint64_t seqNum = mSequence++;
  ComponentEvent *event = new ComponentEvent(*dataItem, seqNum,
                                             time, value);
  (*mSlidingBuffer)[seqNum] = event;
  mLatest.addComponentEvent(event);
  event->unrefer();
  
  // Special case for the first event in the series to prime the first checkpoint.
  if (seqNum == 1) {
    mFirst.addComponentEvent(event);
  }
  
  // Checkpoint management
  int index = mSlidingBuffer->get_element_id(seqNum);
  if (mCheckpointCount > 0 && index % mCheckpointFreq == 0) {
    // Copy the checkpoint from the current into the slot
    mCheckpoints[index / mCheckpointFreq].copy(mLatest);
  }
  
  
  // See if the next sequence has an event. If the event exists it
  // should be added to the first checkpoint.
  if ((*mSlidingBuffer)[mSequence] != NULL)
  {
    // Keep the last checkpoint up to date with the last.
    mFirst.addComponentEvent((*mSlidingBuffer)[mSequence]);
  }
  
  dataItem->signalObservers(seqNum);
  
  return seqNum;
}

bool Agent::addAsset(Device *aDevice, const string &aId, const string &aAsset,
                     const string &aType, const string &aTime)
{
  // Check to make sure the values are present
  if (aType.empty() || aAsset.empty() || aId.empty()) {
    sLogger << LWARN << "Asset '" << aId << "' missing required type, id, or body. Asset is rejected.";
    return false;
  }
  
  string time;
  if (aTime.empty())
    time = getCurrentTime(GMT_UV_SEC);
  else
    time = aTime;

  AssetPtr ptr;
  
  // Lock the asset addition to protect from multithreaded collisions. Releaes
  // before we add the event so we don't cause a race condition.
  {
    dlib::auto_mutex lock(*mAssetLock);
    
    try {
      ptr = mXmlParser->parseAsset(aId, aType, aAsset);
    }
    catch (runtime_error &e) {
      sLogger << LERROR << "addAsset: Error parsing asset: " << aAsset << "\n" << e.what();
      return false;
    }
    
    if (ptr.getObject() == NULL)
    {
      sLogger << LERROR << "addAssete: Error parsing asset";
      return false;
    }

    AssetPtr *old = &mAssetMap[aId];
    if (!ptr->isRemoved())
    {
      if (old->getObject() != NULL)
        mAssets.remove(old);
      else
        mAssetCounts[aType] += 1;
    } else if (old->getObject() == NULL) {
      sLogger << LWARN << "Cannot remove non-existent asset";
      return false;
    }
    
    if (ptr.getObject() == NULL) {
      sLogger << LWARN << "Asset could not be created";
      return false;
    } else {
      ptr->setAssetId(aId);
      ptr->setTimestamp(time);
      ptr->setDeviceUuid(aDevice->getUuid());
    }
    
    // Check for overflow
    if (mAssets.size() >= mMaxAssets)
    {
      AssetPtr oldref(*mAssets.front());
      mAssetCounts[oldref->getType()] -= 1;
      mAssets.pop_front();
      mAssetMap.erase(oldref->getAssetId());
      
      // Add secondary keys
      AssetKeys &keys = oldref->getKeys();
      AssetKeys::iterator iter;
      for (iter = keys.begin(); iter != keys.end(); iter++)
      {
        AssetIndex &index = mAssetIndices[iter->first];
        index.erase(iter->second);
      }
    }
    
    mAssetMap[aId] = ptr;
    if (!ptr->isRemoved())
    {
      AssetPtr &newPtr = mAssetMap[aId];
      mAssets.push_back(&newPtr);
    }
    
    // Add secondary keys
    AssetKeys &keys = ptr->getKeys();
    AssetKeys::iterator iter;
    for (iter = keys.begin(); iter != keys.end(); iter++)
    {
      AssetIndex &index = mAssetIndices[iter->first];
      index[iter->second] = ptr;
    }
  }
  
  // Generate an asset chnaged event.
  if (ptr->isRemoved())
    addToBuffer(aDevice->getAssetRemoved(), aType + "|" + aId, time);
  else
    addToBuffer(aDevice->getAssetChanged(), aType + "|" + aId, time);
  
  return true;
}

bool Agent::updateAsset(Device *aDevice, const std::string &aId, AssetChangeList &aList,
                        const string &aTime)
{
  AssetPtr asset;
  string time;
  if (aTime.empty())
    time = getCurrentTime(GMT_UV_SEC);
  else
    time = aTime;
  
  {
    dlib::auto_mutex lock(*mAssetLock);
    
    asset = mAssetMap[aId];
    if (asset.getObject() == NULL)
      return false;
    
    if (asset->getType() != "CuttingTool" && asset->getType() != "CuttingToolArchitype")
      return false;
    
    CuttingToolPtr tool((CuttingTool*) asset.getObject());
    
    try {
      AssetChangeList::iterator iter;
      for (iter = aList.begin(); iter != aList.end(); ++iter)
      {
        if (iter->first == "xml") {
          mXmlParser->updateAsset(asset, asset->getType(), iter->second);        
        } else {
          tool->updateValue(iter->first, iter->second);
        }        
      }
    }
    catch (runtime_error &e) {
      sLogger << LERROR << "updateAsset: Error parsing asset: " << asset << "\n" << e.what();
      return false;
    }
    
    // Move it to the front of the queue
    mAssets.remove(&asset);
    mAssets.push_back(&asset);
    
    tool->setTimestamp(time);
    tool->setDeviceUuid(aDevice->getUuid());
    tool->changed();
  }
  
  addToBuffer(aDevice->getAssetChanged(), asset->getType() + "|" + aId, time);

  return true;
}

bool Agent::removeAsset(Device *aDevice, const std::string &aId, const string &aTime)
{
  AssetPtr asset;

  string time;
  if (aTime.empty())
    time = getCurrentTime(GMT_UV_SEC);
  else
    time = aTime;
  
  {
    dlib::auto_mutex lock(*mAssetLock);
    
    asset = mAssetMap[aId];
    if (asset.getObject() == NULL)
      return false;
    
    asset->setRemoved(true);
    asset->setTimestamp(time);
    
    // Check if the asset changed id is the same as this asset.
    ComponentEventPtr *ptr = mLatest.getEventPtr(aDevice->getAssetChanged()->getId());
    if (ptr != NULL && (*ptr)->getValue() == aId)
    {
      addToBuffer(aDevice->getAssetChanged(), asset->getType() + "|UNAVAILABLE", time);
    }
  }
  
  addToBuffer(aDevice->getAssetRemoved(), asset->getType() + "|" + aId, time);
  
  return true;
}

bool Agent::removeAllAssets(Device *aDevice, const std::string &aType, const std::string &aTime)
{
  string time;
  if (aTime.empty())
    time = getCurrentTime(GMT_UV_SEC);
  else
    time = aTime;
  
  {
    dlib::auto_mutex lock(*mAssetLock);
    
    ComponentEventPtr *ptr = mLatest.getEventPtr(aDevice->getAssetChanged()->getId());
    string changedId;
    if (ptr != NULL)
      changedId = (*ptr)->getValue();
    
    list<AssetPtr*>::reverse_iterator iter;
    for (iter = mAssets.rbegin(); iter != mAssets.rend(); ++iter)
    {
      AssetPtr asset = (**iter);
      if (aType == asset->getType() && !asset->isRemoved()) {
        asset->setRemoved(true);
        asset->setTimestamp(time);
        
        addToBuffer(aDevice->getAssetRemoved(), asset->getType() + "|" + asset->getAssetId(), time);
        
        if (changedId == asset->getAssetId())
          addToBuffer(aDevice->getAssetChanged(), asset->getType() + "|UNAVAILABLE", time);
      }
    }
  }
  
  return true;
}

/* Add values for related data items UNAVAILABLE */
void Agent::disconnected(Adapter *anAdapter, std::vector<Device*> aDevices)
{
  string time = getCurrentTime(GMT_UV_SEC);
  sLogger << LDEBUG << "Disconnected from adapter, setting all values to UNAVAILABLE";
  
  std::vector<Device*>::iterator iter;
  for (iter = aDevices.begin(); iter != aDevices.end(); ++iter) {
    std::map<std::string, DataItem *> dataItems = (*iter)->getDeviceDataItems();
    std::map<std::string, DataItem*>::iterator dataItemAssoc;
    for (dataItemAssoc = dataItems.begin(); dataItemAssoc != dataItems.end(); ++dataItemAssoc)
    {
      DataItem *dataItem = (*dataItemAssoc).second;
      if (dataItem != NULL && (dataItem->getDataSource() == anAdapter ||
                               (anAdapter->isAutoAvailable() &&
                                dataItem->getDataSource() == NULL &&
                                dataItem->getType() == "AVAILABILITY")))
      {
        ComponentEventPtr *ptr = mLatest.getEventPtr(dataItem->getId());
        
        if (ptr != NULL) {
          const string *value = NULL;
          if (dataItem->isCondition()) {
            if ((*ptr)->getLevel() != ComponentEvent::UNAVAILABLE)
              value = &sConditionUnavailable;
          } else if (dataItem->hasConstraints()) { 
            std::vector<std::string> &values = dataItem->getConstrainedValues();
            if (values.size() > 1 && (*ptr)->getValue() != sUnavailable)
              value = &sUnavailable;
          } else if ((*ptr)->getValue() != sUnavailable) {
            value = &sUnavailable;
          }
          
          if (value != NULL)
            addToBuffer(dataItem, *value, time);
        }
      } else if (dataItem == NULL) {
        sLogger << LWARN << "No data Item for " << (*dataItemAssoc).first;
      }
    }
  }
}

void Agent::connected(Adapter *anAdapter, std::vector<Device*> aDevices)
{
  if (anAdapter->isAutoAvailable()) {
    string time = getCurrentTime(GMT_UV_SEC);
    std::vector<Device*>::iterator iter;
    for (iter = aDevices.begin(); iter != aDevices.end(); ++iter) {
      sLogger << LDEBUG << "Connected to adapter, setting all Availability data items to AVAILABLE";
      
      if ((*iter)->getAvailability() != NULL) 
      {
        sLogger << LDEBUG << "Adding availabilty event for " << (*iter)->getAvailability()->getId();
        addToBuffer((*iter)->getAvailability(), sAvailable, time);
      }
      else
      {
        sLogger << LDEBUG << "Cannot find availability for " << (*iter)->getName();
      }
    }
  }
}

/* Agent protected methods */
string Agent::handleCall(ostream& out,
                         const string& path,
                         const key_value_map& queries,
                         const string& call,
                         const string& device)
{
  try {
    string deviceName;
    if (!device.empty())
    {
      deviceName = device;
    }
    
    if (call == "current")
    {
      const string path = queries[(string) "path"];
      string result;
      
      int freq = checkAndGetParam(queries, "frequency", NO_FREQ,
                                  FASTEST_FREQ, false,SLOWEST_FREQ);
      // Check for 1.2 conversion to interval
      if (freq == NO_FREQ)
        freq = checkAndGetParam(queries, "interval", NO_FREQ,
                                FASTEST_FREQ, false, SLOWEST_FREQ);
      uint64_t at = checkAndGetParam64(queries, "at", NO_START, getFirstSequence(), true,
                                    mSequence - 1);
      int heartbeat = checkAndGetParam(queries, "heartbeat", 10000, 10, true, 600000);
      
      if (freq != NO_FREQ && at != NO_START) {
        return printError("INVALID_REQUEST", "You cannot specify both the at and frequency arguments to a current request");
      }
      
      
      return handleStream(out, devicesAndPath(path, deviceName), true,
                          freq, at, 0, heartbeat);
    }
    else if (call == "probe" || call.empty())
    {
      return handleProbe(deviceName);
    }
    else if (call == "sample")
    {
      string path = queries[(string) "path"];
      string result;
      
      int count = checkAndGetParam(queries, "count", DEFAULT_COUNT,
                                   1, true, mSlidingBufferSize);
      int freq = checkAndGetParam(queries, "frequency", NO_FREQ,
                                  FASTEST_FREQ, false, SLOWEST_FREQ);
      // Check for 1.2 conversion to interval
      if (freq == NO_FREQ)
        freq = checkAndGetParam(queries, "interval", NO_FREQ,
                                FASTEST_FREQ, false, SLOWEST_FREQ);
      
      uint64 start = checkAndGetParam64(queries, "start", NO_START, getFirstSequence(),
                                       true, mSequence);
      
      if (start == NO_START) // If there was no data in queries
      {
        start = checkAndGetParam64(queries, "from", 1,
                                   getFirstSequence(), true, mSequence);
      }
      
      int heartbeat = checkAndGetParam(queries, "heartbeat", 10000, 10, true, 600000);

      return handleStream(out, devicesAndPath(path, deviceName), false,
                          freq, start, count, heartbeat);
    }
    else if ((mDeviceMap[call] != NULL) && device.empty())
    {
      return handleProbe(call);
    }
    else
    {
      return printError("UNSUPPORTED",
                        "The following path is invalid: " + path);
    }
  }
  catch (ParameterError &aError)
  {
    return printError(aError.mCode, aError.mMessage);
  }
}

/* Agent protected methods */
string Agent::handlePut(
                        ostream& out,
                        const string& path,
                        const key_value_map& queries,
                        const string& adapter,
                        const string& deviceName
                        )
{
  string device = deviceName;
  if (device.empty() && adapter.empty())
  {
    return printError("UNSUPPORTED",
                      "Device must be specified for PUT");
  } else if (device.empty()) {
    device = adapter;
  }
  
  Device *dev = mDeviceMap[device];
  if (dev == NULL) {
    string message = ((string) "Cannot find device: ") + device;
    return printError("UNSUPPORTED", message);
  }
  
  // First check if this is an adapter put or a data put...
  if (queries["_type"] == "command") 
  {
    std::vector<Adapter*>::iterator adpt;
    
    for (adpt = dev->mAdapters.begin(); adpt != dev->mAdapters.end(); adpt++) {
      key_value_map::const_iterator kv;
      for (kv = queries.begin(); kv != queries.end(); kv++) {
        string command = kv->first + "=" + kv->second;
        sLogger << LDEBUG << "Sending command '" << command << "' to " << device;
        (*adpt)->sendCommand(command);
      }
    }
  } 
  else
  {
    string time = queries["time"];
    if (time.empty())
      time = getCurrentTime(GMT_UV_SEC);

    key_value_map::const_iterator kv;
    for (kv = queries.begin(); kv != queries.end(); kv++) {
      if (kv->first != "time") 
      {
        DataItem *di = dev->getDeviceDataItem(kv->first);
        if (di != NULL)
          addToBuffer(di, kv->second, time);
        else
          sLogger << LWARN << "(" << device << ") Could not find data item: " << kv->first;
      }
    }
    
  }
  
  return "<success/>";
}

string Agent::handleProbe(const string& name)
{
  std::vector<Device *> mDeviceList;
  
  if (!name.empty())
  {
    Device * device = getDeviceByName(name);
    if (device == NULL)
    {
      return printError("NO_DEVICE",
                        "Could not find the device '" + name + "'");
    }
    else
    {
      mDeviceList.push_back(device);
    }
  }
  else
  {
    mDeviceList = mDevices;
  }
  
  return XmlPrinter::printProbe(mInstanceId, mSlidingBufferSize, mSequence,
                                mMaxAssets, mAssets.size(),
                                mDeviceList, &mAssetCounts);
}

string Agent::handleStream(
                           ostream& out,
                           const string& path,
                           bool current,
                           unsigned int frequency,
                           uint64_t start,
                           unsigned int count,
                           unsigned int aHb
                           )
{
  std::set<string> filter;
  try
  {
    mXmlParser->getDataItems(filter, path);
  }
  catch (exception& e)
  {
    return printError("INVALID_XPATH", e.what());
  }
  
  if (filter.empty())
  {
    return printError("INVALID_XPATH",
                      "The path could not be parsed. Invalid syntax: " + path);
  }
  
  // Check if there is a frequency to stream data or not
  if (frequency != (unsigned) NO_FREQ)
  {
    streamData(out, filter, current, frequency, start, count, aHb);
    return "";
  }
  else
  {
    uint64_t end;
    bool endOfBuffer;
    if (current)
      return fetchCurrentData(filter, start);
    else
      return fetchSampleData(filter, start, count, end, endOfBuffer);
  }
}

std::string Agent::handleAssets(std::ostream& aOut,
                                const key_value_map& aQueries,
                                const std::string& aList)
{
  using namespace dlib;  
  std::vector<AssetPtr> assets;
  if (!aList.empty()) 
  {
    auto_mutex lock(*mAssetLock);
    istringstream str(aList);
    tokenizer_kernel_1 tok;
    tok.set_stream(str);
    tok.set_identifier_token(tok.lowercase_letters() + tok.uppercase_letters() +
                             tok.numbers() + "_.@$%&^:+-_=",
                             tok.lowercase_letters() + tok.uppercase_letters() +
                             tok.numbers() + "_.@$%&^:+-_=");
    
    
    int type;
    string token;
    for (tok.get_token(type, token); type != tok.END_OF_FILE; tok.get_token(type, token))
    {
      if (type == tok.IDENTIFIER)
      {
        AssetPtr ptr = mAssetMap[token];
        if (ptr.getObject() == NULL)
          return XmlPrinter::printError(mInstanceId, 0, 0, "ASSET_NOT_FOUND", 
                                        (string) "Could not find asset: " + token);
        assets.push_back(ptr);
      }
    }
  }
  else
  {
    auto_mutex lock(*mAssetLock);
    // Return all asssets, first check if there is a type attribute
    
    string type = aQueries["type"];
    bool removed = (aQueries.count("removed") > 0 && aQueries["removed"] == "true");    
    int count = checkAndGetParam(aQueries, "count", mAssets.size(),
                                1, false, NO_VALUE32);
    
    list<AssetPtr*>::reverse_iterator iter;
    for (iter = mAssets.rbegin(); iter != mAssets.rend() && count > 0; ++iter, --count)
    {
      if ((type.empty() || type == (**iter)->getType()) && (removed || !(**iter)->isRemoved())) {
        assets.push_back(**iter);
      }
    }    
  }
  
  return XmlPrinter::printAssets(mInstanceId, mMaxAssets, mAssets.size(), assets);
}


// Store an asset in the map by asset # and use the circular buffer as
// an LRU. Check if we're removing an existing asset and clean up the 
// map, and then store this asset. 
std::string Agent::storeAsset(std::ostream& aOut,
                              const key_value_map& aQueries,
                              const std::string& aId,
                              const std::string& aBody)
{
  string name = aQueries["device"];
  string type = aQueries["type"];
  Device *device = NULL;
  
  if (!name.empty()) device = mDeviceMap[name];
  
  // If the device was not found or was not provided, use the default device.
  if (device == NULL) device = mDevices[0];

  if (addAsset(device, aId, aBody, type))
    return "<success/>";
  else
    return "<failure/>";
}

string Agent::handleFile(const string &aUri, outgoing_things& aOutgoing)
{
  // Get the mime type for the file.
  bool unknown = true;
  size_t last = aUri.find_last_of("./");
  string contentType;
  if (last != string::npos && aUri[last] == '.')
  {
    string ext = aUri.substr(last + 1);
    if (mMimeTypes.count(ext) > 0)
    {
      contentType = mMimeTypes[ext];
      unknown = false;
    }
  }
  if (unknown)
    contentType = "application/octet-stream";
  
  // Check if the file is cached
  RefCountedPtr<CachedFile> cachedFile;
  std::map<string, RefCountedPtr<CachedFile> >::iterator cached = mFileCache.find(aUri);
  if (cached != mFileCache.end())
    cachedFile = cached->second;
  else
  {
    
    std::map<string,string>::iterator file = mFileMap.find(aUri);
    
    // Should never happen
    if (file == mFileMap.end()) {
      aOutgoing.http_return = 404;
      aOutgoing.http_return_status = "File not found";
      return "";
    }
    
    const char *path = file->second.c_str();
    
    struct stat fs;
    int res = stat(path, &fs);
    if (res != 0) {
      aOutgoing.http_return = 404;
      aOutgoing.http_return_status = "File not found";
      return "";
    }
    
    int fd = open(path, O_RDONLY | O_BINARY);
    if (res < 0) {
      aOutgoing.http_return = 404;
      aOutgoing.http_return_status = "File not found";
      return "";
    }
    
    cachedFile.setObject(new CachedFile(fs.st_size), true);
    int bytes = read(fd, cachedFile->mBuffer, fs.st_size);
    close(fd);
    
    if (bytes < fs.st_size) {
      aOutgoing.http_return = 404;
      aOutgoing.http_return_status = "File not found";
      return "";
    }
    
    // If this is a small file, cache it.
    if (bytes <= SMALL_FILE) {
      mFileCache.insert(pair<string, RefCountedPtr<CachedFile> >(aUri, cachedFile));
    }
  }
  
  (*aOutgoing.out) << "HTTP/1.1 200 OK\r\n"
    "Date: " << getCurrentTime(HUM_READ) << "\r\n"
    "Server: MTConnectAgent\r\n"
    "Connection: close\r\n"
    "Content-Length: " << cachedFile->mSize << "\r\n"
    "Expires: " << getCurrentTime(time(NULL) + 60 * 60 * 24, 0, HUM_READ) << "\r\n"
    "Content-Type: " << contentType << "\r\n\r\n";

  aOutgoing.out->write(cachedFile->mBuffer, cachedFile->mSize);
  
  aOutgoing.out->setstate(ios::badbit);

  return "";
}

void Agent::streamData(ostream& out,
                       std::set<string> &aFilter,
                       bool current,
                       unsigned int aInterval,
                       uint64_t start,
                       unsigned int count,
                       unsigned int aHeartbeat
                       )
{
  // Create header
  string boundary = md5(intToString(time(NULL)));
  
  ofstream log;
  if (mLogStreamData)
  {
    string filename = "Stream_" + getCurrentTime(LOCAL) + "_" +
                      int64ToString((uint64_t) dlib::get_thread_id()) + ".log";
    log.open(filename.c_str());
  }

  out << "HTTP/1.1 200 OK\r\n"
         "Date: " << getCurrentTime(HUM_READ) << "\r\n"
         "Server: MTConnectAgent\r\n"
         "Expires: -1\r\n"
         "Connection: close\r\n"
         "Cache-Control: private, max-age=0\r\n"
         "Content-Type: multipart/x-mixed-replace;boundary=" << boundary << "\r\n"
         "Transfer-Encoding: chunked\r\n\r\n";
     
  // This object will automatically clean up all the observer from the
  // signalers in an exception proof manor.
  ChangeObserver observer;
  
  // Add observers
  std::set<string>::iterator iter;
  for (iter = aFilter.begin(); iter != aFilter.end(); ++iter)
    mDataItemMap[*iter]->addObserver(&observer);
  
  uint64_t interMicros = aInterval * 1000;
  uint64_t firstSeq = getFirstSequence();
  if (start < firstSeq)
    start = firstSeq;
  
  try {
    // Loop until the user closes the connection
    timestamper ts;
    while (out.good())
    {
      // Remember when we started this grab...
      uint64_t last = ts.get_timestamp();
      
      // Fetch sample data now resets the observer while holding the sequence
      // mutex to make sure that a new event will be recorded in the observer
      // when it returns.
      string content;
      uint64_t end;
      bool endOfBuffer = true;
      if (current) {
        content = fetchCurrentData(aFilter, NO_START);
      } else {
        // Check if we're falling too far behind. If we are, generate an 
        // MTConnectError and return.
        if (start < getFirstSequence()) {
          sLogger << LWARN << "Client fell too far behind, disconnecting";
          throw ParameterError("OUT_OF_RANGE", "Client can't keep up with event stream, disconnecting");
        } else {
          // end and endOfBuffer are set during the fetch sample data while the 
          // mutex is held. This removed the race to check if we are at the end of 
          // the bufffer and setting the next start to the last sequence number 
          // sent.
          content = fetchSampleData(aFilter, start, count, end, 
                                    endOfBuffer, &observer);                
        }
        
        if (mLogStreamData)
          log << content << endl;
      }

      ostringstream str;

      // Make sure we're terminated with a <cr><nl>
      content.append("\r\n");
      out.setf(ios::dec, ios::basefield);
      str << "--" << boundary << "\r\n"
             "Content-type: text/xml\r\n"
             "Content-length: " << content.length() << "\r\n\r\n"
             << content;
        
      string chunk = str.str();
      out.setf(ios::hex, ios::basefield);
      out << chunk.length() << "\r\n";
      out << chunk << "\r\n";
      out.flush();
      
      // Wait for up to frequency ms for something to arrive... Don't wait if 
      // we are not at the end of the buffer. Just put the next set after aInterval 
      // has elapsed. Check also if in the intervening time between the last fetch
      // and now. If so, we just spin through and wait the next interval.
      
      // Even if we are at the end of the buffer, or within range. If we are filtering,
      // we will need to make sure we are not spinning when there are no valid events
      // to be reported. we will waste cycles spinning on the end of the buffer when 
      // we should be in a heartbeat wait as well. 
      if (!endOfBuffer) {
        // If we're not at the end of the buffer, move to the end of the previous set and
        // begin filtering from where we left off.
        start = end;
        
        // For replaying of events, we will stream as fast as we can with a 1ms sleep
        // to allow other threads to run.          
        dlib::sleep(1);
      } else {
        uint64 delta;
        
        if (!current) {
          // Busy wait to make sure the signal was actually signaled. We have observed that
          // a signal can occur in rare conditions where there are multiple threads listening
          // on separate condition variables and this pops out too soon. This will make sure
          // observer was actually signaled and instead of throwing an error will wait again
          // for the remaining hartbeat interval.
          delta = (ts.get_timestamp() - last) / 1000;
          while (delta < aHeartbeat &&
                 observer.wait(aHeartbeat - delta) &&
                 !observer.wasSignaled()) {
            delta = (ts.get_timestamp() - last) / 1000;
          }
          
          {
            dlib::auto_mutex lock(*mSequenceLock);
          
            // Make sure the observer was signaled!
            if (!observer.wasSignaled()) {
              // If nothing came out during the last wait, we may have still have advanced
              // the sequence number. We should reset the start to something closer to the
              // current sequence. If we lock the sequence lock, we can check if the observer
              // was signaled between the time the wait timed out and the mutex was locked.
              // Otherwise, nothing has arrived and we set to the next sequence number to
              // the next sequence number to be allocated and continue.
              start = mSequence;
            } else {
              // Get the sequence # signaled in the observer when the earliest event arrived.
              // This will allow the next set of data to be pulled. Any later events will have
              // greater sequence numbers, so this should not cause a problem. Also, signaled
              // sequence numbers can only decrease, never increase.
              start = observer.getSequence();
            }
          }
        }
        
        // Now wait the remainder if we triggered before the timer was up.
        delta = ts.get_timestamp() - last;
        if (delta < interMicros) {
          // Sleep the remainder
          dlib::sleep((interMicros - delta) / 1000);
        }
      }
    }
  }
  catch (ParameterError &aError)
  {
    sLogger << LINFO << "Caught a parameter error.";
    if (out.good()) {
      ostringstream str;
      string content = printError(aError.mCode, aError.mMessage);
      str << "--" << boundary << "\r\n"
        "Content-type: text/xml\r\n"
        "Content-length: " << content.length() << "\r\n\r\n"
        << content;
      
      string chunk = str.str();
      out.setf(ios::hex, ios::basefield);
      out << chunk.length() << "\r\n";
      out << chunk << "\r\n";
      out.flush();
    }
  }
  
  catch (...)
  {
    sLogger << LWARN << "Error occurred during streaming data";
    if (out.good()) {
      ostringstream str;
      string content = printError("INTERNAL_ERROR", "Unknown error occurred during streaming");
      str << "--" << boundary << "\r\n"
        "Content-type: text/xml\r\n"
        "Content-length: " << content.length() << "\r\n\r\n"
        << content;
      
      string chunk = str.str();
      out.setf(ios::hex, ios::basefield);
      out << chunk.length() << "\r\n";
      out << chunk;
      out.flush();
    }
  }
  
  out.setstate(ios::badbit);
  // Observer is auto removed from signalers
}

string Agent::fetchCurrentData(std::set<string> &aFilter, uint64_t at)
{
  ComponentEventPtrArray events;
  uint64_t firstSeq, seq;
  {
    dlib::auto_mutex lock(*mSequenceLock);
    firstSeq = getFirstSequence();
    seq = mSequence;
    if (at == NO_START)
    {
      mLatest.getComponentEvents(events, &aFilter);
    }
    else
    {
      long pos = (long) mSlidingBuffer->get_element_id(at);
      long first = (long) mSlidingBuffer->get_element_id(firstSeq);
      long checkIndex = pos / mCheckpointFreq;
      long closestCp = checkIndex * mCheckpointFreq;
      unsigned long index;
      
      Checkpoint *ref;
      
      // Compute the closest checkpoint. If the checkpoint is after the
      // first checkpoint and before the next incremental checkpoint,
      // use first.
      if (first > closestCp && pos >= first)
      {
        ref = &mFirst;
        // The checkpoint is inclusive of the "first" event. So we add one
        // so we don't duplicate effort.
        index = first + 1;
      }
      else
      {
        index = closestCp + 1;
        ref = &mCheckpoints[checkIndex];
      }
      
      Checkpoint check(*ref, &aFilter);
      
      // Roll forward from the checkpoint.
      for (; index <= (unsigned long) pos; index++) {
        check.addComponentEvent(((*mSlidingBuffer)[(unsigned long)index]).getObject());
      }
      
      check.getComponentEvents(events);
    }
  }
  
  string toReturn = XmlPrinter::printSample(mInstanceId, mSlidingBufferSize,
                                            seq, firstSeq, mSequence - 1, events);
  
  return toReturn;
}

string Agent::fetchSampleData(std::set<string> &aFilter,
                              uint64_t start, unsigned int count, uint64_t &end, 
                              bool &endOfBuffer, ChangeObserver *aObserver)
{
  ComponentEventPtrArray results;
  uint64_t firstSeq;
  {
    dlib::auto_mutex lock(*mSequenceLock);
    
    firstSeq = (mSequence > mSlidingBufferSize) ?
                        mSequence - mSlidingBufferSize : 1;
    
    // START SHOULD BE BETWEEN 0 AND SEQUENCE NUMBER
    start = (start <= firstSeq) ? firstSeq : start;
    
    uint64_t i;
    for (i = start; results.size() < count && i < mSequence; i++)
    {
      // Filter out according to if it exists in the list
      const string &dataId = (*mSlidingBuffer)[i]->getDataItem()->getId();
      if (aFilter.count(dataId) > 0)
      {
        ComponentEventPtr event = (*mSlidingBuffer)[i];
        results.push_back(event);
      }
    }
    
    end = i;
    if (i >= mSequence)
      endOfBuffer = true;
    else
      endOfBuffer = false;
    
    if (aObserver != NULL) aObserver->reset();
  }
  
  return XmlPrinter::printSample(mInstanceId, mSlidingBufferSize, end, 
                                 firstSeq, mSequence - 1, results);
}

string Agent::printError(const string& errorCode, const string& text)
{
  sLogger << LDEBUG << "Returning error " << errorCode << ": " << text;
  return XmlPrinter::printError(mInstanceId, mSlidingBufferSize, mSequence,
                                errorCode, text);
}

string Agent::devicesAndPath(const string& path, const string& device)
{
  string dataPath = "";
  
  if (!device.empty())
  {
    string prefix = "//Devices/Device[@name=\"" + device + "\"]";
    
    if (!path.empty())
    {
      istringstream toParse(path);
      string token;
      
      // Prefix path (i.e. "path1|path2" => "{prefix}path1|{prefix}path2")
      while (getline(toParse, token, '|'))
      {
        dataPath += prefix + token + "|";
      }
      
      dataPath.erase(dataPath.length()-1);
    }
    else
    {
      dataPath = prefix;
    }
  }
  else
  {
    dataPath = (!path.empty()) ? path : "//Devices/Device";
  }
  
  return dataPath;
}

int Agent::checkAndGetParam(const key_value_map& queries,
                            const string& param,
                            const int defaultValue,
                            const int minValue,
                            bool minError,
                            const int maxValue)
{
  if (queries.count(param) == 0)
  {
    return defaultValue;
  }
  
  if (queries[param].empty())
  {
    throw ParameterError("QUERY_ERROR", "'" + param + "' cannot be empty.");
  }
  
  if (!isNonNegativeInteger(queries[param]))
  {
    throw ParameterError("OUT_OF_RANGE",
                        "'" + param + "' must be a positive integer.");
  }
  
  long int value = strtol(queries[param].c_str(), NULL, 10);
  
  if (minValue != NO_VALUE32 && value < minValue)
  {
    if (minError)
    {
      throw ParameterError("OUT_OF_RANGE",
                          "'" + param + "' must be greater than or equal to " + intToString(minValue) + ".");
    }
    return minValue;
  }
  
  if (maxValue != NO_VALUE32 && value > maxValue)
  {
    throw ParameterError("OUT_OF_RANGE",
                        "'" + param + "' must be less than or equal to " + intToString(maxValue) + ".");
  }
  
  return value;
}

uint64_t Agent::checkAndGetParam64(const key_value_map& queries,
                                const string& param,
                                const uint64_t defaultValue,
                                const uint64_t minValue,
                                bool minError,
                                const uint64_t maxValue)
{
  if (queries.count(param) == 0)
  {
    return defaultValue;
  }
  
  if (queries[param].empty())
  {
    throw ParameterError("QUERY_ERROR", "'" + param + "' cannot be empty.");
  }
  
  if (!isNonNegativeInteger(queries[param]))
  {
    throw ParameterError("OUT_OF_RANGE",
                        "'" + param + "' must be a positive integer.");
  }
  
  uint64_t value = strtoull(queries[param].c_str(), NULL, 10);
  
  if (minValue != NO_VALUE64 && value < minValue)
  {
    if (minError)
    {
      throw ParameterError("OUT_OF_RANGE",
                          "'" + param + "' must be greater than or equal to " + intToString(minValue) + ".");
    }
    return minValue;
  }
  
  if (maxValue != NO_VALUE64 && value > maxValue)
  {
    throw ParameterError("OUT_OF_RANGE",
                        "'" + param + "' must be less than or equal to " + intToString(maxValue) + ".");
  }
  
  return value;
}


DataItem * Agent::getDataItemByName(const string& device, const string& name)
{
  Device *dev = mDeviceMap[device];
  return (dev) ? dev->getDeviceDataItem(name) : NULL;
}

void Agent::updateDom(Device *aDevice)
{
  mXmlParser->updateDevice(aDevice);
}

