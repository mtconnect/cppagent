//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#pragma once

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <map>
#include <string>
#include <iosfwd>
#include <chrono>
#include <memory>

#include "dlib/md5.h"
#include "dlib/server.h"
#include "dlib/sliding_buffer.h"


#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "adapter.hpp"
#include "agent.hpp"
#include "test_globals.hpp"
#include "agent_test_helper.hpp"

namespace mtconnect {
  namespace test {
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
      CPPUNIT_TEST(testFilterValues13);
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
      
      typedef dlib::map<std::string, std::string>::kernel_1a_c map_type;
      typedef dlib::queue<std::string>::kernel_1a_c queue_type;
      
    protected:
      std::unique_ptr<Agent> m_agent;
      Adapter *m_adapter;
      std::string m_agentId;
      AgentTestHelper m_agentTestHelper;
      
      std::chrono::milliseconds m_delay;
      
    protected:
      // Test Basic
      void testConstructor();
      
      // Test Errors
      void testBadPath();
      void testBadXPath();
      void testBadCount();
      void testBadFreq();
      
      // test good
      void testGoodPath();
      void testXPath();
      
      // Test calls
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
      
      // Test Adapter
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
      void testFilterValues13();
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
      
      // Data item name handling
      void testBadDataItem();
      void testConstantValue();
      
      // Composition testing
      void testComposition();
      
    public:
      void setUp();
      void tearDown();
    };
  }
}
