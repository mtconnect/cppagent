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

#include "Cuti.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

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
#include "json_helper.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

TEST_CLASS(JsonPrinterStreamTest)
{
public:
  void testStreamHeader();
  void testDeviceStream();
  void testComponentStream();
  void testComponentStreamTwoComponents();
  void testTwoDevices();
  void testSampleAndEventDataItem();
  void testConditionDataItem();
  void testTimeSeries();
  void testAssetChanged();
  void testResetTrigger();
  void testMessage();
  void testUnavailability();

  SET_UP();
  TEAR_DOWN();
  
  DataItem *getDataItem(const char *name) {
    for (auto &device : m_devices) {
      auto di = device->getDeviceDataItem(name);
      if (di) return di;
    }
    return nullptr;
  }
  
  ComponentEvent *newObservaton(const char *name,uint64_t sequence,string value, string time = "TIME")
  {
    // Make sure the data item is there
    const auto d = getDataItem(name);
    CPPUNIT_ASSERT_MESSAGE((string) "Could not find data item " + name, d);
    
    return new ComponentEvent(*d, sequence, time, value);
  }
  
  ComponentEvent *addObservationToCheckpoint(Checkpoint &checkpoint,
                                       const char *name, uint64_t sequence,
                                       string value, string time = "TIME")
  {
    auto event = newObservaton(name, sequence, value, time);
    checkpoint.addComponentEvent(event);
    return event;
  }
  
  
protected:
  std::unique_ptr<JsonPrinter> m_printer;
  std::unique_ptr<XmlParser> m_config;
  std::unique_ptr<XmlPrinter> m_xmlPrinter;
  std::vector<Device *> m_devices;
  
  CPPUNIT_TEST_SUITE(JsonPrinterStreamTest);
  CPPUNIT_TEST(testStreamHeader);
  CPPUNIT_TEST(testDeviceStream);
  CPPUNIT_TEST(testComponentStream);
  CPPUNIT_TEST(testComponentStreamTwoComponents);
  CPPUNIT_TEST(testTwoDevices);
  CPPUNIT_TEST(testSampleAndEventDataItem);
  CPPUNIT_TEST(testConditionDataItem);
  CPPUNIT_TEST(testTimeSeries);
  CPPUNIT_TEST(testAssetChanged);
  CPPUNIT_TEST(testResetTrigger);
  CPPUNIT_TEST(testMessage);
  CPPUNIT_TEST(testUnavailability);
  CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(JsonPrinterStreamTest);

void JsonPrinterStreamTest::setUp()
{
  m_xmlPrinter.reset(new XmlPrinter("1.5"));
  m_printer.reset(new JsonPrinter("1.5", true));
  m_config.reset(new XmlParser());
  m_devices = m_config->parseFile(PROJECT_ROOT_DIR "/samples/SimpleDevlce.xml", m_xmlPrinter.get());
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
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254805), jdoc.at("/MTConnectStreams/Header/@nextSequence"_json_pointer).get<uint64_t>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10123733), jdoc.at("/MTConnectStreams/Header/@firstSequence"_json_pointer).get<uint64_t>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10123800), jdoc.at("/MTConnectStreams/Header/@lastSequence"_json_pointer).get<uint64_t>());
}

void JsonPrinterStreamTest::testDeviceStream()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, "100");
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
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, "100");
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  json stream = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("Linear"), stream.at("/@component"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("X1"), stream.at("/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("e373fec0"), stream.at("/@componentId"_json_pointer).get<string>());
}

void JsonPrinterStreamTest::testComponentStreamTwoComponents()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, "100");
  addObservationToCheckpoint(checkpoint, "Sspeed_act", 10254805, "500");
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(2_S, streams.size());
  
  json stream1 = streams.at("/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream1.is_object());
  CPPUNIT_ASSERT_EQUAL(string("Linear"), stream1.at("/@component"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("e373fec0"), stream1.at("/@componentId"_json_pointer).get<string>());
  
  json stream2 = streams.at("/1/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream2.is_object());
  CPPUNIT_ASSERT_EQUAL(string("Rotary"), stream2.at("/@component"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("zf476090"), stream2.at("/@componentId"_json_pointer).get<string>());
}

void JsonPrinterStreamTest::testTwoDevices()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, "100");
  addObservationToCheckpoint(checkpoint, "z2143c50", 10254805, "AVAILABLE");
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(2_S, streams.size());
  
  json stream1 = streams.at("/1/DeviceStream"_json_pointer);
  CPPUNIT_ASSERT(stream1.is_object());
  CPPUNIT_ASSERT_EQUAL(string("SimpleCnc"), stream1.at("/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("872a3490-bd2d-0136-3eb0-0c85909298d9"), stream1.at("/@uuid"_json_pointer).get<string>());
  
  json stream2 = streams.at("/0/DeviceStream"_json_pointer);
  CPPUNIT_ASSERT(stream2.is_object());
  CPPUNIT_ASSERT_EQUAL(string("SampleDevice2"), stream2.at("/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("f2db97b0-2bd1-0137-91ba-2a0081597801"), stream2.at("/@uuid"_json_pointer).get<string>());
}

void JsonPrinterStreamTest::testSampleAndEventDataItem()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "if36ff60", 10254804, "AUTOMATIC"); // Controller Mode
  addObservationToCheckpoint(checkpoint, "r186cd60", 10254805, "10 20 30"); // Path Position
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(1_S, streams.size());
  
  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("a4a7bdf0"), stream.at("/@componentId"_json_pointer).get<string>());
  
  auto events = stream.at("/Events"_json_pointer);
  CPPUNIT_ASSERT(events.is_array());
  auto mode = events.at(0);
  CPPUNIT_ASSERT(mode.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("AUTOMATIC"), mode.at("/ControllerMode/Value"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("if36ff60"), mode.at("/ControllerMode/@dataItemId"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("mode"), mode.at("/ControllerMode/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TIME"), mode.at("/ControllerMode/@timestamp"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254804), mode.at("/ControllerMode/@sequence"_json_pointer).get<uint64_t>());
  
  auto samples = stream.at("/Samples"_json_pointer);
  CPPUNIT_ASSERT(samples.is_array());
  auto pos = samples.at(0);
  
  CPPUNIT_ASSERT_EQUAL(3_S, pos.at("/PathPosition/Value"_json_pointer).size());
  
  CPPUNIT_ASSERT_EQUAL(10.0, pos.at("/PathPosition/Value/0"_json_pointer).get<double>());
  CPPUNIT_ASSERT_EQUAL(20.0, pos.at("/PathPosition/Value/1"_json_pointer).get<double>());
  CPPUNIT_ASSERT_EQUAL(30.0, pos.at("/PathPosition/Value/2"_json_pointer).get<double>());
  CPPUNIT_ASSERT_EQUAL(string("r186cd60"), pos.at("/PathPosition/@dataItemId"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TIME"), pos.at("/PathPosition/@timestamp"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254805), pos.at("/PathPosition/@sequence"_json_pointer).get<uint64_t>());
}

void JsonPrinterStreamTest::testConditionDataItem()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "a5b23650", 10254804, "fault|syn|ack|HIGH|Syntax error"); // Motion Program Condition
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(1_S, streams.size());
  
  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());

  CPPUNIT_ASSERT_EQUAL(string("a4a7bdf0"), stream.at("/@componentId"_json_pointer).get<string>());
  
  auto conds = stream.at("/Condition"_json_pointer);
  CPPUNIT_ASSERT(conds.is_array());
  CPPUNIT_ASSERT_EQUAL(1_S, conds.size());
  auto motion = conds.at(0);
  CPPUNIT_ASSERT(motion.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("a5b23650"), motion.at("/Fault/@dataItemId"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("motion"), motion.at("/Fault/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TIME"), motion.at("/Fault/@timestamp"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254804), motion.at("/Fault/@sequence"_json_pointer).get<uint64_t>());
  CPPUNIT_ASSERT_EQUAL(string("HIGH"), motion.at("/Fault/@qualifier"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("ack"), motion.at("/Fault/@nativeSeverity"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("syn"), motion.at("/Fault/@nativeCode"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("Syntax error"), motion.at("/Fault/Value"_json_pointer).get<string>());
}

void JsonPrinterStreamTest::testTimeSeries()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "tc9edc70", 10254804, "10|100|1.0 2.0 3 4 5.0 6 7 8.8 9.0 10.2"); // Volt Ampere Time Series
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(1_S, streams.size());
  
  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("afb91ba0"), stream.at("/@componentId"_json_pointer).get<string>());
  
  auto samples = stream.at("/Samples"_json_pointer);
  CPPUNIT_ASSERT(samples.is_array());
  CPPUNIT_ASSERT_EQUAL(1_S, samples.size());
  auto amps = samples.at(0);
  CPPUNIT_ASSERT(amps.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("tc9edc70"), amps.at("/VoltAmpereTimeSeries/@dataItemId"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("pampts"), amps.at("/VoltAmpereTimeSeries/@name"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TIME"), amps.at("/VoltAmpereTimeSeries/@timestamp"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254804), amps.at("/VoltAmpereTimeSeries/@sequence"_json_pointer).get<uint64_t>());
  CPPUNIT_ASSERT_EQUAL(10.0, amps.at("/VoltAmpereTimeSeries/@sampleCount"_json_pointer).get<double>());
  CPPUNIT_ASSERT_EQUAL(100.0, amps.at("/VoltAmpereTimeSeries/@sampleRate"_json_pointer).get<double>());
  
  auto value = amps.at("/VoltAmpereTimeSeries/Value"_json_pointer);
  CPPUNIT_ASSERT(value.is_array());
  CPPUNIT_ASSERT_EQUAL(10_S, value.size());
  
  CPPUNIT_ASSERT_EQUAL(1.0, value[0].get<double>());
  CPPUNIT_ASSERT_EQUAL(2.0, value[1].get<double>());
  CPPUNIT_ASSERT_EQUAL(3.0, value[2].get<double>());
  CPPUNIT_ASSERT_EQUAL(4.0, value[3].get<double>());
  CPPUNIT_ASSERT_EQUAL(5.0, value[4].get<double>());
  CPPUNIT_ASSERT_EQUAL(6.0, value[5].get<double>());
  CPPUNIT_ASSERT_EQUAL(7.0, value[6].get<double>());
  CPPUNIT_ASSERT_DOUBLES_EQUAL(8.8, value[7].get<double>(), 0.0001);
  CPPUNIT_ASSERT_EQUAL(9.0, value[8].get<double>());
  CPPUNIT_ASSERT_DOUBLES_EQUAL(10.2, value[9].get<double>(), 0.0001);
}

void JsonPrinterStreamTest::testAssetChanged()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "e4a300e0", 10254804, "CuttingTool|31d416a0-33c7"); // asset changed
  addObservationToCheckpoint(checkpoint, "f2df7550", 10254805, "QIF|400477d0-33c7"); // asset removed
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(1_S, streams.size());
  
  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("x872a3490"), stream.at("/@componentId"_json_pointer).get<string>());
  
  auto events = stream.at("/Events"_json_pointer);
  CPPUNIT_ASSERT(events.is_array());
  CPPUNIT_ASSERT_EQUAL(2_S, events.size());

  auto changed = events.at(0);
  CPPUNIT_ASSERT(changed.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("e4a300e0"), changed.at("/AssetChanged/@dataItemId"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TIME"), changed.at("/AssetChanged/@timestamp"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254804), changed.at("/AssetChanged/@sequence"_json_pointer).get<uint64_t>());
  CPPUNIT_ASSERT_EQUAL(string("CuttingTool"), changed.at("/AssetChanged/@assetType"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("31d416a0-33c7"), changed.at("/AssetChanged/Value"_json_pointer).get<string>());

  auto removed = events.at(1);
  CPPUNIT_ASSERT(removed.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("f2df7550"), removed.at("/AssetRemoved/@dataItemId"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TIME"), removed.at("/AssetRemoved/@timestamp"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254805), removed.at("/AssetRemoved/@sequence"_json_pointer).get<uint64_t>());
  CPPUNIT_ASSERT_EQUAL(string("QIF"), removed.at("/AssetRemoved/@assetType"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("400477d0-33c7"), removed.at("/AssetRemoved/Value"_json_pointer).get<string>());

}

void JsonPrinterStreamTest::testResetTrigger()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "qb9212c0", 10254804, "10.0:ACTION_COMPLETE", "TIME@100.0"); // Amperage
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(1_S, streams.size());
  
  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("afb91ba0"), stream.at("/@componentId"_json_pointer).get<string>());
  
  auto samples = stream.at("/Samples"_json_pointer);
  CPPUNIT_ASSERT(samples.is_array());
  CPPUNIT_ASSERT_EQUAL(1_S, samples.size());
  auto amp = samples.at(0);
  CPPUNIT_ASSERT(amp.is_object());
  cout << amp.dump(2) << endl;
  
  CPPUNIT_ASSERT_EQUAL(string("qb9212c0"), amp.at("/Amperage/@dataItemId"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TIME"), amp.at("/Amperage/@timestamp"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254804), amp.at("/Amperage/@sequence"_json_pointer).get<uint64_t>());
  CPPUNIT_ASSERT_EQUAL(string("ACTION_COMPLETE"), amp.at("/Amperage/@resetTriggered"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("AVERAGE"), amp.at("/Amperage/@statistic"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(100.0, amp.at("/Amperage/@duration"_json_pointer).get<double>());
  CPPUNIT_ASSERT_EQUAL(10.0, amp.at("/Amperage/Value"_json_pointer).get<double>());

}

void JsonPrinterStreamTest::testMessage()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "m17f1750", 10254804, "XXXX|XXX is on the roof"); // asset changed
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(1_S, streams.size());
  
  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("p5add360"), stream.at("/@componentId"_json_pointer).get<string>());
  
  auto events = stream.at("/Events"_json_pointer);
  CPPUNIT_ASSERT(events.is_array());
  CPPUNIT_ASSERT_EQUAL(1_S, events.size());
  
  auto message = events.at(0);
  CPPUNIT_ASSERT(message.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("m17f1750"), message.at("/Message/@dataItemId"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("TIME"), message.at("/Message/@timestamp"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(uint64_t(10254804), message.at("/Message/@sequence"_json_pointer).get<uint64_t>());
  CPPUNIT_ASSERT_EQUAL(string("XXXX"), message.at("/Message/@nativeCode"_json_pointer).get<string>());
  CPPUNIT_ASSERT_EQUAL(string("XXX is on the roof"), message.at("/Message/Value"_json_pointer).get<string>());

}

void JsonPrinterStreamTest::testUnavailability()
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "m17f1750", 10254804, "|UNAVAILABLE"); // asset changed
  addObservationToCheckpoint(checkpoint, "a5b23650", 10254804, "unavailable||||"); // Motion Program Condition
  ComponentEventPtrArray list;
  checkpoint.getComponentEvents(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  
  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  CPPUNIT_ASSERT_EQUAL(2_S, streams.size());
  
  auto stream = streams.at("/1/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("p5add360"), stream.at("/@componentId"_json_pointer).get<string>());
  
  auto events = stream.at("/Events"_json_pointer);
  CPPUNIT_ASSERT(events.is_array());
  CPPUNIT_ASSERT_EQUAL(1_S, events.size());
  auto message = events.at(0);
  CPPUNIT_ASSERT(message.is_object());

  CPPUNIT_ASSERT_EQUAL(string("UNAVAILABLE"), message.at("/Message/Value"_json_pointer).get<string>());
  
  stream = streams.at("/0/ComponentStream"_json_pointer);
  CPPUNIT_ASSERT(stream.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("a4a7bdf0"), stream.at("/@componentId"_json_pointer).get<string>());
  
  auto conds = stream.at("/Condition"_json_pointer);
  CPPUNIT_ASSERT(conds.is_array());
  CPPUNIT_ASSERT_EQUAL(1_S, conds.size());
  auto motion = conds.at(0);
  CPPUNIT_ASSERT(motion.is_object());
  
  CPPUNIT_ASSERT_EQUAL(string("a5b23650"), motion.at("/Unavailable/@dataItemId"_json_pointer).get<string>());

}
