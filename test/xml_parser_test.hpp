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

#include <vector>
#include "xml_parser.hpp"

namespace mtconnect {
  class Device;
  namespace test {
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
      CPPUNIT_TEST(testFilteredDataItem13);
      CPPUNIT_TEST(testFilteredDataItem);
      CPPUNIT_TEST(testReferences);
      CPPUNIT_TEST(testSourceReferences);
      CPPUNIT_TEST(testExtendedAsset);
      CPPUNIT_TEST(testExtendedAssetFragment);
      CPPUNIT_TEST(testParseOtherAsset);
      CPPUNIT_TEST(testParseRemovedAsset);
      CPPUNIT_TEST(testBadAsset);
      CPPUNIT_TEST_SUITE_END();
      
    protected:
      XmlParser *m_xmlParser;
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
      void testFilteredDataItem13();
      void testFilteredDataItem();
      void testReferences();
      void testSourceReferences();
      void testExtendedAsset();
      void testExtendedAssetFragment();
      void testBadAsset();
      
    public:
      void setUp();
      void tearDown();
    };
  }
}
