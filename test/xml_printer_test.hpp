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

namespace mtconnect {
  namespace test {
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
      CPPUNIT_TEST(testSourceReferences);
      CPPUNIT_TEST(testLegacyReferences);
      CPPUNIT_TEST(testPrintExtendedCuttingTool);
      CPPUNIT_TEST(testStreamsStyle);
      CPPUNIT_TEST(testDevicesStyle);
      CPPUNIT_TEST(testErrorStyle);
      CPPUNIT_TEST(testAssetsStyle);
      CPPUNIT_TEST(testPrintRemovedCuttingTool);
      CPPUNIT_TEST(testEscapedXMLCharacters);
      CPPUNIT_TEST_SUITE_END();
      
    protected:
      mtconnect::XmlParser *m_config;
      std::vector<mtconnect::Device *> m_devices;
      
    protected:
      // Helper methods to test
      
      // Main methods to test
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
      void testSourceReferences();
      void testLegacyReferences();
      
      // Styles
      void testStreamsStyle();
      void testDevicesStyle();
      void testErrorStyle();
      void testAssetsStyle();
      
      
      // Retrieve a data item by name string
      mtconnect::DataItem *getDataItem(const char *name);
      
      // Construct a component event and set it as the data item's latest event
      mtconnect::ComponentEvent *addEventToCheckpoint(
                                                      mtconnect::Checkpoint &checkpoint,
                                                      const char *name,
                                                      uint64_t sequence,
                                                      std::string value
                                                      );
      
      mtconnect::ComponentEvent *newEvent(
                                          const char *name,
                                          uint64_t sequence,
                                          std::string value
                                          );
      
    public:
      void setUp();
      void tearDown();
    };
  }
}
