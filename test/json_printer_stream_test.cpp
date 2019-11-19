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

#include "gtest/gtest.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "checkpoint.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "globals.hpp"
#include "json_helper.hpp"
#include "json_printer.hpp"
#include "observation.hpp"
#include "test_globals.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class JsonPrinterStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    m_xmlPrinter.reset(new XmlPrinter("1.5"));
    m_printer.reset(new JsonPrinter("1.5", true));
    m_config.reset(new XmlParser());
    m_devices =
        m_config->parseFile(PROJECT_ROOT_DIR "/samples/SimpleDevlce.xml", m_xmlPrinter.get());
  }

  void TearDown() override {
    m_config.reset();
    m_xmlPrinter.reset();
    m_printer.reset();
  }

  DataItem *getDataItem(const char *name) {
    for (auto &device : m_devices) {
      auto di = device->getDeviceDataItem(name);
      if (di) return di;
    }
    return nullptr;
  }

  void addObservationToCheckpoint(Checkpoint &checkpoint, const char *name, uint64_t sequence,
                                  string value, string time = "TIME") {
    const auto d = getDataItem(name);
    ASSERT_TRUE(d) << "Could not find data item " << name;
    auto event = new Observation(*d, sequence, time, value);
    checkpoint.addObservation(event);
  }

 protected:
  std::unique_ptr<JsonPrinter> m_printer;
  std::unique_ptr<XmlParser> m_config;
  std::unique_ptr<XmlPrinter> m_xmlPrinter;
  std::vector<Device *> m_devices;
};

TEST_F(JsonPrinterStreamTest, StreamHeader) {
  Checkpoint checkpoint;
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  auto jdoc = json::parse(doc);
  auto it = jdoc.begin();
  ASSERT_EQ(string("MTConnectStreams"), it.key());
  ASSERT_EQ(123, jdoc.at("/MTConnectStreams/Header/instanceId"_json_pointer).get<int32_t>());
  ASSERT_EQ(131072, jdoc.at("/MTConnectStreams/Header/bufferSize"_json_pointer).get<int32_t>());
  ASSERT_EQ(uint64_t(10254805),
            jdoc.at("/MTConnectStreams/Header/nextSequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(uint64_t(10123733),
            jdoc.at("/MTConnectStreams/Header/firstSequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(uint64_t(10123800),
            jdoc.at("/MTConnectStreams/Header/lastSequence"_json_pointer).get<uint64_t>());
}

TEST_F(JsonPrinterStreamTest, DeviceStream) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, "100");
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  // cout << "\n" << doc << endl;

  auto jdoc = json::parse(doc);
  json stream = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("SimpleCnc"), stream.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("872a3490-bd2d-0136-3eb0-0c85909298d9"),
            stream.at("/uuid"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, ComponentStream) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, "100");
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  json stream = jdoc.at(
      "/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("Linear"), stream.at("/component"_json_pointer).get<string>());
  ASSERT_EQ(string("X1"), stream.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("e373fec0"), stream.at("/componentId"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, ComponentStreamTwoComponents) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, "100");
  addObservationToCheckpoint(checkpoint, "Sspeed_act", 10254805, "500");
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(2_S, streams.size());

  json stream1 = streams.at("/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream1.is_object());
  ASSERT_EQ(string("Linear"), stream1.at("/component"_json_pointer).get<string>());
  ASSERT_EQ(string("e373fec0"), stream1.at("/componentId"_json_pointer).get<string>());

  json stream2 = streams.at("/1/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream2.is_object());
  ASSERT_EQ(string("Rotary"), stream2.at("/component"_json_pointer).get<string>());
  ASSERT_EQ(string("zf476090"), stream2.at("/componentId"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, TwoDevices) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, "100");
  addObservationToCheckpoint(checkpoint, "z2143c50", 10254805, "AVAILABLE");
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams"_json_pointer);
  ASSERT_EQ(2_S, streams.size());

  json stream1 = streams.at("/1/DeviceStream"_json_pointer);
  ASSERT_TRUE(stream1.is_object());
  ASSERT_EQ(string("SimpleCnc"), stream1.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("872a3490-bd2d-0136-3eb0-0c85909298d9"),
            stream1.at("/uuid"_json_pointer).get<string>());

  json stream2 = streams.at("/0/DeviceStream"_json_pointer);
  ASSERT_TRUE(stream2.is_object());
  ASSERT_EQ(string("SampleDevice2"), stream2.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("f2db97b0-2bd1-0137-91ba-2a0081597801"),
            stream2.at("/uuid"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, SampleAndEventDataItem) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "if36ff60", 10254804, "AUTOMATIC");  // Controller Mode
  addObservationToCheckpoint(checkpoint, "r186cd60", 10254805, "10 20 30");   // Path Position
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(1_S, streams.size());

  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("a4a7bdf0"), stream.at("/componentId"_json_pointer).get<string>());

  auto events = stream.at("/Events"_json_pointer);
  ASSERT_TRUE(events.is_array());
  auto mode = events.at(0);
  ASSERT_TRUE(mode.is_object());

  ASSERT_EQ(string("AUTOMATIC"), mode.at("/ControllerMode/value"_json_pointer).get<string>());
  ASSERT_EQ(string("if36ff60"), mode.at("/ControllerMode/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("mode"), mode.at("/ControllerMode/name"_json_pointer).get<string>());
  ASSERT_EQ(string("TIME"), mode.at("/ControllerMode/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), mode.at("/ControllerMode/sequence"_json_pointer).get<uint64_t>());

  auto samples = stream.at("/Samples"_json_pointer);
  ASSERT_TRUE(samples.is_array());
  auto pos = samples.at(0);

  ASSERT_EQ(3_S, pos.at("/PathPosition/value"_json_pointer).size());

  ASSERT_EQ(10.0, pos.at("/PathPosition/value/0"_json_pointer).get<double>());
  ASSERT_EQ(20.0, pos.at("/PathPosition/value/1"_json_pointer).get<double>());
  ASSERT_EQ(30.0, pos.at("/PathPosition/value/2"_json_pointer).get<double>());
  ASSERT_EQ(string("r186cd60"), pos.at("/PathPosition/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("TIME"), pos.at("/PathPosition/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254805), pos.at("/PathPosition/sequence"_json_pointer).get<uint64_t>());
}

TEST_F(JsonPrinterStreamTest, ConditionDataItem) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "a5b23650", 10254804,
                             "fault|syn|ack|HIGH|Syntax error");  // Motion Program Condition
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(1_S, streams.size());

  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("a4a7bdf0"), stream.at("/componentId"_json_pointer).get<string>());

  auto conds = stream.at("/Condition"_json_pointer);
  ASSERT_TRUE(conds.is_array());
  ASSERT_EQ(1_S, conds.size());
  auto motion = conds.at(0);
  ASSERT_TRUE(motion.is_object());

  ASSERT_EQ(string("a5b23650"), motion.at("/Fault/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("motion"), motion.at("/Fault/name"_json_pointer).get<string>());
  ASSERT_EQ(string("TIME"), motion.at("/Fault/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), motion.at("/Fault/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("HIGH"), motion.at("/Fault/qualifier"_json_pointer).get<string>());
  ASSERT_EQ(string("ack"), motion.at("/Fault/nativeSeverity"_json_pointer).get<string>());
  ASSERT_EQ(string("syn"), motion.at("/Fault/nativeCode"_json_pointer).get<string>());
  ASSERT_EQ(string("Syntax error"), motion.at("/Fault/value"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, TimeSeries) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "tc9edc70", 10254804,
                             "10|100|1.0 2.0 3 4 5.0 6 7 8.8 9.0 10.2");  // Volt Ampere Time Series
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(1_S, streams.size());

  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("afb91ba0"), stream.at("/componentId"_json_pointer).get<string>());

  auto samples = stream.at("/Samples"_json_pointer);
  ASSERT_TRUE(samples.is_array());
  ASSERT_EQ(1_S, samples.size());
  auto amps = samples.at(0);
  ASSERT_TRUE(amps.is_object());

  ASSERT_EQ(string("tc9edc70"),
            amps.at("/VoltAmpereTimeSeries/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("pampts"), amps.at("/VoltAmpereTimeSeries/name"_json_pointer).get<string>());
  ASSERT_EQ(string("TIME"), amps.at("/VoltAmpereTimeSeries/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804),
            amps.at("/VoltAmpereTimeSeries/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(10.0, amps.at("/VoltAmpereTimeSeries/sampleCount"_json_pointer).get<double>());
  ASSERT_EQ(100.0, amps.at("/VoltAmpereTimeSeries/sampleRate"_json_pointer).get<double>());

  auto value = amps.at("/VoltAmpereTimeSeries/value"_json_pointer);
  ASSERT_TRUE(value.is_array());
  ASSERT_EQ(10_S, value.size());

  ASSERT_EQ(1.0, value[0].get<double>());
  ASSERT_EQ(2.0, value[1].get<double>());
  ASSERT_EQ(3.0, value[2].get<double>());
  ASSERT_EQ(4.0, value[3].get<double>());
  ASSERT_EQ(5.0, value[4].get<double>());
  ASSERT_EQ(6.0, value[5].get<double>());
  ASSERT_EQ(7.0, value[6].get<double>());
  ASSERT_NEAR(8.8, value[7].get<double>(), 0.0001);
  ASSERT_EQ(9.0, value[8].get<double>());
  ASSERT_NEAR(10.2, value[9].get<double>(), 0.0001);
}

TEST_F(JsonPrinterStreamTest, AssetChanged) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "e4a300e0", 10254804,
                             "CuttingTool|31d416a0-33c7");  // asset changed
  addObservationToCheckpoint(checkpoint, "f2df7550", 10254805,
                             "QIF|400477d0-33c7");  // asset removed
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(1_S, streams.size());

  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("x872a3490"), stream.at("/componentId"_json_pointer).get<string>());

  auto events = stream.at("/Events"_json_pointer);
  ASSERT_TRUE(events.is_array());
  ASSERT_EQ(2_S, events.size());

  auto changed = events.at(0);
  ASSERT_TRUE(changed.is_object());

  ASSERT_EQ(string("e4a300e0"), changed.at("/AssetChanged/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("TIME"), changed.at("/AssetChanged/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), changed.at("/AssetChanged/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("CuttingTool"),
            changed.at("/AssetChanged/assetType"_json_pointer).get<string>());
  ASSERT_EQ(string("31d416a0-33c7"), changed.at("/AssetChanged/value"_json_pointer).get<string>());

  auto removed = events.at(1);
  ASSERT_TRUE(removed.is_object());

  ASSERT_EQ(string("f2df7550"), removed.at("/AssetRemoved/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("TIME"), removed.at("/AssetRemoved/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254805), removed.at("/AssetRemoved/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("QIF"), removed.at("/AssetRemoved/assetType"_json_pointer).get<string>());
  ASSERT_EQ(string("400477d0-33c7"), removed.at("/AssetRemoved/value"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, ResetTrigger) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "qb9212c0", 10254804, "10.0:ACTION_COMPLETE",
                             "TIME@100.0");  // Amperage
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(1_S, streams.size());

  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("afb91ba0"), stream.at("/componentId"_json_pointer).get<string>());

  auto samples = stream.at("/Samples"_json_pointer);
  ASSERT_TRUE(samples.is_array());
  ASSERT_EQ(1_S, samples.size());
  auto amp = samples.at(0);
  ASSERT_TRUE(amp.is_object());
  cout << amp.dump(2) << endl;

  ASSERT_EQ(string("qb9212c0"), amp.at("/Amperage/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("TIME"), amp.at("/Amperage/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), amp.at("/Amperage/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("ACTION_COMPLETE"),
            amp.at("/Amperage/resetTriggered"_json_pointer).get<string>());
  ASSERT_EQ(string("AVERAGE"), amp.at("/Amperage/statistic"_json_pointer).get<string>());
  ASSERT_EQ(100.0, amp.at("/Amperage/duration"_json_pointer).get<double>());
  ASSERT_EQ(10.0, amp.at("/Amperage/value"_json_pointer).get<double>());
}

TEST_F(JsonPrinterStreamTest, Message) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "m17f1750", 10254804,
                             "XXXX|XXX is on the roof");  // asset changed
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(1_S, streams.size());

  auto stream = streams.at("/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("p5add360"), stream.at("/componentId"_json_pointer).get<string>());

  auto events = stream.at("/Events"_json_pointer);
  ASSERT_TRUE(events.is_array());
  ASSERT_EQ(1_S, events.size());

  auto message = events.at(0);
  ASSERT_TRUE(message.is_object());

  ASSERT_EQ(string("m17f1750"), message.at("/Message/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("TIME"), message.at("/Message/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), message.at("/Message/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("XXXX"), message.at("/Message/nativeCode"_json_pointer).get<string>());
  ASSERT_EQ(string("XXX is on the roof"), message.at("/Message/value"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, Unavailability) {
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "m17f1750", 10254804, "|UNAVAILABLE");  // asset changed
  addObservationToCheckpoint(checkpoint, "a5b23650", 10254804,
                             "unavailable||||");  // Motion Program Condition
  ObservationPtrArray list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(2_S, streams.size());

  auto stream = streams.at("/1/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("p5add360"), stream.at("/componentId"_json_pointer).get<string>());

  auto events = stream.at("/Events"_json_pointer);
  ASSERT_TRUE(events.is_array());
  ASSERT_EQ(1_S, events.size());
  auto message = events.at(0);
  ASSERT_TRUE(message.is_object());

  ASSERT_EQ(string("UNAVAILABLE"), message.at("/Message/value"_json_pointer).get<string>());

  stream = streams.at("/0/ComponentStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("a4a7bdf0"), stream.at("/componentId"_json_pointer).get<string>());

  auto conds = stream.at("/Condition"_json_pointer);
  ASSERT_TRUE(conds.is_array());
  ASSERT_EQ(1_S, conds.size());
  auto motion = conds.at(0);
  ASSERT_TRUE(motion.is_object());

  ASSERT_EQ(string("a5b23650"), motion.at("/Unavailable/dataItemId"_json_pointer).get<string>());
}
