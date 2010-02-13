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
#include <list>
#include <map>

#include "dlib/md5.h"
#include "dlib/server.h"
#include "dlib/sliding_buffer.h"

#include <libxml++/libxml++.h>

#include "adapter.hpp"
#include "globals.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"

class Adapter;
class ComponentEvent;
class DataItem;
class Device;

using namespace dlib;

class Agent : public server::http_1a_c
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
  
public:
  /* Load agent with the xml configuration */
  Agent(const std::string& configXmlPath, int aBufferSize);
  
  /* Virtual destructor */
  virtual ~Agent();
  
  /* Overridden method that is called per web request */  
  bool on_request(
    const req_type rtype,
    const std::string& path,
    std::string& result,
    const map_type& queries,
    const map_type& cookies,
    queue_type& new_cookies,
    const map_type& incoming_headers,
    map_type& response_headers,
    const std::string& foreign_ip,
    const std::string& local_ip,
    unsigned short foreign_port,
    unsigned short local_port,
    std::ostream& out // Added to allow streaming of data
  );
  
  /* Add an adapter to the agent */
  Adapter * addAdapter(
    const std::string& device,
    const std::string& host,
    const unsigned int port
  );
  
  /* Get device from device map */
  Device * getDeviceByName(const std::string& name) { return mDeviceMap[name]; } 
  
  /* Add component events to the sliding buffer */
  unsigned int addToBuffer(
    DataItem *dataItem,
    const std::string& value,
    std::string time = ""
  );
  
  /* Message when adapter has disconnected */
  void disconnected(Adapter *anAdapter, const std::string aDevice);
  
  DataItem * getDataItemByName(
    const std::string& device,
    const std::string& name
  );
  
protected:
  /* HTTP methods to handle the 3 basic calls */
  bool handleCall(
    std::ostream& out,
    const std::string& path,
    std::string& result,
    const map_type& queries,
    const std::string& call,
    const std::string& device = ""
  );
  
  /* Handle probe calls */
  std::string handleProbe(const std::string& device);
  
  /* Handle stream calls, which includes both current and sample */
  bool handleStream(
    std::ostream& out,
    std::string& result,
    const std::string& path,
    bool current,  
    unsigned int frequency,
    unsigned int start = 0,
    unsigned int count = 0
  );
  
  /* Stream the data to the user */
  void streamData(
    std::ostream& out,
    std::list<DataItem *>& dataItems,
    bool current,
    unsigned int frequency,
    unsigned int start = 1,
    unsigned int count = 0
  );
  
  /* Fetch the current/sample data and return the XML in a std::string */
  std::string fetchCurrentData(std::list<DataItem *>& dataItems);
  std::string fetchSampleData(
    std::list<DataItem *>& dataItems,
    unsigned int start,
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
  
  /* Get std::list of data items in path */
  std::list<DataItem *> getDataItems(
    const std::string& path,
    xmlpp::Node * node = NULL
  );
  
  /* Perform a check on parameter and return a value or a code */
  int checkAndGetParam(
    std::string& result,
    const map_type& queries,
    const std::string& param,
    const int defaultValue,
    const int minValue = NO_VALUE,
    bool minError = false,
    const int maxValue = NO_VALUE
  );
  
  /* Find data items by name/id */
  DataItem * getDataItemById(const std::string& id) { return mDataItemMap[id]; }

  /* Find if there's data item with that name/source name */
  bool hasDataItem(std::list<DataItem *>& dataItems, const std::string& name);
    
protected:
  /* Unique id based on the time of creation */
  unsigned int mInstanceId;
  
  /* Pointer to the configuration file for node access */
  XmlParser *mConfig;
  
  /* For access to the sequence number and sliding buffer, use the mutex */
  dlib::mutex *mSequenceLock;
  
  /* Sequence number */
  unsigned int mSequence;
  
  /* The sliding/circular buffer to hold all of the events/sample data */
  dlib::sliding_buffer_kernel_1<ComponentEvent *> *mSlidingBuffer;
  
  unsigned int mSlidingBufferSize;
  
  /* Data containers */
  std::list<Device *> mDevices;
  std::map<std::string, Device *> mDeviceMap;
  std::map<std::string, DataItem *> mDataItemMap;
};

#endif

