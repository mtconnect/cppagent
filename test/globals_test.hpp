//
// Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the AMT nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
// BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
// AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
// RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
// (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
// WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
// LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
//
// LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
// PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
// OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
// CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
// WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
// THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
// SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
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
      CPPUNIT_TEST(testGetEnumerations);
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
      void testGetEnumerations();
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
