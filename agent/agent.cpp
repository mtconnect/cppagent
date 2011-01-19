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

#include "agent.hpp"
#include "dlib/logger.h"

using namespace std;

static const string sUnavailable("UNAVAILABLE");
static const string sConditionUnavailable("UNAVAILABLE|||");

#ifdef WIN32
#define strtoll _strtoi64
#endif

static dlib::logger sLogger("agent");

/* Agent public methods */
Agent::Agent(const string& configXmlPath, int aBufferSize, int aCheckpointFreq)
{
  try
  {
    // Load the configuration for the Agent
    mConfig = new XmlParser(configXmlPath);
  }
  catch (exception & e)
  {
    sLogger << LFATAL << "Error loading xml configuration: " + configXmlPath;
    sLogger << LFATAL << "Error detail: " << e.what();
    cerr << e.what() << endl;
    throw e;
  }

  // Grab data from configuration
  mDevices = mConfig->getDevices();
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

  // Create the checkpoints at a regular frequency
  mCheckpoints = new Checkpoint[mCheckpointCount];

  // Mutex used for synchronized access to sliding buffer and sequence number
  mSequenceLock = new dlib::mutex;

  /* Initialize the id mapping for the devices and set all data items to UNAVAILABLE */
  vector<Device *>::iterator device;
  for (device = mDevices.begin(); device != mDevices.end(); ++device) 
  {
    mDeviceMap[(*device)->getName()] = *device;

    std::map<string, DataItem*> items = (*device)->getDeviceDataItems();

    std::map<string, DataItem *>::iterator item;
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
      mDataItemMap[d->getId()] = d;
    }
  }
}

Agent::~Agent()
{
  delete mSlidingBuffer;
  delete mSequenceLock;
  delete mConfig;
  delete[] mCheckpoints;
}

void Agent::start()
{
  try {
    // Start all the adapters
    vector<Adapter*>::iterator iter;
    for (iter = mAdapters.begin(); iter != mAdapters.end(); iter++) {
      (*iter)->start();
    }
    
    // Start the server. This blocks until the server stops.
    server::http_1a::start();
  }
  catch (dlib::socket_error &e) {
    sLogger << LFATAL << "Cannot start server: " << e.what();
    exit(1);
  }
}

// Methods for service
const string Agent::on_request (
  const incoming_things& incoming,
  outgoing_things& outgoing
  )
{
  string result;
  outgoing.headers["Content-Type"] = "text/xml";
  try 
  {
    sLogger << LDEBUG << "Request: " << incoming.request_type << " " << 
      incoming.path << " from " << incoming.foreign_ip << ":" << incoming.foreign_port;
      
    if (incoming.request_type != "GET" && incoming.request_type != "PUT" &&
        incoming.request_type != "POST") {
      return printError("UNSUPPORTED",
        "Only the HTTP GET and PUT requests are supported by MTConnect");
    }

    // Parse the URL path looking for '/'
    string path = incoming.path;
    size_t qm = path.find_last_of('?');
    if (qm != string::npos)
      path = path.substr(0, qm);
    string::size_type loc1 = path.find("/", 1);

    string::size_type end = (path[path.length()-1] == '/') ?
      path.length()-1 : string::npos;
      
    string device, call;

    // If a '/' was found
    if (loc1 < end)
    {
      // Look for another '/'
      string::size_type loc2 = path.find("/", loc1+1);

      if (loc2 == end)
      {
        call = path.substr(loc1+1, loc2-loc1-1);
        device = path.substr(1, loc1-1);
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
      call = path.substr(1, loc1-1);
    }
    
    if (incoming.request_type == "GET")
      result = handleCall(*outgoing.out, path, incoming.queries, call, device);    
    else
      result = handlePut(*outgoing.out, path, incoming.queries, call, device);
  }
  catch (exception & e)
  {
    printError("SERVER_EXCEPTION",(string) e.what()); 
  }

  return result;
}

Adapter * Agent::addAdapter(
  const string& device,
  const string& host,
  const unsigned int port,
  bool start
  )
{
  Adapter *adapter = new Adapter(device, host, port);
  adapter->setAgent(*this);
  mAdapters.push_back(adapter);
  if (start)
    adapter->start();
  return adapter;
}

unsigned int Agent::addToBuffer(
  DataItem *dataItem,
  const string& value,
  string time
  )
{
  if (dataItem == 0) return 0;

  dlib::auto_mutex lock(*mSequenceLock);

  // If this function is being used as an API, add the current time in
  if (time.empty()) {
    time = getCurrentTime(GMT_UV_SEC);
  }

  Int64 seqNum = mSequence++;
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

  return seqNum;
}

/* Add values for related data items UNAVAILABLE */
void Agent::disconnected(Adapter *anAdapter, Device *aDevice)
{
  string time = getCurrentTime(GMT_UV_SEC);
  sLogger << LDEBUG << "Disconnected from adapter, setting all values to UNAVAILABLE";

  if (aDevice != NULL)
  {
    std::map<std::string, DataItem *> dataItems = aDevice->getDeviceDataItems();
    std::map<std::string, DataItem*>::iterator dataItemAssoc;
    for (dataItemAssoc = dataItems.begin(); dataItemAssoc != dataItems.end(); ++dataItemAssoc)
    {
      DataItem *dataItem = (*dataItemAssoc).second;
      if (dataItem != NULL && dataItem->getDataSource() == anAdapter)
      {
        const string *value = NULL;
        if (dataItem->isCondition()) {
          value = &sConditionUnavailable;
        } else if (dataItem->hasConstraints()) { 
          std::vector<std::string> &values = dataItem->getConstrainedValues();
          if (values.size() > 1)
            value = &sUnavailable;
        } else {
          value = &sUnavailable;
        }

        if (value != NULL)
          addToBuffer(dataItem, *value, time);
      } else if (dataItem == NULL) {
        sLogger << LWARN << "No data Item for " << (*dataItemAssoc).first;
      }
    }
  }
}

/* Agent protected methods */
string Agent::handleCall(
  ostream& out,
  const string& path,
  const key_value_map& queries,
  const string& call,
  const string& device
  )
{
  string deviceName;
  if (!device.empty())
  {
    deviceName = device;
  }

  if (call == "current")
  {
    const string path = queries[(string) "path"];
    string result;

    int freq = checkAndGetParam(result, queries, "frequency", NO_FREQ,
      FASTEST_FREQ, false, SLOWEST_FREQ);
    Int64 at = checkAndGetParam64(result, queries, "at", NO_START, getFirstSequence(), true,
      mSequence);
    if (freq == PARAM_ERROR || at == PARAM_ERROR)
    {
      return result;
    }

    if (freq != NO_FREQ && at != NO_START) {
      return printError("INVALID_REQUEST", "You cannot specify both the at and frequency arguments to a current request");
    }


    return handleStream(out, devicesAndPath(path, deviceName), true,
      freq, at);
  }
  else if (call == "probe" || call.empty())
  {
    return handleProbe(deviceName);
  }
  else if (call == "sample")
  {
    string path = queries[(string) "path"];
    string result;

    int count = checkAndGetParam(result, queries, "count", DEFAULT_COUNT,
      1, true, mSlidingBufferSize);
    int freq = checkAndGetParam(result, queries, "frequency", NO_FREQ,
      FASTEST_FREQ, false, SLOWEST_FREQ);

    Int64 start = checkAndGetParam64(result, queries, "start", NO_START, getFirstSequence(),
      true, mSequence);

    if (start == NO_START) // If there was no data in queries
    {
      start = checkAndGetParam64(result, queries, "from", 1,
        getFirstSequence(), true, mSequence);
    }

    if (freq == PARAM_ERROR || count == PARAM_ERROR || start == PARAM_ERROR)
    {
      return result;
    }

    return handleStream(out, devicesAndPath(path, deviceName), false,
      freq, start, count);
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
  
  std::vector<Adapter*>::iterator adpt;
  
  for (adpt = dev->mAdapters.begin(); adpt != dev->mAdapters.end(); adpt++) {
    key_value_map::const_iterator kv;
    for (kv = queries.begin(); kv != queries.end(); kv++) {
      string command = kv->first + "=" + kv->second;
      sLogger << LDEBUG << "Sending command '" << command << "' to " << device;
      (*adpt)->sendCommand(command);
    }
  }
  
  return "";
}

string Agent::handleProbe(const string& name)
{
  vector<Device *> mDeviceList;

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
    mDeviceList);
}

string Agent::handleStream(
  ostream& out,
  const string& path,
  bool current,
  unsigned int frequency,
  Int64 start,
  unsigned int count
  )
{
  std::set<string> filter;
  try
  {
    mConfig->getDataItems(filter, path);
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
    if (current)
    {
      streamData(out, filter, true, frequency);
    }
    else
    {
      streamData(out, filter, false, frequency, start, count);
    }
    return "";
  }
  else
  {
    unsigned int items;
    if (current)
      return fetchCurrentData(filter, start);
    else
      return fetchSampleData(filter, start, count, items);
  }
}

void Agent::streamData(ostream& out,
  std::set<string> &aFilter,
  bool current,
  unsigned int frequency,
  Int64 start,
  unsigned int count
  )
{
  // Create header
  out << "HTTP/1.1 200 OK" << endl;
  out << "Connection: close" << endl;
  out << "Date: " << getCurrentTime(HUM_READ) << endl;
  out << "Status: 200 OK" << endl;
  out << "Content-Disposition: inline" << endl;
  out << "Content-Type: multipart/x-mixed-replace;";

  string boundary = md5(intToString(time(NULL)));
  out << "boundary=" << boundary << endl << endl;

  // Loop until the user closes the connection
  time_t t;
  int heartbeat = 0;
  while (out.good())
  {
    unsigned int items;
    string content;
    if (current)
      content = fetchCurrentData(aFilter, NO_START);
    else
      content = fetchSampleData(aFilter, start, count, items);

    start = (start + count < mSequence) ? (start + count) : mSequence;

    if (items > 0 || (time(&t) - heartbeat) >= 10) 
    {
      heartbeat = t;
      out << "--" + boundary << endl;
      out << "Content-type: text/xml" << endl;
      out << "Content-length: " << content.length() << endl;
      out << endl << content;

      out.flush();
    }

    dlib::sleep(frequency);
  }
}

string Agent::fetchCurrentData(std::set<string> &aFilter, Int64 at)
{
  dlib::auto_mutex lock(*mSequenceLock);

  vector<ComponentEventPtr> events;
  unsigned int firstSeq = getFirstSequence();
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

  string toReturn = XmlPrinter::printSample(mInstanceId, mSlidingBufferSize,
    mSequence, firstSeq, events);

  return toReturn;
}

string Agent::fetchSampleData(std::set<string> &aFilter,
  Int64 start,
  unsigned int count,
  unsigned int &items
  )
{
  vector<ComponentEventPtr> results;

  dlib::auto_mutex lock(*mSequenceLock);

  Int64 seq = mSequence;
  Int64 firstSeq = (mSequence > mSlidingBufferSize) ?
    mSequence - mSlidingBufferSize : 1;

  // START SHOULD BE BETWEEN 0 AND SEQUENCE NUMBER
  start = (start <= firstSeq) ? firstSeq : start;
  Int64 end = (count + start >= mSequence) ? mSequence : count + start;
  items = 0;

  for (Int64 i = start; i < end; i++)
  {
    // Filter out according to if it exists in the list
    const string &dataId = (*mSlidingBuffer)[i]->getDataItem()->getId();
    if (aFilter.count(dataId) > 0)
    {
      ComponentEvent *event = (*mSlidingBuffer)[i];
      results.push_back(event);
      items++;
    }
  }

  return XmlPrinter::printSample(mInstanceId, mSlidingBufferSize, seq, 
    firstSeq, results);
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

int Agent::checkAndGetParam(
  string& result,
  const key_value_map& queries,
  const string& param,
  const int defaultValue,
  const int minValue,
  bool minError,
  const int maxValue
  )
{
  if (queries.count(param) == 0)
  {
    return defaultValue;
  }

  if (queries[param].empty())
  {
    result = printError("QUERY_ERROR", "'" + param + "' cannot be empty.");
    return PARAM_ERROR;
  }

  if (!isNonNegativeInteger(queries[param]))
  {
    result = printError("QUERY_ERROR",
      "'" + param + "' must be a positive integer.");
    return PARAM_ERROR;
  }

  long int value = strtol(queries[param].c_str(), NULL, 10);

  if (minValue != NO_VALUE && value < minValue)
  {
    if (minError)
    {
      result = printError("QUERY_ERROR",
        "'" + param + "' must be greater than or equal to " + intToString(minValue) + ".");
      return PARAM_ERROR;
    }
    return minValue;
  }

  if (maxValue != NO_VALUE && value > maxValue)
  {
    result = printError("QUERY_ERROR",
      "'" + param + "' must be less than or equal to " + intToString(maxValue) + ".");
    return PARAM_ERROR;
  }

  return value;
}

Int64 Agent::checkAndGetParam64(
  string& result,
  const key_value_map& queries,
  const string& param,
  const Int64 defaultValue,
  const Int64 minValue,
  bool minError,
  const Int64 maxValue
  )
{
  if (queries.count(param) == 0)
  {
    return defaultValue;
  }

  if (queries[param].empty())
  {
    result = printError("QUERY_ERROR", "'" + param + "' cannot be empty.");
    return PARAM_ERROR;
  }

  if (!isNonNegativeInteger(queries[param]))
  {
    result = printError("QUERY_ERROR",
      "'" + param + "' must be a positive integer.");
    return PARAM_ERROR;
  }

  Int64 value = strtoll(queries[param].c_str(), NULL, 10);

  if (minValue != NO_VALUE && value < minValue)
  {
    if (minError)
    {
      result = printError("QUERY_ERROR",
        "'" + param + "' must be greater than or equal to " + intToString(minValue) + ".");
      return PARAM_ERROR;
    }
    return minValue;
  }

  if (maxValue != NO_VALUE && value > maxValue)
  {
    result = printError("QUERY_ERROR",
      "'" + param + "' must be less than or equal to " + intToString(maxValue) + ".");
    return PARAM_ERROR;
  }

  return value;
}


DataItem * Agent::getDataItemByName(const string& device, const string& name)
{
  Device *dev = mDeviceMap[device];
  return (dev) ? dev->getDeviceDataItem(name) : NULL;
}

