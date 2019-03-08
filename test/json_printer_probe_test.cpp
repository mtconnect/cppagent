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

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <nlohmann/json.hpp>

#include "component_event.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "json_printer.hpp"
#include "globals.hpp"
#include "checkpoint.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"
#include "test_globals.hpp"

using json = nlohmann::json;
using namespace std;

namespace mtconnect {
  namespace test {
    class JsonPrinterProbeTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(JsonPrinterProbeTest);
      CPPUNIT_TEST(testDeviceRootAndDescription);
      CPPUNIT_TEST_SUITE_END();
      
    public:
      void testDeviceRootAndDescription();
      
      void setUp();
      void tearDown();

    protected:
      std::unique_ptr<JsonPrinter> m_printer;
      std::vector<Device *> m_devices;
      
      std::unique_ptr<XmlParser> m_config;
      std::unique_ptr<XmlPrinter> m_xmlPrinter;
    };

    CPPUNIT_TEST_SUITE_REGISTRATION(JsonPrinterProbeTest);

    void JsonPrinterProbeTest::setUp()
    {
      m_config.reset(new XmlParser());
      m_xmlPrinter.reset(new XmlPrinter("1.5"));
      m_devices = m_config->parseFile("../samples/test_config.xml", m_xmlPrinter.get());
      m_printer.reset(new JsonPrinter("1.5", true));
    }

    void JsonPrinterProbeTest::tearDown()
    {
      m_config.reset();
      m_xmlPrinter.reset();
      m_printer.reset();
    }
    
    void JsonPrinterProbeTest::testDeviceRootAndDescription()
    {
      auto doc = m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices);
      cout << "\n" << doc;
      auto jdoc = json::parse(doc);
      auto it = jdoc.begin();
      CPPUNIT_ASSERT_EQUAL(string("MTConnectDevices"), it.key());
      CPPUNIT_ASSERT_EQUAL(123, jdoc.at("/MTConnectDevices/Header/@instanceId"_json_pointer).get<int32_t>());
      CPPUNIT_ASSERT_EQUAL(9999, jdoc.at("/MTConnectDevices/Header/@bufferSize"_json_pointer).get<int32_t>());
      CPPUNIT_ASSERT_EQUAL(1024, jdoc.at("/MTConnectDevices/Header/@assetBufferSize"_json_pointer).get<int32_t>());
      CPPUNIT_ASSERT_EQUAL(10, jdoc.at("/MTConnectDevices/Header/@assetCount"_json_pointer).get<int32_t>());

      auto devices = jdoc.at("/MTConnectDevices/Devices"_json_pointer).begin();
      auto first = devices->begin();
      
      CPPUNIT_ASSERT_EQUAL(string("Device"), first.key());
      
      auto device = first.value();
      CPPUNIT_ASSERT_EQUAL(string("d"), device.at("/@id"_json_pointer).get<string>());
      CPPUNIT_ASSERT_EQUAL(string("LinuxCNC"), device.at("/@name"_json_pointer).get<string>());
      CPPUNIT_ASSERT_EQUAL(string("000"), device.at("/@uuid"_json_pointer).get<string>());
      
      CPPUNIT_ASSERT_EQUAL(string("Linux CNC Device"), device.at("/Description/#text"_json_pointer).get<string>());
      CPPUNIT_ASSERT_EQUAL(string("NIST"), device.at("/Description/@manufacturer"_json_pointer).get<string>());
      CPPUNIT_ASSERT_EQUAL(string("1122"), device.at("/Description/@serialNumber"_json_pointer).get<string>());
    }
  }
}

      
