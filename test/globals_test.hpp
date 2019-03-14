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

#include "globals.hpp"

namespace mtconnect {
  namespace test {
    class GlobalsTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(GlobalsTest);
      CPPUNIT_TEST(testIntToString);
      CPPUNIT_TEST(testFloatToString);
      CPPUNIT_TEST(testToUpperCase);
      CPPUNIT_TEST(testIsNonNegativeInteger);
      CPPUNIT_TEST(testTime);
      CPPUNIT_TEST(testIllegalCharacters);
      CPPUNIT_TEST(testGetCurrentTime);
      CPPUNIT_TEST(testGetCurrentTime2);
      CPPUNIT_TEST(testParseTimeMicro);
      CPPUNIT_TEST(testAddNamespace);
      CPPUNIT_TEST(testParseTimeMilli);
      CPPUNIT_TEST(testInt64ToString);
      CPPUNIT_TEST_SUITE_END();
      
    protected:
      void testIntToString();
      void testFloatToString();
      void testToUpperCase();
      void testIsNonNegativeInteger();
      void testTime();
      void testIllegalCharacters();
      void testGetCurrentTime();
      void testGetCurrentTime2();
      void testParseTimeMicro();
      void testAddNamespace();
      void testParseTimeMilli();
      void testInt64ToString();
      
    public:
      void setUp();
      void tearDown();
    };
  }
}
