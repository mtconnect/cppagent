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
    class JsonPrinterStreamTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(JsonPrinterStreamTest);
      CPPUNIT_TEST(testStreamHeader);
      CPPUNIT_TEST(testDeviceStream);
      CPPUNIT_TEST(testComponentStream);
      CPPUNIT_TEST_SUITE_END();
      
    public:
      void testStreamHeader();
      void testDeviceStream();
      void testComponentStream();
      
    public:
      void setUp();
      void tearDown();
      
      DataItem *getDataItem(const char *name) {
        const auto device = m_devices.front();
        
        CPPUNIT_ASSERT(device);
        return device->getDeviceDataItem(name);
      }

      ComponentEvent *newEvent(const char *name,uint64_t sequence,string value)
      {
        string time("TIME");
        // Make sure the data item is there
        const auto d = getDataItem(name);
        CPPUNIT_ASSERT_MESSAGE((string) "Could not find data item " + name, d);
        
        return new ComponentEvent(*d, sequence, time, value);
      }
      ComponentEvent *addEventToCheckpoint(Checkpoint &checkpoint, const char *name, uint64_t sequence,
                                           string value)
      {
        ComponentEvent *event = newEvent(name, sequence, value);
        checkpoint.addComponentEvent(event);
        return event;
      }
      
    protected:
      std::unique_ptr<JsonPrinter> m_printer;      
      std::unique_ptr<XmlParser> m_config;
      std::unique_ptr<XmlPrinter> m_xmlPrinter;
      std::vector<Device *> m_devices;
    };
    
    CPPUNIT_TEST_SUITE_REGISTRATION(JsonPrinterStreamTest);

    void JsonPrinterStreamTest::setUp()
    {
      m_xmlPrinter.reset(new XmlPrinter("1.5"));
      m_printer.reset(new JsonPrinter("1.5", true));      
      m_config.reset(new XmlParser());
      m_devices = m_config->parseFile("../samples/SimpleDevlce.xml", m_xmlPrinter.get());
    }
    
    void JsonPrinterStreamTest::tearDown()
    {
      m_config.reset();
      m_xmlPrinter.reset();
      m_printer.reset();
    }
    
    void JsonPrinterStreamTest::testStreamHeader()
    {
      Checkpoint checkpoint;
      ComponentEventPtrArray list;
      checkpoint.getComponentEvents(list);
      auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
      auto jdoc = json::parse(doc);
      auto it = jdoc.begin();
      CPPUNIT_ASSERT_EQUAL(string("MTConnectStreams"), it.key());
      CPPUNIT_ASSERT_EQUAL(123, jdoc.at("/MTConnectStreams/Header/@instanceId"_json_pointer).get<int32_t>());
      CPPUNIT_ASSERT_EQUAL(131072, jdoc.at("/MTConnectStreams/Header/@bufferSize"_json_pointer).get<int32_t>());
      CPPUNIT_ASSERT_EQUAL(10254805ull, jdoc.at("/MTConnectStreams/Header/@nextSequence"_json_pointer).get<uint64_t>());
      CPPUNIT_ASSERT_EQUAL(10123733ull, jdoc.at("/MTConnectStreams/Header/@firstSequence"_json_pointer).get<uint64_t>());
      CPPUNIT_ASSERT_EQUAL(10123800ull, jdoc.at("/MTConnectStreams/Header/@lastSequence"_json_pointer).get<uint64_t>());
    }
    
    void JsonPrinterStreamTest::testDeviceStream()
    {
      Checkpoint checkpoint;
      addEventToCheckpoint(checkpoint, "Xpos", 10254804, "100");
      ComponentEventPtrArray list;
      checkpoint.getComponentEvents(list);
      auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
      //cout << "\n" << doc << endl;

      auto jdoc = json::parse(doc);
      json stream = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream"_json_pointer);
      CPPUNIT_ASSERT(stream.is_object());
      
      CPPUNIT_ASSERT_EQUAL(string("SimpleCnc"), stream.at("/@name"_json_pointer).get<string>());
      CPPUNIT_ASSERT_EQUAL(string("872a3490-bd2d-0136-3eb0-0c85909298d9"), stream.at("/@uuid"_json_pointer).get<string>());
    }
    
    void JsonPrinterStreamTest::testComponentStream()
    {
      Checkpoint checkpoint;
      addEventToCheckpoint(checkpoint, "Xpos", 10254804, "100");
      ComponentEventPtrArray list;
      checkpoint.getComponentEvents(list);
      auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
      //cout << "\n" << doc << endl;
      
      auto jdoc = json::parse(doc);
      json stream = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams/0/ComponentStream"_json_pointer);
      CPPUNIT_ASSERT(stream.is_object());      
    }

  }
}
