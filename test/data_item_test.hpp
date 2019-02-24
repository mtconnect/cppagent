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

#include <memory>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "data_item.hpp"

namespace mtconnect {
  namespace test {
    class DataItemTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(DataItemTest);
      CPPUNIT_TEST(testGetters);
      CPPUNIT_TEST(testGetAttributes);
      CPPUNIT_TEST(testHasNameAndSource);
      CPPUNIT_TEST(testIsSample);
      CPPUNIT_TEST(testComponent);
      CPPUNIT_TEST(testGetCamel);
      CPPUNIT_TEST(testConversion);
      CPPUNIT_TEST(testCondition);
      CPPUNIT_TEST(testTimeSeries);
      CPPUNIT_TEST(testStatistic);
      CPPUNIT_TEST(testSampleRate);
      CPPUNIT_TEST(testDuplicates);
      CPPUNIT_TEST(testFilter);
      CPPUNIT_TEST_SUITE_END();
      
    protected:
      std::unique_ptr<DataItem> m_dataItemA;
      std::unique_ptr<DataItem> m_dataItemB;
      std::unique_ptr<DataItem> m_dataItemC;
      
    protected:
      void testGetters();
      void testGetAttributes();
      void testHasNameAndSource();
      void testIsSample();
      void testComponent();
      void testGetCamel();
      void testConversion();
      void testCondition();
      void testTimeSeries();
      void testStatistic();
      void testSampleRate();
      void testDuplicates();
      void testFilter();
      void testReferences();
      
    public:
      void setUp();
      void tearDown();
    };
  }
}
