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

#ifndef AGENT_TEST_HPP
#define AGENT_TEST_HPP

#include <map>
#include <string>

#include "dlib/md5.h"
#include "dlib/server.h"
#include "dlib/sliding_buffer.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "adapter.hpp"
#include "agent.hpp"
#include "test_globals.hpp"

class AgentTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(AgentTest);
  CPPUNIT_TEST(testConstructor);
  CPPUNIT_TEST(testBadPath);
  CPPUNIT_TEST(testProbe);
  CPPUNIT_TEST(testBadXPath);
  CPPUNIT_TEST(testBadCount);
  CPPUNIT_TEST(testBadFreq);
  CPPUNIT_TEST(testGoodPath);
  CPPUNIT_TEST(testEmptyStream);
  CPPUNIT_TEST(testBadDevices);
  CPPUNIT_TEST(testAddAdapter);
  CPPUNIT_TEST(testAddToBuffer);
  CPPUNIT_TEST(testAdapter);
  CPPUNIT_TEST(testCurrentAt);
  CPPUNIT_TEST(testCurrentAt64);
  CPPUNIT_TEST(testCurrentAtOutOfRange);
  CPPUNIT_TEST(testSampleAtNextSeq);
  CPPUNIT_TEST(testAdapterCommands);
  CPPUNIT_TEST(testFileDownload);
  CPPUNIT_TEST(testFailedFileDownload);
  CPPUNIT_TEST_SUITE_END();
  
  typedef map<std::string, std::string>::kernel_1a_c map_type;
  typedef queue<std::string>::kernel_1a_c queue_type;
  
protected:
  Agent *a;
  Adapter *adapter;
  std::string agentId;
  
  bool response;
  std::string path;
  std::string at;
  Agent::key_value_map queries;
  std::string result;
  Agent::key_value_map cookies;
  queue_type new_cookies;
  Agent::key_value_map incoming_headers;
  std::string foreign_ip;
  std::string local_ip;
  unsigned short foreign_port;
  unsigned short local_port;
  std::ostringstream out;
  
protected:
  /* Test Basic */
  void testConstructor();
  
  /* Test Errors */
  void testBadPath();
  void testBadXPath();
  void testBadCount();
  void testBadFreq();

  /* test good */
  void testGoodPath();
  
  /* Test calls */
  void testProbe();
  void testEmptyStream();
  void testBadDevices();
  void testAddAdapter();
  void testAddToBuffer();
  void testCurrentAt();
  void testCurrentAt64();
  void testCurrentAtOutOfRange();
  void testSampleAtNextSeq();
  
  /* Test Adapter */
  void testAdapter();

  // File handler tests
  void testFileDownload();
  void testFailedFileDownload();
  
  // Adapter commands
  void testAdapterCommands();
  
  /* Helper method to test expected string, given optional query, & run tests */
  xmlDocPtr responseHelper(CPPUNIT_NS::SourceLine sourceLine,
                           std::string key = "",
                           std::string value = "");

  
public:
  void setUp();
  void tearDown();
};

#define PARSE_XML_RESPONSE \
  xmlDocPtr doc = responseHelper(CPPUNIT_SOURCELINE()); \
  CPPUNIT_ASSERT(doc);

#define PARSE_XML_RESPONSE_QUERY(key, value) \
  xmlDocPtr doc = responseHelper(CPPUNIT_SOURCELINE(), key, value); \
  CPPUNIT_ASSERT(doc);

#endif

