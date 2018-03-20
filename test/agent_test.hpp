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

#pragma once

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

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
  CPPUNIT_TEST(testXPath);
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
  CPPUNIT_TEST(testDuplicateCheck);
  CPPUNIT_TEST(testDuplicateCheckAfterDisconnect);
  CPPUNIT_TEST(testAutoAvailable);
  CPPUNIT_TEST(testIgnoreTimestamps);
  CPPUNIT_TEST(testAssetStorage);
  CPPUNIT_TEST(testAssetError);
  CPPUNIT_TEST(testAssetBuffer);
  CPPUNIT_TEST(testPut);
  CPPUNIT_TEST(testPutBlocking);
  CPPUNIT_TEST(testPutBlockingFrom);
  CPPUNIT_TEST(testAdapterAddAsset);
  CPPUNIT_TEST(testMultiLineAsset);
  CPPUNIT_TEST(testAssetRefCounts);
  CPPUNIT_TEST(testAssetProbe);
  CPPUNIT_TEST(testAssetStorageWithoutType);
  CPPUNIT_TEST(testStreamData);
  CPPUNIT_TEST(testSequenceNumberRollover);
  CPPUNIT_TEST(testSampleCount);
  CPPUNIT_TEST(testStreamDataObserver);
  CPPUNIT_TEST(testFailWithDuplicateDeviceUUID);
  CPPUNIT_TEST(testMultipleDisconnect);
  CPPUNIT_TEST(testRelativeTime);
  CPPUNIT_TEST(testRelativeParsedTime);
  CPPUNIT_TEST(testRelativeParsedTimeDetection);
  CPPUNIT_TEST(testRelativeOffsetDetection);
  CPPUNIT_TEST(testDynamicCalibration);
  CPPUNIT_TEST(testInitialTimeSeriesValues);
  CPPUNIT_TEST(testUUIDChange);
  CPPUNIT_TEST(testFilterValues);
  CPPUNIT_TEST(testReferences);
  CPPUNIT_TEST(testDiscrete);
  CPPUNIT_TEST(testUpcaseValues);
  CPPUNIT_TEST(testConditionSequence);
  CPPUNIT_TEST(testAssetRemoval);
  CPPUNIT_TEST(testAssetRemovalByAdapter);
  CPPUNIT_TEST(testAssetAdditionOfAssetChanged12);
  CPPUNIT_TEST(testAssetAdditionOfAssetRemoved13);
  CPPUNIT_TEST(testAssetPrependId);
  CPPUNIT_TEST(testBadAsset);
  CPPUNIT_TEST(testAssetWithSimpleCuttingItems);
  CPPUNIT_TEST(testRemoveLastAssetChanged);
  CPPUNIT_TEST(testRemoveAllAssets);
  CPPUNIT_TEST(testEmptyLastItemFromAdapter);
  CPPUNIT_TEST(testAdapterDeviceCommand);
  CPPUNIT_TEST(testBadDataItem);
  CPPUNIT_TEST(testConstantValue);
  CPPUNIT_TEST(testComposition);
  CPPUNIT_TEST(testResetTriggered);
  

  CPPUNIT_TEST_SUITE_END();
  
  typedef map<std::string, std::string>::kernel_1a_c map_type;
  typedef queue<std::string>::kernel_1a_c queue_type;
  
protected:
  Agent *a;
  Adapter *adapter;
  std::string agentId;
  std::string incomingIp;
  
  bool response;
  std::string path;
  std::string at;
  key_value_map queries;
  std::string result;
  key_value_map cookies;
  queue_type new_cookies;
  key_value_map_ci incoming_headers;
  std::string foreign_ip;
  std::string local_ip;
  unsigned short foreign_port;
  unsigned short local_port;
  std::ostringstream out;
  int delay;
  
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
  void testXPath();
  
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
  void testEmptyLastItemFromAdapter();
  
  /* Test Adapter */
  void testAdapter();
  void testUUIDChange();

  // File handler tests
  void testFileDownload();
  void testFailedFileDownload();
  
  // Adapter commands
  void testAdapterCommands();
  void testAdapterDeviceCommand();

  // Duplicate checks
  void testDuplicateCheck();
  void testDuplicateCheckAfterDisconnect();

  void testAutoAvailable();
  void testIgnoreTimestamps();
  
  void testMultipleDisconnect();

  // Test assets
  void testAssetStorage();
  void testAssetError();
  void testAssetBuffer();
  void testAdapterAddAsset();
  void testMultiLineAsset();
  void testAssetRefCounts();
  void testAssetProbe();
  void testAssetStorageWithoutType();
  
  void testAssetRemoval();
  void testAssetRemovalByAdapter();
  
  void testAssetAdditionOfAssetChanged12();
  void testAssetAdditionOfAssetRemoved13();
  
  void testAssetPrependId();
  void testBadAsset();
  void testAssetWithSimpleCuttingItems();
  
  void testRemoveLastAssetChanged();
  void testRemoveAllAssets();
  
  // Test put for data items
  void testPut();
  void testPutBlocking();
  void testPutBlockingFrom();
  
  // Streaming tests
  void testStreamDataObserver();

  static void killThread(void *aArg);
  static void addThread(void *aArg);
  static void streamThread(void *aArg);

  void testStreamData();
  
  // Sequence number tests
  void testSequenceNumberRollover();
  void testSampleCount();
  
  // Test failure when adding a duplicate device uuid.
  void testFailWithDuplicateDeviceUUID();
  
  // Relative time test
  void testRelativeTime();
  void testRelativeParsedTime();
  void testRelativeParsedTimeDetection();
  void testRelativeOffsetDetection();
  void testDynamicCalibration();
  
  // Time series tests
  void testInitialTimeSeriesValues();
  
  // Filtering
  void testFilterValues();
  
  // Reset Triggered
  void testResetTriggered();
  
  // Reference tests
  void testReferences();
  
  // Discrete
  void testDiscrete();
  void testUpcaseValues();
  
  // Conditions
  void testConditionSequence();
    
  /* Helper method to test expected string, given optional query, & run tests */
  xmlDocPtr responseHelper(CPPUNIT_NS::SourceLine sourceLine, key_value_map &aQueries);
  xmlDocPtr putResponseHelper(CPPUNIT_NS::SourceLine sourceLine, std::string body,
                              key_value_map &aQueries);
  
  // Data item name handling
  void testBadDataItem();
  void testConstantValue();
  
  // Composition testing
  void testComposition();
  
public:
  void setUp();
  void tearDown();
};

#define PARSE_XML_RESPONSE \
  xmlDocPtr doc = responseHelper(CPPUNIT_SOURCELINE(), queries); \
  CPPUNIT_ASSERT(doc)

#define PARSE_XML_RESPONSE_QUERY_KV(key, value) \
  queries[key] = value; \
  xmlDocPtr doc = responseHelper(CPPUNIT_SOURCELINE(), queries); \
  queries.clear(); \
  CPPUNIT_ASSERT(doc)

#define PARSE_XML_RESPONSE_QUERY(aQueries) \
  xmlDocPtr doc = responseHelper(CPPUNIT_SOURCELINE(), aQueries); \
  CPPUNIT_ASSERT(doc)

#define PARSE_XML_RESPONSE_PUT(body, queries)			    \
  xmlDocPtr doc = putResponseHelper(CPPUNIT_SOURCELINE(), body, queries); \
  CPPUNIT_ASSERT(doc)

