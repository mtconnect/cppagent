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

#ifndef COMPONENT_EVENT_TEST_HPP
#define COMPONENT_EVENT_TEST_HPP

#include <map>
#include <string>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "config.hpp"

class ConfigTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(ConfigTest);
  CPPUNIT_TEST(testBlankConfig);
  CPPUNIT_TEST(testBufferSize);
  CPPUNIT_TEST(testDevice);
  CPPUNIT_TEST(testAdapter);
  CPPUNIT_TEST(testDefaultPreserveUUID);
  CPPUNIT_TEST(testDefaultPreserveOverride);
  CPPUNIT_TEST(testDisablePut);
  CPPUNIT_TEST(testLimitPut);
  CPPUNIT_TEST(testLimitPutFromHosts);
  CPPUNIT_TEST(testNamespaces);
  CPPUNIT_TEST(testLegacyTimeout);
  CPPUNIT_TEST(testIgnoreTimestamps);
  CPPUNIT_TEST(testIgnoreTimestampsOverride);
  CPPUNIT_TEST(testSpecifyMTCNamespace);
  CPPUNIT_TEST(testSetSchemaVersion);
  CPPUNIT_TEST(testSchemaDirectory);
  CPPUNIT_TEST(testLogFileRollover);
  CPPUNIT_TEST(testMaxSize);
  CPPUNIT_TEST(testConfigFileRelativePaths);
  CPPUNIT_TEST_SUITE_END();
  
public:
  void setUp();
  void tearDown();
  
protected:
  AgentConfiguration *mConfig;
  
protected:
  void testBlankConfig();
  void testBufferSize();
  void testDevice();
  void testAdapter();
  void testDefaultPreserveUUID();
  void testDefaultPreserveOverride();
  void testDisablePut();
  void testLimitPut();
  void testLimitPutFromHosts();
  void testNamespaces();
  void testLegacyTimeout();
  void testIgnoreTimestamps();
  void testIgnoreTimestampsOverride();
  void testSpecifyMTCNamespace();
  void testSetSchemaVersion();
  void testSchemaDirectory();
  void testLogFileRollover();
  void testMaxSize();
  void testConfigFileRelativePaths();
};

#endif

