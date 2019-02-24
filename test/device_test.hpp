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

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "device.hpp"

namespace mtconnect {
  namespace test {
    class DeviceTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(DeviceTest);
      CPPUNIT_TEST(testGetters);
      CPPUNIT_TEST(testGetAttributes);
      CPPUNIT_TEST(testDescription);
      CPPUNIT_TEST(testRelationships);
      CPPUNIT_TEST(testDataItems);
      CPPUNIT_TEST(testDeviceDataItem);
      CPPUNIT_TEST(testGetDataItem);
      CPPUNIT_TEST_SUITE_END();
      
    protected:
      Device *m_devA, *m_devB;
      
    protected:
      void testGetters();
      void testGetAttributes();
      void testDescription();
      void testRelationships();
      void testDataItems();
      void testDeviceDataItem();
      void testGetDataItem();
      
    public:
      void setUp();
      void tearDown();
    };
  }
}

