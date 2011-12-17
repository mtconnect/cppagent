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

#include "agent_test.hpp"
#include <stdexcept>
#include <iostream>
#include <sstream>


#if defined(WIN32) && _MSC_VER < 1500
typedef __int64 int64_t;
#endif

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(AgentTest);

using namespace std;

void AgentTest::setUp()
{
  a = NULL;
  a = new Agent("../samples/test_config.xml", 8, 4, 25);
  agentId = intToString(getCurrentTimeInSec());
  adapter = NULL;
  queries.clear();
}

void AgentTest::tearDown()
{
  delete a;
  a = NULL;
}

void AgentTest::testConstructor()
{
#ifndef WIN32
  CPPUNIT_ASSERT_THROW(Agent("../samples/badPath.xml", 17, 8), std::exception);
#endif
  CPPUNIT_ASSERT_NO_THROW(Agent("../samples/test_config.xml", 17, 8));
}

void AgentTest::testBadPath()
{
  string pathError = getFile("../samples/test_error.xml");
    
  {
    path = "/bad_path";
    PARSE_XML_RESPONSE;
    string message = (string) "The following path is invalid: " + path;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
  
  {
    path = "/bad/path/";
    PARSE_XML_RESPONSE;
    string message = (string) "The following path is invalid: " + path;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
  
  {
    path = "/LinuxCNC/current/blah";
    PARSE_XML_RESPONSE;
    string message = (string) "The following path is invalid: " + path;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
}

void AgentTest::testBadXPath()
{
  path = "/current";
  Agent::key_value_map query;
  
  {
    query["path"] = "//////Linear";
    PARSE_XML_RESPONSE_QUERY(query);
    string message = (string) "The path could not be parsed. Invalid syntax: //////Linear";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
  
  {
    query["path"] = "//Axes?//Linear";
    PARSE_XML_RESPONSE_QUERY(query);
    string message = (string) "The path could not be parsed. Invalid syntax: //Axes?//Linear";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
  
  {
    query["path"] = "//Devices/Device[@name=\"I_DON'T_EXIST\"";
    PARSE_XML_RESPONSE_QUERY(query);
    string message = (string) "The path could not be parsed. Invalid syntax: //Devices/Device[@name=\"I_DON'T_EXIST\"";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
}

void AgentTest::testBadCount()
{
  path = "/sample";
  Agent::key_value_map query;

  {
    query["count"] = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must be a positive integer.");
  }
  
  {
    query["count"] = "-123";
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must be a positive integer.");
  }
  
  {
    query["count"] = "0";
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must be greater than or equal to 1.");
  }
  
  {
    query["count"] = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    string value("'count' must be less than or equal to ");
    value += intToString(a->getBufferSize()) + ".";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
  }

}

void AgentTest::testBadFreq()
{
  path = "/sample";
  Agent::key_value_map query;
  
  {
    query["frequency"] = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'frequency' must be a positive integer.");
  }
  
  {
    query["frequency"] = "-123";
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'frequency' must be a positive integer.");
  }
  
  {
    query["frequency"] = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'frequency' must be less than or equal to 2147483646.");
  }
}

void AgentTest::testGoodPath()
{
  {
    path = "/current?path=//Power";
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Power']//m:PowerState",
				      "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Path']//m:Condition",
				      "");
  }
}

void AgentTest::testProbe()
{
  {
    path = "/probe";
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }
  
  {
    path = "/";
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }
  
  {
    path = "/LinuxCNC";
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
  }
}

void AgentTest::testEmptyStream()
{
  {
    path = "/current";
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Condition/m:Unavailable", 0);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Condition/m:Unavailable@qualifier",
				      0);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:RotaryMode", "SPINDLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ToolGroup", "UNAVAILABLE");
  }
  
  {
    path = "/sample";
    char line[80];
    sprintf(line, "%d", (int) a->getSequence());
    PARSE_XML_RESPONSE_QUERY_KV("from", line);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", 0);
  }
}

void AgentTest::testBadDevices()
{
  {
    path = "/LinuxCN/probe";
    PARSE_XML_RESPONSE;
    string message = (string) "Could not find the device 'LinuxCN'";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "NO_DEVICE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
  }
}

void AgentTest::testAddAdapter()
{
  CPPUNIT_ASSERT(adapter == NULL);
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);
}

void AgentTest::testAddToBuffer()
{
  string device("LinuxCNC"), key("badKey"), value("ON");
  int seqNum;  
  ComponentEvent *event1, *event2;
  DataItem *di1 = a->getDataItemByName(device, key);
  seqNum = a->addToBuffer(di1, value, "NOW");
  CPPUNIT_ASSERT_EQUAL(0, seqNum);
  
  event1 = a->getFromBuffer(seqNum);
  CPPUNIT_ASSERT(NULL == event1);
  
  {
    path = "/sample";
    PARSE_XML_RESPONSE_QUERY_KV("from", "31");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", 0);
  }
  
  key = "power";

  DataItem *di2 = a->getDataItemByName(device, key);
  seqNum = a->addToBuffer(di2, value, "NOW");
  event2 = a->getFromBuffer(seqNum);
  CPPUNIT_ASSERT_EQUAL(2, (int) event2->refCount());
  
  {
    path = "/current";
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState", "ON");
  }
 
  {
    path = "/sample";
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState[2]", "ON");
  }
}

void AgentTest::testAdapter()
{
  path = "/sample";
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);
    
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }
  
  adapter->processData("TIME|line|204");
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
  }
  
  adapter->processData("TIME|alarm|code|nativeCode|severity|state|description");

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[2]", "DESCRIPTION");
  }
}

void AgentTest::testCurrentAt()
{
  path = "/current";
  string key("at"), value;
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);

  // Get the current position
  int seq = a->getSequence();
  char line[80];

  // Add many events
  for (int i = 1; i <= 100; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    adapter->processData(line);
  }

  // Check each current at all the positions.
  for (int i = 0; i < 100; i++)
  {
    value = intToString(i + seq);
    sprintf(line, "%d", i + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }

  // Test buffer wrapping
  // Add a large many events
  for (int i = 101; i <= 301; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    adapter->processData(line);
  }

  // Check each current at all the positions.
  for (int i = 100; i < 301; i++)
  {
    value = intToString(i + seq);
    sprintf(line, "%d", i + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }

  // Check the first couple of items in the list
  for (int j = 0; j < 10; j ++)
  {
    int i = a->getSequence() - a->getBufferSize() - seq + j;       
    value = intToString(i + seq);
    sprintf(line, "%d", i + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }
  
  // Test out of range...
  {
    int i = a->getSequence() - a->getBufferSize() - seq - 1;       
    value = intToString(i + seq);
    sprintf(line, "'at' must be greater than or equal to %d.", i + seq + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
}

void AgentTest::testCurrentAt64()
{
  path = "/current";
  string key("at"), value;
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);

  // Get the current position
  char line[80];

  // Initialize the sliding buffer at a very large number.
  uint64_t start = (((uint64_t) 1) << 48) + 1317;
  a->setSequence(start);

  // Add many events
  for (uint64_t i = 1; i <= 500; i++)
  {
    sprintf(line, "TIME|line|%d", (int) i);
    adapter->processData(line);
  }

  // Check each current at all the positions.
  for (uint64_t i = start + 300; i < start + 500; i++)
  {
    value = int64ToString(i);
    sprintf(line, "%d", (int) (i - start) + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
  }
}


void AgentTest::testCurrentAtOutOfRange()
{
  path = "/current";
  string key("at"), value;
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);
  
  // Get the current position
  char line[80];
  
  // Add many events
  for (int i = 1; i <= 200; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    adapter->processData(line);
  }
  
  int seq = a->getSequence();

  {
    value = intToString(seq);
    sprintf(line, "'at' must be less than or equal to %d.", seq - 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
  
  seq = a->getFirstSequence() - 1;
  
  {
    value = intToString(seq);
    sprintf(line, "'at' must be greater than or equal to %d.", seq + 1);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
  }
}

void AgentTest::testSampleAtNextSeq()
{
  path = "/sample";
  string key("from"), value;
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);
  
  // Get the current position
  char line[80];
  
  // Add many events
  for (int i = 1; i <= 300; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    adapter->processData(line);
  }
  
  int seq = a->getSequence();
  
  {
    value = intToString(seq);
    PARSE_XML_RESPONSE_QUERY_KV(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", 0);
  }
}

void AgentTest::testSequenceNumberRollover()
{
  Agent::key_value_map kvm;

  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);

  // Set the sequence number near MAX_UINT32
  a->setSequence(0xFFFFFFA0);
  int64_t seq = a->getSequence();
  CPPUNIT_ASSERT_EQUAL((int64_t) 0xFFFFFFA0, seq);
  
  // Get the current position
  char line[80];
  
  // Add many events
  for (int i = 0; i < 128; i++)
  {
    sprintf(line, "TIME|line|%d", i);
    adapter->processData(line);
    
    {
      path = "/current";
      PARSE_XML_RESPONSE;
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence", 
                                        int64ToString(seq + i).c_str());
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
                                        int64ToString(seq + i + 1).c_str());
    }
    
    {
      path = "/sample";
      kvm["from"] = int64ToString(seq);
      kvm["count"] = "128";
      
      PARSE_XML_RESPONSE_QUERY(kvm);
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
                                        int64ToString(seq + i + 1).c_str());
      
      for (int j = 0; j <= i; j++)
      {
        sprintf(line, "//m:DeviceStream//m:Line[%d]@sequence", j + 1);
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, line, int64ToString(seq + j).c_str());
      }
    }
    
    for (int j = 0; j <= i; j++)
    {
      path = "/sample";
      kvm["from"] = int64ToString(seq + j);
      kvm["count"] = "1";
            
      PARSE_XML_RESPONSE_QUERY(kvm);
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence", 
                                        int64ToString(seq + j).c_str());
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
                                        int64ToString(seq + j + 1).c_str());
    }
  }

  CPPUNIT_ASSERT_EQUAL((uint64_t) 0xFFFFFFA0 + 128, a->getSequence());  
  
}


void AgentTest::testAdapterCommands()
{
  path = "/probe";

  Device *device = a->getDeviceByName("LinuxCNC");
  CPPUNIT_ASSERT(device);
  CPPUNIT_ASSERT(!device->mPreserveUuid);
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);

  adapter->parseBuffer("* uuid: MK-1234\n");
  adapter->parseBuffer("* manufacturer: Big Tool\n");
  adapter->parseBuffer("* serialNumber: XXXX-1234\n");
  adapter->parseBuffer("* station: YYYY\n");
    
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "XXXX-1234");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "YYYY");
  }

  device->mPreserveUuid = true;
  adapter->parseBuffer("* uuid: XXXXXXX\n");

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
  }
  
}

void AgentTest::testFileDownload()
{
  string uri("/schemas/MTConnectDevices_1.1.xsd");
  
  // Register a file with the agent.
  a->registerFile(uri, "./MTConnectDevices_1.1.xsd");

  // Reqyest the file...
  struct Agent::incoming_things incoming;
  struct Agent::outgoing_things outgoing;
  incoming.request_type = "GET";
  incoming.path = uri;
  incoming.queries = queries;
  incoming.cookies = cookies;
  incoming.headers = incoming_headers;
  
  outgoing.out = &out;
  
  result = a->on_request(incoming, outgoing);
  CPPUNIT_ASSERT_EQUAL((string) "TEST SCHEMA FILE 1234567890\n", result);
}

void AgentTest::testFailedFileDownload()
{
  string uri("/schemas/MTConnectDevices_1.1.xsd");
  
  // Register a file with the agent.
  a->registerFile(uri, "./BadFileName.xsd");
  
  // Reqyest the file...
  struct Agent::incoming_things incoming;
  struct Agent::outgoing_things outgoing;
  incoming.request_type = "GET";
  incoming.path = uri;
  incoming.queries = queries;
  incoming.cookies = cookies;
  incoming.headers = incoming_headers;
  
  outgoing.out = &out;
  
  result = a->on_request(incoming, outgoing);
  
  CPPUNIT_ASSERT_EQUAL((unsigned short) 404, outgoing.http_return);
  CPPUNIT_ASSERT_EQUAL((string) "File not found", outgoing.http_return_status);
}

void AgentTest::testDuplicateCheck()
{
  path = "/sample";
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);
  adapter->setDupCheck(true);
    
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }
  
  adapter->processData("TIME|line|204");
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
  }
  
  adapter->processData("TIME|line|204");
  adapter->processData("TIME|line|205");
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
  }
}

void AgentTest::testAutoAvailable()
{
  path = "/sample";
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);
  adapter->setAutoAvailable(true);
  Device *d = a->getDevices()[0];
  vector<Device*> devices;
  devices.push_back(d);
    
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
  }

  a->connected(adapter, devices);
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
  }

  a->disconnected(adapter, devices);
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[3]", "UNAVAILABLE");
  }

  a->connected(adapter, devices);
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[3]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[4]", "AVAILABLE");
  }

}

void AgentTest::testIgnoreTimestamps()
{
  path = "/sample";
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);
    
  adapter->processData("TIME|line|204");
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "TIME");
  }
  
  adapter->setIgnoreTimestamps(true);
  adapter->processData("TIME|line|205");

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "TIME");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]@timestamp", "!TIME");
  }
}

void AgentTest::testAssetStorage()
{
  a->enablePut();
  path = "/asset/123";
  string body = "<Part>TEST</Part>";
  Agent::key_value_map queries;
  
  queries["type"] = "Part";
  queries["device"] = "LinuxCNC";

  CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getMaxAssets());
  CPPUNIT_ASSERT_EQUAL((unsigned int) 0, a->getAssetCount());

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 1, a->getAssetCount());
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetBufferSize", "4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST");
  }
  
  // The device should generate an asset changed event as well.
  path = "/current";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged", "123");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@assetType", "Part");
  }
}

void AgentTest::testAssetBuffer()
{
  a->enablePut();
  path = "/asset/1";
  string body = "<Part>TEST 1</Part>";
  Agent::key_value_map queries;
  
  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getMaxAssets());
  CPPUNIT_ASSERT_EQUAL((unsigned int) 0, a->getAssetCount());
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 1, a->getAssetCount());
    CPPUNIT_ASSERT_EQUAL(1, a->getAssetCount("Part"));
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }
  
  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 1, a->getAssetCount());
    CPPUNIT_ASSERT_EQUAL(1, a->getAssetCount("Part"));
  }

  path = "/asset/2";
  body = "<Part>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 2, a->getAssetCount());
    CPPUNIT_ASSERT_EQUAL(2, a->getAssetCount("Part"));
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  path = "/asset/3";
  body = "<Part>TEST 3</Part>";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 3, a->getAssetCount());
    CPPUNIT_ASSERT_EQUAL(3, a->getAssetCount("Part"));
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  path = "/asset/4";
  body = "<Part>TEST 4</Part>";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getAssetCount());
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 4");
    CPPUNIT_ASSERT_EQUAL(4, a->getAssetCount("Part"));
  }
  
  // Test multiple asset get
  path = "/assets";
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 2");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 4");
  }

  // Test multiple asset get with filter
  path = "/assets";
  {
    PARSE_XML_RESPONSE_QUERY(queries);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 2");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 4");
  }

  
  path = "/asset/5";
  body = "<Part>TEST 5</Part>";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getAssetCount());
    CPPUNIT_ASSERT_EQUAL(4, a->getAssetCount("Part"));
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 5");
  }
  
  path = "/asset/1";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Could not find asset: 1");
  }
  
  path = "/asset/3";
  body = "<Part>TEST 6</Part>";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getAssetCount());
    CPPUNIT_ASSERT_EQUAL(4, a->getAssetCount("Part"));
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 6");
  }
  
  path = "/asset/2";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  path = "/asset/2";
  body = "<Part>TEST 7</Part>";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getAssetCount());
    CPPUNIT_ASSERT_EQUAL(4, a->getAssetCount("Part"));
  }

  path = "/asset/6";
  body = "<Part>TEST 8</Part>";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getAssetCount());
    CPPUNIT_ASSERT_EQUAL(4, a->getAssetCount("Part"));
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 8");
  }

  // Now since two and three have been modified, asset 4 should be removed.
  path = "/asset/4";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Could not find asset: 4");
  }
}


void AgentTest::testAssetError()
{
  path = "/asset/123";

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Could not find asset: 123");
  }
}

void AgentTest::testAdapterAddAsset()
{
  testAddAdapter();
  
  adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
  CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getMaxAssets());
  CPPUNIT_ASSERT_EQUAL((unsigned int) 1, a->getAssetCount());
  
  path = "/asset/111";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

}

void AgentTest::testMultiLineAsset()
{
  testAddAdapter();
  
  adapter->parseBuffer("TIME|@ASSET@|111|Part|--multiline--AAAA\n");
  adapter->parseBuffer("<Part>\n"
                       "  <PartXXX>TEST 1</PartXXX>\n");
  adapter->parseBuffer("</Part>\n"
                       "--multiline--AAAA\n");
  CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getMaxAssets());
  CPPUNIT_ASSERT_EQUAL((unsigned int) 1, a->getAssetCount());
  
  path = "/asset/111";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:PartXXX", "TEST 1");
  }  
  
  // Make sure we can still add a line and we are out of multiline mode...
  path = "/current";  
  adapter->processData("TIME|line|204");
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", "204");
  }

}

void AgentTest::testAssetProbe()
{
  a->enablePut();
  path = "/asset/1";
  string body = "<Part>TEST 1</Part>";
  Agent::key_value_map queries;
  
  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  path = "/asset/1";
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 1, a->getAssetCount());
  }
  path = "/asset/2";
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 2, a->getAssetCount());
  }
  
  {
    path = "/probe";
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount@assetType", "Part");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount", "2");
  }
}

void AgentTest::testAssetStorageWithoutType()
{
  a->enablePut();
  path = "/asset/123";
  string body = "<Part>TEST</Part>";
  Agent::key_value_map queries;
  
  queries["device"] = "LinuxCNC";
  
  CPPUNIT_ASSERT_EQUAL((unsigned int) 4, a->getMaxAssets());
  CPPUNIT_ASSERT_EQUAL((unsigned int) 0, a->getAssetCount());
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNIT_ASSERT_EQUAL((unsigned int) 0, a->getAssetCount());
  }
}

void AgentTest::testPut()
{
  Agent::key_value_map queries;
  string body;
  a->enablePut();

  queries["time"] = "TIME";
  queries["line"] = "205";
  queries["power"] = "ON";
  path = "/LinuxCNC";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
  }

  path = "/LinuxCNC/current";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line@timestamp", "TIME");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:PowerState", "ON");
  }
}

// Test diabling of HTTP PUT or POST
void AgentTest::testPutBlocking()
{
  Agent::key_value_map queries;
  string body;
  
  queries["time"] = "TIME";
  queries["line"] = "205";
  queries["power"] = "ON";
  path = "/LinuxCNC";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "Only the HTTP GET request is supported");
  }
}

// Test diabling of HTTP PUT or POST
void AgentTest::testPutBlockingFrom()
{
  Agent::key_value_map queries;
  string body;
  a->enablePut();
  
  incomingIp = "127.0.0.1";
  a->allowPutFrom("192.168.0.1");
  
  queries["time"] = "TIME";
  queries["line"] = "205";
  path = "/LinuxCNC";
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "HTTP PUT is not allowed from 127.0.0.1");
  } 
  
  path = "/LinuxCNC/current";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "UNAVAILABLE");
  }
  
  // Retry request after adding ip address
  path = "/LinuxCNC";
  a->allowPutFrom("127.0.0.1");
  
  {
    PARSE_XML_RESPONSE_PUT(body, queries);
  }
  
  path = "/LinuxCNC/current";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
  } 
}

void AgentTest::killThread(void *aArg)
{
  AgentTest *test = (AgentTest*) aArg;
  dlib::sleep(test->delay);
  test->out.setstate(ios::eofbit);
}


void AgentTest::addThread(void *aArg)
{
  AgentTest *test = (AgentTest*) aArg;
  dlib::sleep(test->delay);
  test->adapter->processData("TIME|line|204");
  test->out.setstate(ios::eofbit);
}


void AgentTest::testStreamData()
{  
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);

  // Start a thread...
  Agent::key_value_map query;
  query["interval"] = "50";
  query["heartbeat"] = "200";
  query["from"] = int64ToString(a->getSequence());
  path = "/LinuxCNC/sample";
  
  timestamper ts;
  
  // Heartbeat test. Heartbeat should be sent in 100ms. Give
  // 25ms range.
  {
    uint64 start = ts.get_timestamp();
    
    delay = 25;
    dlib::create_new_thread(killThread, this);
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", 0);
    
    int64 delta = ts.get_timestamp() - start;
    CPPUNIT_ASSERT(delta < 225000 && delta > 200000);
  }

  out.clear();
  out.str("");

  // Set some data and make sure we get data within 20ms. 
  // Again, allow for some slop.
  {
    uint64 start = ts.get_timestamp();
    
    delay = 10;
    dlib::create_new_thread(addThread, this);
    PARSE_XML_RESPONSE_QUERY(query);
    
    int64 delta = ts.get_timestamp() - start;
    CPPUNIT_ASSERT(delta < 70000 && delta > 50000);
  }
  
}

void AgentTest::streamThread(void *aArg)
{
  AgentTest *test = (AgentTest*) aArg;
  dlib::sleep(test->delay);
  test->a->setSequence(test->a->getSequence() + 20);
  test->adapter->processData("TIME|line|204");
  dlib::sleep(120);
  test->out.setstate(ios::eofbit);
}

void AgentTest::testStreamDataObserver()
{
  adapter = a->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(adapter);
  
  // Start a thread...
  Agent::key_value_map query;
  query["interval"] = "100";
  query["heartbeat"] = "1000";
  query["count"] = "10";
  query["from"] = int64ToString(a->getSequence());
  path = "/LinuxCNC/sample";
  
  // Test to make sure the signal will push the sequence number forward and capture   
  // the new data.
  {
    delay = 50;
    string seq = int64ToString(a->getSequence() + 20);
    dlib::create_new_thread(streamThread, this);
    PARSE_XML_RESPONSE_QUERY(query);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line@sequence", seq.c_str());
  } 
}

xmlDocPtr AgentTest::responseHelper(CPPUNIT_NS::SourceLine sourceLine,
                                    Agent::key_value_map &aQueries)
{  
  struct Agent::incoming_things incoming;
  struct Agent::outgoing_things outgoing;
  incoming.request_type = "GET";
  incoming.path = path;
  incoming.queries = aQueries;
  incoming.cookies = cookies;
  incoming.headers = incoming_headers;
  
  outgoing.out = &out;
  
  result = a->on_request(incoming, outgoing);
  if (result.empty()) 
  {
    result = out.str();
    size_t pos = result.rfind("\n--");
    if (pos != string::npos)
    {
      pos = result.find('<', pos);
      if (pos != string::npos)
        result.erase(0, pos);
    }
  }
  
  string message = (string) "No response to request" + path + " with: ";
  
  Agent::key_value_map::iterator iter;
  for (iter = aQueries.begin(); iter != aQueries.end(); ++iter)
    message += iter->first + "=" + iter->second + ",";
  
  CPPUNIT_NS::Asserter::failIf(outgoing.http_return != 200, message, sourceLine);
  
  return xmlParseMemory(result.c_str(), result.length());
}

xmlDocPtr AgentTest::putResponseHelper(CPPUNIT_NS::SourceLine sourceLine,
                                       string body, Agent::key_value_map &aQueries)
{
  struct Agent::incoming_things incoming;
  struct Agent::outgoing_things outgoing;
  incoming.request_type = "PUT";
  incoming.path = path;
  incoming.queries = aQueries;
  incoming.cookies = cookies;
  incoming.headers = incoming_headers;
  incoming.body = body;
  incoming.foreign_ip = incomingIp;
  
  outgoing.out = &out;
  
  result = a->on_request(incoming, outgoing);
  
  string message = (string) "No response to request" + path;
  
  CPPUNIT_NS::Asserter::failIf(outgoing.http_return != 200, message, sourceLine);
  
  return xmlParseMemory(result.c_str(), result.length());
}
