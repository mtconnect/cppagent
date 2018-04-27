/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#ifndef XML_PRINTER_TEST_HPP
#define XML_PRINTER_TEST_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "component_event.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"
#include "globals.hpp"
#include "checkpoint.hpp"

#include "test_globals.hpp"

class XmlPrinterTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(XmlPrinterTest);
  CPPUNIT_TEST(testPrintError);
  CPPUNIT_TEST(testPrintProbe);
  CPPUNIT_TEST(testPrintDataItemElements);
  CPPUNIT_TEST(testPrintCurrent);
  CPPUNIT_TEST(testPrintSample);
  CPPUNIT_TEST(testCondition);
  CPPUNIT_TEST(testVeryLargeSequence);
  CPPUNIT_TEST(testChangeDeviceAttributes);
  CPPUNIT_TEST(testChangeDevicesNamespace);
  CPPUNIT_TEST(testChangeStreamsNamespace);
  CPPUNIT_TEST(testChangeErrorNamespace);
  CPPUNIT_TEST(testStatisticAndTimeSeriesProbe);
  CPPUNIT_TEST(testTimeSeries);
  CPPUNIT_TEST(testNonPrintableCharacters);
  CPPUNIT_TEST(testPrintAsset);
  CPPUNIT_TEST(testPrintAssetProbe);
  CPPUNIT_TEST(testConfiguration);
  CPPUNIT_TEST(testPrintCuttingTool);
  CPPUNIT_TEST(testChangeVersion);
  CPPUNIT_TEST(testChangeMTCLocation);
  CPPUNIT_TEST(testProbeWithFilter13);
  CPPUNIT_TEST(testProbeWithFilter);
  CPPUNIT_TEST(testReferences);
  CPPUNIT_TEST(testPrintExtendedCuttingTool);
  CPPUNIT_TEST(testStreamsStyle);
  CPPUNIT_TEST(testDevicesStyle);
  CPPUNIT_TEST(testErrorStyle);
  CPPUNIT_TEST(testAssetsStyle);
  CPPUNIT_TEST(testPrintRemovedCuttingTool);
  CPPUNIT_TEST(testEscapedXMLCharacters);
  CPPUNIT_TEST_SUITE_END();
  
protected:
  XmlParser *config;
  std::vector<Device *> devices;
  
protected:
  /* Helper methods to test */
  
  /* Main methods to test */
  void testPrintError();
  void testPrintProbe();
  void testPrintDataItemElements();
  void testPrintCurrent();
  void testPrintSample();
  void testChangeDevicesNamespace();
  void testChangeErrorNamespace();
  void testChangeStreamsNamespace();
  void testStatisticAndTimeSeriesProbe();
  void testTimeSeries();
  
  // Character generation
  void testEscapedXMLCharacters();
  
  // Test printing configuration...
  void testConfiguration();

  // Test new condition handling
  void testCondition();
  
  // Test overflow
  void testVeryLargeSequence();
  
  void testChangeDeviceAttributes();
  
  void testNonPrintableCharacters();

  // Asset tests
  void testPrintAsset();
  void testPrintAssetProbe();
  void testPrintCuttingTool();
  void testPrintExtendedCuttingTool();
  void testPrintRemovedCuttingTool();
  
  // Schema tests
  void testChangeVersion();
  void testChangeMTCLocation();
  
  // Filter Tests
  void testProbeWithFilter13();
  void testProbeWithFilter();
  
  // Reference tests
  void testReferences();
  
  // Styles
  void testStreamsStyle();
  void testDevicesStyle();
  void testErrorStyle();
  void testAssetsStyle();

  
  /* Retrieve a data item by name string */
  DataItem * getDataItem(const char *name);
  
  /* Construct a component event and set it as the data item's latest event */
  ComponentEvent * addEventToCheckpoint(
    Checkpoint &aCheckpoint,
    const char *name,
    uint64_t sequence,
    std::string value
  );
  
  ComponentEvent * newEvent(
    const char *name,
    uint64_t sequence,
    std::string value
  );
  
public:
  void setUp();
  void tearDown();
};

#endif

