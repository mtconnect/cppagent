//
// Copyright Copyright 2012, System Insights, Inc.
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

#include <map>
#include <string>
#include <iosfwd>
#include <chrono>

#include "dlib/md5.h"
#include "dlib/server.h"
#include "dlib/sliding_buffer.h"


#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "adapter.hpp"
#include "agent.hpp"
#include "test_globals.hpp"
#include "agent_test_helper.hpp"

class DataSetTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(DataSetTest);

  CPPUNIT_TEST(testDataItem);
  CPPUNIT_TEST(testInitialSet);
  CPPUNIT_TEST(testUpdateOneElement);
  CPPUNIT_TEST(testUpdateMany);
  CPPUNIT_TEST(testReset);
  CPPUNIT_TEST(testBadData);
  CPPUNIT_TEST(testCurrent);
  CPPUNIT_TEST(testSample);
  CPPUNIT_TEST(testCurrentAt);
  CPPUNIT_TEST(testDeleteKey);
  CPPUNIT_TEST(testResetWithJustNoData);

  CPPUNIT_TEST_SUITE_END();
    
protected:
  Checkpoint *m_checkpoint;
  Agent *m_agent;
  Adapter *m_adapter;
  std::string m_agentId;
  DataItem *m_dataItem1;
  
  AgentTestHelper m_agentTestHelper;
  
protected:
  void testDataItem();
  void testInitialSet();
  void testUpdateOneElement();
  void testUpdateMany();
  void testReset();
  void testBadData();
  void testCurrent();
  void testSample();
  void testCurrentAt();
  void testDeleteKey();
  void testResetWithJustNoData();

public:
  void setUp();
  void tearDown();
};
