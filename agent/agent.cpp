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

using namespace std;

static const string sUnavailable("UNAVAILABLE");
static const string sNormal("normal||UNAVAILABLE|");

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
    logEvent("Agent::Agent",
      "Error loading xml configuration: " + configXmlPath);
    delete mConfig;
    throw (string) e.what();
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
	value = &sNormal;
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

bool Agent::on_request(
    const req_type rtype,
    const string& path,
    string& result,
    const map_type& queries,
    const map_type& cookies,
    queue_type& new_cookies,
    const map_type& incoming_headers,
    map_type& response_headers,
    const string& foreign_ip,
    const string& local_ip,
    unsigned short foreign_port,
    unsigned short local_port,
    ostream& out
  )
{
  try 
  {
    if (rtype != get)
    {
      result = printError("UNSUPPORTED",
			  "Only the HTTP GET request is supported by MTConnect");
      return true;
      
    }
    
    // Parse the URL path looking for '/'
    string::size_type loc1 = path.find("/", 1);
    
    string::size_type end = (path[path.length()-1] == '/') ?
      path.length()-1 : string::npos;
    
    // If a '/' was found
    if (loc1 < end)
    {
      // Look for another '/'
      string::size_type loc2 = path.find("/", loc1+1);
      
      if (loc2 == end)
      {
        return handleCall(out, path, result, queries, 
          path.substr(loc1+1, loc2-loc1-1), path.substr(1, loc1-1));
      }
      else
      {
        // Path is too long
        result = printError("UNSUPPORTED",
          "The following path is invalid: " + path);
        return true;
      }
    }
    else
    {
      // Try to handle the call
      return handleCall(out, path, result, queries, path.substr(1, loc1-1));
    }
  }
  catch (exception & e)
  {
    logEvent("Agent", (string) e.what());
    printError("SERVER_EXCEPTION",(string) e.what()); 
  }
  
  return true;
}

Adapter * Agent::addAdapter(
    const string& device,
    const string& host,
    const unsigned int port
  )
{
  Adapter *adapter = new Adapter(device, host, port);
  adapter->setAgent(*this);
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

  unsigned int seqNum = mSequence++;
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
void Agent::disconnected(Adapter *anAdapter, const std::string aDevice)
{
  string time = getCurrentTime(GMT_UV_SEC);
  
  Device *dev = mDeviceMap[aDevice];
  if (dev != NULL)
  {
    std::map<std::string, DataItem *> dataItems = dev->getDeviceDataItems();
    std::map<std::string, DataItem*>::iterator dataItemAssoc;
    for (dataItemAssoc = dataItems.begin(); dataItemAssoc != dataItems.end(); ++dataItemAssoc)
    {
      DataItem *dataItem = (*dataItemAssoc).second;
      if (dataItem->getDataSource() == anAdapter)
      {
	const string *value = NULL;
	if (dataItem->isCondition()) {
	  value = &sNormal;
	} else if (dataItem->hasConstraints()) { 
	  std::vector<std::string> &values = dataItem->getConstrainedValues();
	  if (values.size() > 1)
	    value = &sUnavailable;
	} else {
          value = &sUnavailable;
        }
      
        if (value != NULL)
          addToBuffer(dataItem, *value, time);
      }
    }
  }
}

/* Agent protected methods */
bool Agent::handleCall(
    ostream& out,
    const string& path,
    string& result,
    const map_type& queries,
    const string& call,
    const string& device
  )
{
  string deviceName;
  if (!device.empty())
  {
    deviceName = device;
  }
  else if (queries.is_in_domain("device"))
  {
    deviceName = queries["device"];
  }
  
  if (call == "current")
  {
    const string path = queries.is_in_domain("path") ? queries["path"] : "";
    
    int freq = checkAndGetParam(result, queries, "frequency", NO_FREQ,
				FASTEST_FREQ, false, SLOWEST_FREQ);
    int at = checkAndGetParam(result, queries, "at", NO_START, getFirstSequence(), true,
			      mSequence);
    if (freq == PARAM_ERROR || at == PARAM_ERROR)
    {
      return true;
    }

    if (freq != NO_FREQ && at != NO_START) {
      result =  printError("INVALID_REQUEST", "You cannot specify both the at and frequency arguments to a current request");
      return true;
    }
    
    
    return handleStream(out, result, devicesAndPath(path, deviceName), true,
			freq, at);
  }
  else if (call == "probe" || call.empty())
  {
    result = handleProbe(deviceName);
    return true;
  }
  else if (call == "sample")
  {
    string path = queries.is_in_domain("path") ?
      queries["path"] : "";
    
    int count = checkAndGetParam(result, queries, "count", DEFAULT_COUNT,
				 1, true, mSlidingBufferSize);
    int freq = checkAndGetParam(result, queries, "frequency", NO_FREQ,
				FASTEST_FREQ, false, SLOWEST_FREQ);
    
    int start = checkAndGetParam(result, queries, "start", NO_START, getFirstSequence(),
				 true, mSequence);
    
    if (start == NO_START) // If there was no data in queries
    {
      start = checkAndGetParam(result, queries, "from", 1,
			       getFirstSequence(), true, mSequence);
    }
    
    if (freq == PARAM_ERROR || count == PARAM_ERROR || start == PARAM_ERROR)
    {
      return true;
    }

    return handleStream(out, result, devicesAndPath(path, deviceName), false,
			freq, start, count);
  }
  else if ((mDeviceMap[call] != NULL) && device.empty())
  {
    result = handleProbe(call);
    return true;
  }
  else
  {
    result = printError("UNSUPPORTED",
      "The following path is invalid: " + path);
    return true;
  }
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

bool Agent::handleStream(
    ostream& out,
    string& result,
    const string& path,
    bool current,
    unsigned int frequency,
    unsigned int start,
    unsigned int count
  )
{
  std::set<string> filter;
  try
  {
    getDataItems(filter, path);
  }
  catch (exception& e)
  {
    result = printError("INVALID_XPATH", e.what());
    logEvent("Agent::handleStream", e.what());
    return true;
  }
  
  if (filter.empty())
  {
    result = printError("INVALID_XPATH",
			"The path could not be parsed. Invalid syntax: " + path);
    return true;
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
    return false;
  }
  else
  {
    unsigned int items;
    if (current)
      result = fetchCurrentData(filter, start);
    else
      result = fetchSampleData(filter, start, count, items);
    return true;
  }
}

void Agent::streamData(ostream& out,
                       std::set<string> &aFilter,
                       bool current,
                       unsigned int frequency,
                       unsigned int start,
                       unsigned int count
  )
{
  // Create header
  out << "HTTP/1.1 200 OK" << endl;
  out << "Connection: close" << endl;
  out << "Date: " << getCurrentTime(HUM_READ) << endl;
  out << "Status: 200 OK" << endl;
  out << "Content-Disposition: inline" << endl;
  out << "X-Runtime: 144ms" << endl;
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
      content = fetchCurrentData(aFilter, start);
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

string Agent::fetchCurrentData(std::set<string> &aFilter, unsigned int at)
{
  dlib::auto_mutex lock(*mSequenceLock);
  
  vector<ComponentEventPtr> events;
  unsigned int firstSeq = (mSequence > mSlidingBufferSize) ?
	       mSequence - mSlidingBufferSize : 1;
  if (at == (unsigned int) NO_START)
  {
    mLatest.getComponentEvents(events, &aFilter);
  }
  else
  {
    long pos = (long) mSlidingBuffer->get_element_id(at);
    long beg = (long) mSlidingBuffer->get_element_id(firstSeq);
    Checkpoint *ref;
    
    // Compute the closest checkpoint
    unsigned long index;
    long delta = (pos - beg);
    if (delta >= 0 && delta < mCheckpointFreq)
    {
      ref = &mFirst;
      index = beg + 1;
    }
    else
    {
      long check = pos / mCheckpointFreq;
      index = check * mCheckpointFreq + 1;
      ref = &mCheckpoints[check];
    }

    Checkpoint check(*ref);
    check.filter(aFilter);
    
    // Roll forward from the checkpoint.
    for (; index <= (unsigned long) pos; index++)
    {
      check.addComponentEvent(((*mSlidingBuffer)[(unsigned long)index]).getObject());
    }

    check.getComponentEvents(events);
  }
  
  string toReturn = XmlPrinter::printSample(mInstanceId, mSlidingBufferSize,
					    mSequence, firstSeq, events);
  
  return toReturn;
}

string Agent::fetchSampleData(std::set<string> &aFilter,
                              unsigned int start,
                              unsigned int count,
                              unsigned int &items
  )
{
  vector<ComponentEventPtr> results;
  
  dlib::auto_mutex lock(*mSequenceLock);

  unsigned int seq = mSequence;
  unsigned int firstSeq = (mSequence > mSlidingBufferSize) ?
    mSequence - mSlidingBufferSize : 1;
  
  // START SHOULD BE BETWEEN 0 AND SEQUENCE NUMBER
  start = (start <= firstSeq) ? firstSeq : start;
  unsigned int end = (count + start >= mSequence) ? mSequence : count + start;
  items = 0;

  for (unsigned int i = start; i < end; i++)
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

void Agent::getDataItems(std::set<string> &aFilterSet,
			 const string& path, xmlpp::Node * node)
{
  if (node == NULL)
  {
    node = mConfig->getRootNode();
  }

  xmlpp::NodeSet elements;
  if (node->get_namespace_uri().empty())
    elements = node->find(path);
  else
  {
    xmlpp::Node::PrefixNsMap spaces;
    spaces.insert(
      pair<Glib::ustring, Glib::ustring>("m", node->get_namespace_uri())
      );
    elements = node->find(addNamespace(path, "m"), spaces);
  }
    
  for (unsigned int i = 0; i < elements.size(); i++)
  {
    const xmlpp::Element *nodeElement =
      dynamic_cast<const xmlpp::Element *>(elements[i]);
    
    if (nodeElement)
    {
      const string nodename = nodeElement->get_name();
      if (nodeElement->get_name() == "DataItem")
      {
	aFilterSet.insert(nodeElement->get_attribute_value("id"));
      }
      else if (nodename == "Components" || nodename == "DataItems")
      {
        // Recursive call
        getDataItems(aFilterSet, "*", elements[i]);
      }
      else // Hopefully a subtype of component.
      {
        getDataItems(aFilterSet, "Components/*|DataItems/*", elements[i]);
      }
    }
  }
}

int Agent::checkAndGetParam(
    string& result,
    const map_type& queries,
    const string& param,
    const int defaultValue,
    const int minValue,
    bool minError,
    const int maxValue
  )
{
  if (!queries.is_in_domain(param))
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

DataItem * Agent::getDataItemByName(const string& device, const string& name)
{
  Device *dev = mDeviceMap[device];
  return (dev) ? dev->getDeviceDataItem(name) : NULL;
}

