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

#include <string>
#include <memory>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "checkpoint.hpp"
#include "adapter.hpp"
#include "agent.hpp"
#include "test_globals.hpp"

namespace mtconnect {
  namespace test {
    class CheckpointTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(CheckpointTest);
      CPPUNIT_TEST(testAddComponentEvents);
      CPPUNIT_TEST(testCopy);
      CPPUNIT_TEST(testGetComponentEvents);
      CPPUNIT_TEST(testFilter);
      CPPUNIT_TEST(testCopyAndFilter);
      CPPUNIT_TEST(testConditionChaining);
      CPPUNIT_TEST(testLastConditionNormal);
      CPPUNIT_TEST_SUITE_END();
      
    protected:
      std::unique_ptr<Checkpoint> m_checkpoint;
      std::unique_ptr<Agent> m_agent;
      std::string m_agentId;
      std::unique_ptr<DataItem> m_dataItem1;
      std::unique_ptr<DataItem> m_dataItem2;
      
    protected:
      void testAddComponentEvents();
      void testCopy();
      void testGetComponentEvents();
      void testFilter();
      void testCopyAndFilter();
      void testConditionChaining();
      void testLastConditionNormal();
      
    public:
      void setUp();
      void tearDown();
    };
  }
}
