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

#include <map>
#include <string>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "component_event.hpp"
#include "data_item.hpp"

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
  DataItem * m_dataItemA, * m_dataItemB, * m_dataItemC;
  
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
