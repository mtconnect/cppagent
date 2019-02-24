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

#include <map>
#include <string>
#include <memory>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "component_event.hpp"
#include "data_item.hpp"

namespace mtconnect {
  namespace test {
    class ComponentEventTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(ComponentEventTest);
      CPPUNIT_TEST(testConstructors);
      CPPUNIT_TEST(testGetAttributes);
      CPPUNIT_TEST(testConvertValue);
      CPPUNIT_TEST(testConvertSimpleUnits);
      CPPUNIT_TEST(testRefCounts);
      CPPUNIT_TEST(testStlLists);
      CPPUNIT_TEST(testEventChaining);
      CPPUNIT_TEST(testCondition);
      CPPUNIT_TEST(testTimeSeries);
      CPPUNIT_TEST(testDuration);
      CPPUNIT_TEST(testAssetChanged);
      CPPUNIT_TEST_SUITE_END();
      
    public:
      void setUp();
      void tearDown();
      
    protected:
      ComponentEvent *m_compEventA;
      ComponentEvent *m_compEventB;
      std::unique_ptr<DataItem> m_dataItem1;
      std::unique_ptr<DataItem> m_dataItem2;
      
    protected:
      void testConstructors();
      void testGetAttributes();
      void testGetters();
      void testConvertValue();
      void testConvertSimpleUnits();
      void testRefCounts();
      void testStlLists();
      void testEventChaining();
      void testCondition();
      void testTimeSeries();
      void testDuration();
      void testAssetChanged();
      
      // Helper to test values
      void testValueHelper(std::map<std::string, std::string> &attributes,
                           const std::string &nativeUnits,
                           float expected,
                           const std::string &value,
                           CPPUNIT_NS::SourceLine sourceLine
                           );
    };
  }
}
