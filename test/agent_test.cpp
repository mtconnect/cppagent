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
  a = new Agent("../samples/test_config.xml");
  agentId = intToString(getCurrentTimeInSec());
  adapter = NULL;
}

void AgentTest::tearDown()
{
  delete a;
}

void AgentTest::testConstructor()
{
  CPPUNIT_ASSERT_THROW(Agent("../samples/badPath.xml"), string);
  CPPUNIT_ASSERT_NO_THROW(Agent("../samples/test_config.xml"));
}

void AgentTest::testBadPath()
{
  string pathError = getFile("../samples/test_error.xml");
  
  fillAttribute(pathError, "errorCode", "UNSUPPORTED");
  fillAttribute(pathError, "instanceId", agentId);
  fillAttribute(pathError, "bufferSize", "131072");
  fillAttribute(pathError, "creationTime", getCurrentTime(GMT));
  
  path = "/bad_path";
  fillErrorText(pathError, "The following path is invalid: " + path);
  responseHelper(pathError);
  
  path = "/bad/path/";
  fillErrorText(pathError, "The following path is invalid: " + path);
  responseHelper(pathError);
    
  path = "/LinuxCNC/current/blah";
  fillErrorText(pathError, "The following path is invalid: " + path);
  responseHelper(pathError);
}

void AgentTest::testBadXPath()
{
  string errorStr = getFile("../samples/test_error.xml");
  fillAttribute(errorStr, "instanceId", agentId);
  fillAttribute(errorStr, "bufferSize", "131072");
  fillAttribute(errorStr, "creationTime", getCurrentTime(GMT));
  fillAttribute(errorStr, "errorCode", "INVALID_XPATH");
  
  string key("path"), value;
  path = "/current";
  
  value = "//////Linear";
  fillErrorText(errorStr, "Invalid XPath: " + value);
  responseHelper(errorStr, key, value);
  
  value = "//Axes?//Linear";
  fillErrorText(errorStr, "Invalid XPath: " + value);
  responseHelper(errorStr, key, value);
  
  value = "//Devices/Device[@name=\"I_DON'T_EXIST\"";
  fillErrorText(errorStr, "Invalid XPath: " + value);
  responseHelper(errorStr, key, value);
}

void AgentTest::testBadCount()
{
  string errorStr = getFile("../samples/test_error.xml");
  fillAttribute(errorStr, "instanceId", agentId);
  fillAttribute(errorStr, "bufferSize", "131072");
  fillAttribute(errorStr, "creationTime", getCurrentTime(GMT));
  fillAttribute(errorStr, "errorCode", "QUERY_ERROR");
  
  path = "/sample";
  string key("count"), value;
  
  //value = "";
  //fillErrorText(errorStr, "'count' cannot be empty.");
  //responseHelper(errorStr, key, value);
  
  value = "NON_INTEGER";
  fillErrorText(errorStr, "'count' must be a positive integer.");
  responseHelper(errorStr, key, value);
  
  value = "-123";
  responseHelper(errorStr, key, value);
  
  value = "0";
  fillErrorText(errorStr, "'count' must be greater than 1.");
  responseHelper(errorStr, key, value);
  
  value = "999999999999999999";
  fillErrorText(errorStr, "'count' must be less than 131072.");
  responseHelper(errorStr, key, value);
}

void AgentTest::testBadFreq()
{
  string errorStr = getFile("../samples/test_error.xml");
  fillAttribute(errorStr, "instanceId", agentId);
  fillAttribute(errorStr, "bufferSize", "131072");
  fillAttribute(errorStr, "creationTime", getCurrentTime(GMT));
  fillAttribute(errorStr, "errorCode", "QUERY_ERROR");
  
  path = "/sample";
  string key("frequency"), value;
  
  /*value = "";
  fillErrorText(errorStr, "'freq' cannot be empty.");
  responseHelper(errorStr, key, value);*/
  
  value = "NON_INTEGER";
  fillErrorText(errorStr, "'frequency' must be a positive integer.");
  responseHelper(errorStr, key, value);
  
  value = "-123";
  responseHelper(errorStr, key, value);
  
  value = "999999999999999999";
  fillErrorText(errorStr, "'frequency' must be less than 2147483646.");
  responseHelper(errorStr, key, value);
  
  path = "/current";
  responseHelper(errorStr, key, value);
}

void AgentTest::testProbe()
{
  string probeStr = getFile("../samples/test_probe.xml");
  
  fillAttribute(probeStr, "instanceId", agentId);
  fillAttribute(probeStr, "bufferSize", "131072");
  fillAttribute(probeStr, "creationTime", getCurrentTime(GMT));
  
  path = "/probe";
  responseHelper(probeStr);
    
  path = "/";
  responseHelper(probeStr);
  
  path = "/LinuxCNC";
  responseHelper(probeStr);
  
  path = "/LinuxCNC/probe";
  responseHelper(probeStr);
  
  string key("device"), value("LinuxCNC");
  path = "/probe";
  responseHelper(probeStr, key, value);
}

void AgentTest::testEmptyStream()
{
  string emptyStr = getFile("../samples/test_empty_stream.xml");
  
  fillAttribute(emptyStr, "instanceId", agentId);
  fillAttribute(emptyStr, "bufferSize", "131072");
  fillAttribute(emptyStr, "creationTime", getCurrentTime(GMT));
  
  path = "/current";
  responseHelper(emptyStr);
  
  path = "/sample";
  responseHelper(emptyStr);
}

void AgentTest::testBadDevices()
{
  string key("device"), value("LinuxCN");
  
  string errorStr = getFile("../samples/test_error.xml");
  
  fillAttribute(errorStr, "instanceId", agentId);
  fillAttribute(errorStr, "bufferSize", "131072");
  fillAttribute(errorStr, "creationTime", getCurrentTime(GMT));
  
  fillAttribute(errorStr, "errorCode", "NO_DEVICE");
  fillErrorText(errorStr, "Could not find the device '" + value + "'");
  
  path = "/probe";
  responseHelper(errorStr, key, value);
  
  path = "/LinuxCN/probe";
  responseHelper(errorStr);
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
  
  DataItem *di1 = a->getDataItemByName(device, key);
  seqNum = a->addToBuffer(di1, key, value);
  CPPUNIT_ASSERT_EQUAL(0, seqNum);
  
  string emptyStr = getFile("../samples/test_empty_stream.xml");
  
  fillAttribute(emptyStr, "instanceId", agentId);
  fillAttribute(emptyStr, "bufferSize", "131072");
  fillAttribute(emptyStr, "creationTime", getCurrentTime(GMT));
  
  path = "/current";
  responseHelper(emptyStr);
  
  path = "/sample";
  responseHelper(emptyStr);
  
  key = "power";

  DataItem *di2 = a->getDataItemByName(device, key);
  seqNum = a->addToBuffer(di2, key, value);
  CPPUNIT_ASSERT_EQUAL(1, seqNum);
  
  // Streams are no longer empty
  path = "/current";
  response = a->on_request(path, result, queries, cookies, new_cookies,
    incoming_headers, response_headers, foreign_ip, local_ip, 123, 321, out);
  
  CPPUNIT_ASSERT(response);
  CPPUNIT_ASSERT(emptyStr != result);
  
  path = "/sample";
  response = a->on_request(path, result, queries, cookies, new_cookies,
    incoming_headers, response_headers, foreign_ip, local_ip, 123, 321, out);
  
  CPPUNIT_ASSERT(response);
  CPPUNIT_ASSERT(emptyStr != result);
}

void AgentTest::testAdapter()
{
  path = "/sample";
  
  adapter = a->addAdapter("LinuxCNC", "server", 7878);
  CPPUNIT_ASSERT(adapter);
  
  response = a->on_request(path, result, queries, cookies, new_cookies,
    incoming_headers, response_headers, foreign_ip, local_ip, 123, 321, out);
  
  CPPUNIT_ASSERT(string::npos == result.find("line"));
  
  adapter->processData("TIME|line|204");
  response = a->on_request(path, result, queries, cookies, new_cookies,
    incoming_headers, response_headers, foreign_ip, local_ip, 123, 321, out);
  
  CPPUNIT_ASSERT(string::npos != result.find("line"));
  CPPUNIT_ASSERT(string::npos == result.find("alarm"));
  
  adapter->processData("TIME|alarm|code|nativeCode|severity|state|description");
  response = a->on_request(path, result, queries, cookies, new_cookies,
    incoming_headers, response_headers, foreign_ip, local_ip, 123, 321, out);
  
  CPPUNIT_ASSERT(string::npos != result.find("alarm"));
}

void AgentTest::responseHelper(
    const string& expected,
    string key,
    string value
  )
{
  bool query = !key.empty() and !value.empty();
  
  if (query)
  {
    queries.add(key, value);
  }
  
  response = a->on_request(path, result, queries, cookies, new_cookies,
    incoming_headers, response_headers, foreign_ip, local_ip, 123, 321, out);
  
  CPPUNIT_ASSERT(response);
  CPPUNIT_ASSERT_EQUAL(expected, result);
  
  if (query)
  {
    queries.remove_any(key, value);
  }
}

