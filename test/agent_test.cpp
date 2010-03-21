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

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(AgentTest);

using namespace std;

void AgentTest::setUp()
{
  a = new Agent("../samples/test_config.xml", 8, 25);
  agentId = intToString(getCurrentTimeInSec());
  adapter = NULL;
}

void AgentTest::tearDown()
{
  delete a;
}

void AgentTest::testConstructor()
{
  CPPUNIT_ASSERT_THROW(Agent("../samples/badPath.xml", 17), string);
  CPPUNIT_ASSERT_NO_THROW(Agent("../samples/test_config.xml", 17));
}

void AgentTest::testBadPath()
{
  string pathError = getFile("../samples/test_error.xml");
    
  {
    path = "/bad_path";
    PARSE_XML_RESPONSE
    string message = (string) "The following path is invalid: " + path;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "UNSUPPORTED");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", message.c_str());
  }
  
  {
    path = "/bad/path/";
    PARSE_XML_RESPONSE
    string message = (string) "The following path is invalid: " + path;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "UNSUPPORTED");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", message.c_str());
  }
  
  {
    path = "/LinuxCNC/current/blah";
    PARSE_XML_RESPONSE
    string message = (string) "The following path is invalid: " + path;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "UNSUPPORTED");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", message.c_str());
  }
}

void AgentTest::testBadXPath()
{
  string key("path"), value;
  path = "/current";
  
  {
    value = "//////Linear";
    PARSE_XML_RESPONSE_QUERY(key, value)
    string message = (string) "Invalid XPath: //m://m://m:Linear";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "INVALID_XPATH");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", message.c_str());
  }
  
  {
    value = "//Axes?//Linear";
    PARSE_XML_RESPONSE_QUERY(key, value)
    string message = (string) "Invalid XPath: //m:Axes?//m:Linear";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "INVALID_XPATH");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", message.c_str());
  }
  
  {
    value = "//Devices/Device[@name=\"I_DON'T_EXIST\"";
    PARSE_XML_RESPONSE_QUERY(key, value)
    string message = (string) "Invalid XPath: //m:Devices/m:Device[@name=\"I_DON'T_EXIST\"";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "INVALID_XPATH");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", message.c_str());
  }
}

void AgentTest::testBadCount()
{
  path = "/sample";
  string key("count"), value;
  
  {
    value = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY(key, value)
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "QUERY_ERROR");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", "'count' must be a positive integer.");
  }
  
  {
    value = "-123";
    PARSE_XML_RESPONSE_QUERY(key, value)
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "QUERY_ERROR");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", "'count' must be a positive integer.");
  }
  
  {
    value = "0";
    PARSE_XML_RESPONSE_QUERY(key, value)
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "QUERY_ERROR");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", "'count' must be greater than or equal to 1.");
  }
  
  {
    value = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY(key, value)
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "QUERY_ERROR");
    string value("'count' must be less than or equal to ");
    value += intToString(a->getBufferSize()) + ".";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", value.c_str());
  }

}

void AgentTest::testBadFreq()
{
  path = "/sample";
  string key("frequency"), value;
  
  {
    value = "NON_INTEGER";
    PARSE_XML_RESPONSE_QUERY(key, value)
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "QUERY_ERROR");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", "'frequency' must be a positive integer.");
  }
  
  {
    value = "-123";
    PARSE_XML_RESPONSE_QUERY(key, value)
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "QUERY_ERROR");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", "'frequency' must be a positive integer.");
  }
  
  {
    value = "999999999999999999";
    PARSE_XML_RESPONSE_QUERY(key, value)
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "QUERY_ERROR");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", "'frequency' must be less than or equal to 2147483646.");
  }
}

void AgentTest::testProbe()
{
  {
    path = "/probe";
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Devices/m:Device@name", "LinuxCNC");
  }
  
  {
    path = "/";
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Devices/m:Device@name", "LinuxCNC");
  }
  
  {
    path = "/LinuxCNC";
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Devices/m:Device@name", "LinuxCNC");
  }
}

void AgentTest::testEmptyStream()
{
  {
    path = "/current";
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:PowerStatus", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:ComponentStream[@name='path']/m:Condition/m:Unavailable", 0);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:ComponentStream[@name='path']/m:Condition/m:Unavailable@qualifier",
				      0);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:RotaryMode", "SPINDLE");
  }
  
  {
    path = "/sample";
    char line[80];
    sprintf(line, "%d", (int) a->getSequence());
    PARSE_XML_RESPONSE_QUERY("from", line);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Streams", 0);
  }
}

void AgentTest::testBadDevices()
{
  {
    path = "/LinuxCN/probe";
    PARSE_XML_RESPONSE
    string message = (string) "Could not find the device 'LinuxCN'";
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "NO_DEVICE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", message.c_str());
  }
}

void AgentTest::testAddAdapter()
{
  CPPUNIT_ASSERT(adapter == NULL);
  adapter = a->addAdapter("LinuxCNC", "server", 7878);
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
    path = "/current";
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream", "");
  }
  
  {
    path = "/sample";
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream", "");
  }
  
  key = "power";

  DataItem *di2 = a->getDataItemByName(device, key);
  seqNum = a->addToBuffer(di2, value, "NOW");
  event2 = a->getFromBuffer(seqNum);
  CPPUNIT_ASSERT_EQUAL(2, (int) event2->refCount());
  
  {
    path = "/current";
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:PowerStatus", "ON");
  }
 
  {
    path = "/sample";
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:PowerStatus[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:PowerStatus[2]", "ON");
  }
}

void AgentTest::testAdapter()
{
  path = "/sample";
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878);
  CPPUNIT_ASSERT(adapter);
    
  {
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
  }
  
  adapter->processData("TIME|line|204");
  
  {
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Line[2]", "204");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
  }
  
  adapter->processData("TIME|alarm|code|nativeCode|severity|state|description");

  {
    PARSE_XML_RESPONSE
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Line[2]", "204");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Alarm[2]", "description");
  }
}


void AgentTest::responseHelper(xmlpp::DomParser *parser,
                               CPPUNIT_NS::SourceLine sourceLine,
                               string key,
                               string value)
{
  bool query = !key.empty() and !value.empty();
  
  if (query)
  {
    queries.add(key, value);
  }
  
  response = a->on_request(Agent::get, path, result, queries, cookies, new_cookies,
    incoming_headers, response_headers, foreign_ip, local_ip, 123, 321, out);

  string message = (string) "No response to request" + path + " with: (" + key + ", " + value + ")";

  CPPUNIT_NS::Asserter::failIf(!response, message, sourceLine);
  
  if (query)
  {
    queries.remove_any(key, value);
  }
  
  parser->parse_memory(result);
}

void AgentTest::testCurrentAt()
{
  path = "/current";
  string key("at"), value;
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878);
  CPPUNIT_ASSERT(adapter);

  // Get the current position
  int seq = a->getSequence();
  char line[80];

  // Add a large many events
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
    PARSE_XML_RESPONSE_QUERY(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Line", line);
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
    PARSE_XML_RESPONSE_QUERY(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Line", line);
  }

  // Check the first couple of items in the list
  for (int j = 0; j < 10; j ++)
  {
    int i = a->getSequence() - a->getBufferSize() - seq + j;       
    value = intToString(i + seq);
    sprintf(line, "%d", i + 1);
    PARSE_XML_RESPONSE_QUERY(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:DeviceStream//m:Line", line);
  }
  
  // Test out of range...
  {
    int i = a->getSequence() - a->getBufferSize() - seq - 1;       
    value = intToString(i + seq);
    sprintf(line, "'at' must be greater than or equal to %d.", i + seq + 1);
    PARSE_XML_RESPONSE_QUERY(key, value);
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error@errorCode", "QUERY_ERROR");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(root, "//m:Error", line);
  }
}

