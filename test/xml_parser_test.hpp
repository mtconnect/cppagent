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

#include <vector>
#include "xml_parser.hpp"
class Device;


class XmlParserTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(XmlParserTest);
  CPPUNIT_TEST(testConstructor);
  CPPUNIT_TEST(testGetDevices);
  CPPUNIT_TEST(testCondition);
  CPPUNIT_TEST(testGetDataItems);
  CPPUNIT_TEST(testExtendedSchema);
  CPPUNIT_TEST(testTimeSeries);
  CPPUNIT_TEST(testGetDataItemsExt);
  CPPUNIT_TEST(testConfiguration);
  CPPUNIT_TEST(testParseAsset);
  CPPUNIT_TEST(testUpdateAsset);
  CPPUNIT_TEST(testNoNamespace);
  CPPUNIT_TEST(testFilteredDataItem);
  CPPUNIT_TEST(testReferences);
  CPPUNIT_TEST(testExtendedAsset);
  CPPUNIT_TEST(testExtendedAssetFragment);
  CPPUNIT_TEST(testParseOtherAsset);
  CPPUNIT_TEST(testParseRemovedAsset);
  CPPUNIT_TEST(testBadAsset);
  CPPUNIT_TEST_SUITE_END();
  
protected:
  XmlParser * m_xmlParser;
  std::vector<Device *> m_devices;
  
protected:
  void testConstructor();
  void testGetDevices();
  void testCondition();
  void testGetDataItems();
  void testExtendedSchema();
  void testTimeSeries();
  void testGetDataItemsExt();
  void testConfiguration();
  void testParseAsset();
  void testUpdateAsset();
  void testParseOtherAsset();
  void testParseRemovedAsset();
  void testNoNamespace();
  void testFilteredDataItem();
  void testReferences();
  void testExtendedAsset();
  void testExtendedAssetFragment();
  void testBadAsset();
  
public:
  void setUp();
  void tearDown();
};
