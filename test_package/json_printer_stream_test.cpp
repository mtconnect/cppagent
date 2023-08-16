//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "json_helper.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/parser/xml_parser.hpp"
#include "mtconnect/printer//json_printer.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/utilities.hpp"
#include "test_utilities.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::buffer;
using namespace mtconnect::observation;
using namespace mtconnect::entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class JsonPrinterStreamTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_xmlPrinter = std::make_unique<printer::XmlPrinter>("1.5");
    m_printer = std::make_unique<printer::JsonPrinter>(1, true);
    m_config = std::make_unique<parser::XmlParser>();
    m_devices =
        m_config->parseFile(TEST_RESOURCE_DIR "/samples/SimpleDevlce.xml", m_xmlPrinter.get());
  }

  void TearDown() override
  {
    m_config.reset();
    m_xmlPrinter.reset();
    m_printer.reset();
  }

  DataItemPtr getDataItem(const char *name)
  {
    for (auto &device : m_devices)
    {
      auto di = device->getDeviceDataItem(name);
      if (di)
        return di;
    }
    return nullptr;
  }

  void addObservationToCheckpoint(Checkpoint &checkpoint, const char *name, uint64_t sequence,
                                  Properties props, Timestamp time = chrono::system_clock::now(),
                                  std::optional<double> duration = nullopt)
  {
    const auto d = getDataItem(name);
    ASSERT_TRUE(d) << "Could not find data item " << name;
    ErrorList errors;
    if (duration)
      props["duration"] = *duration;
    auto event = Observation::make(d, props, time, errors);
    ASSERT_TRUE(event);
    ASSERT_EQ(0, errors.size());

    event->setSequence(sequence);
    checkpoint.addObservation(event);
  }

  void addObservationToList(ObservationList &list, const char *name, uint64_t sequence,
                            Properties props, Timestamp time = chrono::system_clock::now(),
                            std::optional<double> duration = nullopt)
  {
    const auto d = getDataItem(name);
    ASSERT_TRUE(d) << "Could not find data item " << name;
    ErrorList errors;
    if (duration)
      props["duration"] = *duration;
    auto event = Observation::make(d, props, time, errors);
    ASSERT_TRUE(event);
    ASSERT_EQ(0, errors.size());

    event->setSequence(sequence);
    list.emplace_back(event);
  }

protected:
  std::unique_ptr<printer::JsonPrinter> m_printer;
  std::unique_ptr<parser::XmlParser> m_config;
  std::unique_ptr<printer::XmlPrinter> m_xmlPrinter;
  std::list<DevicePtr> m_devices;
};

Properties operator"" _value(unsigned long long value)
{
  return Properties {{"VALUE", int64_t(value)}};
}

Properties operator"" _value(long double value) { return Properties {{"VALUE", double(value)}}; }

Properties operator"" _value(const char *value, size_t s)
{
  return Properties {{"VALUE", string(value)}};
}

TEST_F(JsonPrinterStreamTest, StreamHeader)
{
  Checkpoint checkpoint;
  ObservationList list;
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

TEST_F(JsonPrinterStreamTest, DeviceStream)
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, 100_value);
  ObservationList list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  json stream = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("SimpleCnc"), stream.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("872a3490-bd2d-0136-3eb0-0c85909298d9"),
            stream.at("/uuid"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, DeviceStream_version_2_one_device)
{
  m_printer = std::make_unique<printer::JsonPrinter>(2, true);

  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, 100_value);
  ObservationList list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  json stream = jdoc.at("/MTConnectStreams/Streams/DeviceStream/0"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("SimpleCnc"), stream.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("872a3490-bd2d-0136-3eb0-0c85909298d9"),
            stream.at("/uuid"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, DeviceStream_version_2_two_devices)
{
  m_printer = std::make_unique<printer::JsonPrinter>(2, true);
  m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/min_config2.xml", m_xmlPrinter.get());

  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Sspeed", 10254804, 100_value);
  addObservationToCheckpoint(checkpoint, "xex", 10254804, "ACTIVE"_value);
  ObservationList list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  json stream1 = jdoc.at("/MTConnectStreams/Streams/DeviceStream/0"_json_pointer);
  ASSERT_TRUE(stream1.is_object());

  ASSERT_EQ(string("LinuxCNC"), stream1.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("000"), stream1.at("/uuid"_json_pointer).get<string>());

  json stream2 = jdoc.at("/MTConnectStreams/Streams/DeviceStream/1"_json_pointer);
  ASSERT_TRUE(stream2.is_object());

  ASSERT_EQ(string("Other"), stream2.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(string("001"), stream2.at("/uuid"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, ComponentStream)
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, 100_value);
  ObservationList list;
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

TEST_F(JsonPrinterStreamTest, ComponentStreamTwoComponents)
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, 100_value);
  addObservationToCheckpoint(checkpoint, "Sspeed_act", 10254805, 500_value);
  ObservationList list;
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

TEST_F(JsonPrinterStreamTest, two_components_version_2)
{
  m_printer = std::make_unique<printer::JsonPrinter>(2, true);

  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, 100_value);
  addObservationToCheckpoint(checkpoint, "Sspeed_act", 10254805, 500_value);
  ObservationList list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/DeviceStream/0/ComponentStream"_json_pointer);
  ASSERT_EQ(2_S, streams.size());

  json stream1 = streams[0];
  ASSERT_TRUE(stream1.is_object());
  ASSERT_EQ(string("Linear"), stream1.at("/component"_json_pointer).get<string>());
  ASSERT_EQ(string("e373fec0"), stream1.at("/componentId"_json_pointer).get<string>());

  json stream2 = streams[1];
  ASSERT_TRUE(stream2.is_object());
  ASSERT_EQ(string("Rotary"), stream2.at("/component"_json_pointer).get<string>());
  ASSERT_EQ(string("zf476090"), stream2.at("/componentId"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, TwoDevices)
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "Xpos", 10254804, 100_value);
  addObservationToCheckpoint(checkpoint, "z2143c50", 10254805, "AVAILABLE"_value);
  ObservationList list;
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

TEST_F(JsonPrinterStreamTest, SampleAndEventDataItem)
{
  mtconnect::buffer::Checkpoint checkpoint;
  Timestamp now = chrono::system_clock::now();
  addObservationToCheckpoint(checkpoint, "if36ff60", 10254804, "AUTOMATIC"_value,
                             now);  // Controller Mode
  addObservationToCheckpoint(checkpoint, "r186cd60", 10254805,
                             Properties {{"VALUE", Vector {10, 20, 30}}}, now);  // Path Position
  ObservationList list;
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
  ASSERT_EQ(format(now), mode.at("/ControllerMode/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), mode.at("/ControllerMode/sequence"_json_pointer).get<uint64_t>());

  auto samples = stream.at("/Samples"_json_pointer);
  ASSERT_TRUE(samples.is_array());
  auto pos = samples.at(0);

  ASSERT_EQ(3_S, pos.at("/PathPosition/value"_json_pointer).size());

  ASSERT_EQ(10.0, pos.at("/PathPosition/value/0"_json_pointer).get<double>());
  ASSERT_EQ(20.0, pos.at("/PathPosition/value/1"_json_pointer).get<double>());
  ASSERT_EQ(30.0, pos.at("/PathPosition/value/2"_json_pointer).get<double>());
  ASSERT_EQ(string("r186cd60"), pos.at("/PathPosition/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(format(now), pos.at("/PathPosition/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254805), pos.at("/PathPosition/sequence"_json_pointer).get<uint64_t>());
}

TEST_F(JsonPrinterStreamTest, samples_and_events_version_2)
{
  m_printer = std::make_unique<printer::JsonPrinter>(2, true);

  ObservationList list;
  Timestamp now = chrono::system_clock::now();

  addObservationToList(list, "if36ff60", 10254804, "AUTOMATIC"_value,
                       now);  // Controller Mode
  addObservationToList(list, "r186cd60", 10254805, Properties {{"VALUE", Vector {10, 20, 30}}},
                       now);  // Path Position
  addObservationToList(list, "r186cd60", 10254806, Properties {{"VALUE", Vector {11, 21, 31}}},
                       now);  // Path Position
  addObservationToList(list, "r186cd60", 10254807, Properties {{"VALUE", Vector {12, 22, 32}}},
                       now);  // Path Position
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);
  auto jdoc = json::parse(doc);

  auto stream = jdoc.at("/MTConnectStreams/Streams/DeviceStream/0/ComponentStream/0"_json_pointer);
  ASSERT_TRUE(stream.is_object());

  ASSERT_EQ(string("a4a7bdf0"), stream.at("/componentId"_json_pointer).get<string>());

  auto mode = stream.at("/Events/ControllerMode/0"_json_pointer);
  ASSERT_TRUE(mode.is_object());

  ASSERT_EQ(string("AUTOMATIC"), mode.at("/value"_json_pointer).get<string>());
  ASSERT_EQ(string("if36ff60"), mode.at("/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(string("mode"), mode.at("/name"_json_pointer).get<string>());
  ASSERT_EQ(format(now), mode.at("/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), mode.at("/sequence"_json_pointer).get<uint64_t>());

  auto samples = stream.at("/Samples"_json_pointer);
  ASSERT_TRUE(samples.is_object());

  auto positions = samples.at("/PathPosition"_json_pointer);
  ASSERT_TRUE(positions.is_array());
  ASSERT_EQ(3_S, positions.size());

  ASSERT_EQ(10.0, positions.at("/0/value/0"_json_pointer).get<double>());
  ASSERT_EQ(20.0, positions.at("/0/value/1"_json_pointer).get<double>());
  ASSERT_EQ(30.0, positions.at("/0/value/2"_json_pointer).get<double>());

  ASSERT_EQ(string("r186cd60"), positions.at("/0/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(format(now), positions.at("/0/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254805), positions.at("/0/sequence"_json_pointer).get<uint64_t>());

  ASSERT_EQ(11.0, positions.at("/1/value/0"_json_pointer).get<double>());
  ASSERT_EQ(21.0, positions.at("/1/value/1"_json_pointer).get<double>());
  ASSERT_EQ(31.0, positions.at("/1/value/2"_json_pointer).get<double>());

  ASSERT_EQ(12.0, positions.at("/2/value/0"_json_pointer).get<double>());
  ASSERT_EQ(22.0, positions.at("/2/value/1"_json_pointer).get<double>());
  ASSERT_EQ(32.0, positions.at("/2/value/2"_json_pointer).get<double>());
}

TEST_F(JsonPrinterStreamTest, ConditionDataItem)
{
  Timestamp now = chrono::system_clock::now();
  auto time = format(now);
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "a5b23650", 10254804,
                             Properties {{"level", "fault"s},
                                         {"nativeCode", "syn"s},
                                         {"nativeSeverity", "ack"s},
                                         {"qualifier", "HIGH"s},
                                         {"VALUE", "Syntax error"s}},
                             now);
  ObservationList list;
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
  ASSERT_EQ(time, motion.at("/Fault/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), motion.at("/Fault/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("HIGH"), motion.at("/Fault/qualifier"_json_pointer).get<string>());
  ASSERT_EQ(string("ack"), motion.at("/Fault/nativeSeverity"_json_pointer).get<string>());
  ASSERT_EQ(string("syn"), motion.at("/Fault/nativeCode"_json_pointer).get<string>());
  ASSERT_EQ(string("Syntax error"), motion.at("/Fault/value"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, TimeSeries)
{
  Timestamp now = chrono::system_clock::now();
  auto time = format(now);
  Checkpoint checkpoint;
  addObservationToCheckpoint(
      checkpoint, "tc9edc70", 10254804,
      Properties {{"sampleCount", int64_t(10)},
                  {"sampleRate", 100.0},
                  {"VALUE", Vector {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.8, 9.0, 10.2}}},
      now);
  ObservationList list;
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
  ASSERT_EQ(time, amps.at("/VoltAmpereTimeSeries/timestamp"_json_pointer).get<string>());
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

TEST_F(JsonPrinterStreamTest, AssetChanged)
{
  Timestamp now = chrono::system_clock::now();
  auto time = format(now);
  Checkpoint checkpoint;
  addObservationToCheckpoint(
      checkpoint, "e4a300e0", 10254804,
      Properties {{"assetType", "CuttingTool"s}, {"VALUE", "31d416a0-33c7"s}}, now);
  addObservationToCheckpoint(checkpoint, "f2df7550", 10254805,
                             Properties {{"assetType", "QIF"s}, {"VALUE", "400477d0-33c7"s}}, now);
  ObservationList list;
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
  ASSERT_EQ(time, changed.at("/AssetChanged/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), changed.at("/AssetChanged/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("CuttingTool"),
            changed.at("/AssetChanged/assetType"_json_pointer).get<string>());
  ASSERT_EQ(string("31d416a0-33c7"), changed.at("/AssetChanged/value"_json_pointer).get<string>());

  auto removed = events.at(1);
  ASSERT_TRUE(removed.is_object());

  ASSERT_EQ(string("f2df7550"), removed.at("/AssetRemoved/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(time, removed.at("/AssetRemoved/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254805), removed.at("/AssetRemoved/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("QIF"), removed.at("/AssetRemoved/assetType"_json_pointer).get<string>());
  ASSERT_EQ(string("400477d0-33c7"), removed.at("/AssetRemoved/value"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, ResetTrigger)
{
  Timestamp now = chrono::system_clock::now();
  auto time = format(now);
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "qb9212c0", 10254804,
                             Properties {{"VALUE", 10.0}, {"resetTriggered", "ACTION_COMPLETE"s}},
                             now, 100.0);  // Amperage
  ObservationList list;
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
  // cout << amp.dump(2) << endl;

  ASSERT_EQ(string("qb9212c0"), amp.at("/Amperage/dataItemId"_json_pointer).get<string>());
  ASSERT_EQ(time, amp.at("/Amperage/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), amp.at("/Amperage/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("ACTION_COMPLETE"),
            amp.at("/Amperage/resetTriggered"_json_pointer).get<string>());
  ASSERT_EQ(string("AVERAGE"), amp.at("/Amperage/statistic"_json_pointer).get<string>());
  ASSERT_EQ(100.0, amp.at("/Amperage/duration"_json_pointer).get<double>());
  ASSERT_EQ(10.0, amp.at("/Amperage/value"_json_pointer).get<double>());
}

TEST_F(JsonPrinterStreamTest, Message)
{
  Timestamp now = chrono::system_clock::now();
  auto time = format(now);
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "m17f1750", 10254804,
                             Properties {{"nativeCode", "XXXX"s}, {"VALUE", "XXX is on the roof"s}},
                             now);
  ObservationList list;
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
  ASSERT_EQ(time, message.at("/Message/timestamp"_json_pointer).get<string>());
  ASSERT_EQ(uint64_t(10254804), message.at("/Message/sequence"_json_pointer).get<uint64_t>());
  ASSERT_EQ(string("XXXX"), message.at("/Message/nativeCode"_json_pointer).get<string>());
  ASSERT_EQ(string("XXX is on the roof"), message.at("/Message/value"_json_pointer).get<string>());
}

TEST_F(JsonPrinterStreamTest, Unavailability)
{
  Checkpoint checkpoint;
  addObservationToCheckpoint(checkpoint, "m17f1750", 10254804,
                             "UNAVAILABLE"_value);  // asset changed
  addObservationToCheckpoint(checkpoint, "dcbc0570", 10254804, "UNAVAILABLE"_value);  // X Position
  addObservationToCheckpoint(checkpoint, "a5b23650", 10254804,
                             Properties {{"level", "unavailable"s}});  // Motion Program Condition
  ObservationList list;
  checkpoint.getObservations(list);
  auto doc = m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list);

  auto jdoc = json::parse(doc);
  auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream/ComponentStreams"_json_pointer);
  ASSERT_EQ(3_S, streams.size());
  // cout << streams;

  auto stream = streams.at("/2/ComponentStream"_json_pointer);
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

  auto sample = streams.at("/1/ComponentStream"_json_pointer);
  ASSERT_TRUE(sample.is_object());

  ASSERT_EQ(string("e373fec0"), sample.at("/componentId"_json_pointer).get<string>());

  auto samples = sample.at("/Samples"_json_pointer);
  ASSERT_TRUE(samples.is_array());
  ASSERT_EQ(1_S, samples.size());
  auto position = samples.at(0);
  ASSERT_TRUE(position.is_object());
  ASSERT_EQ(string("UNAVAILABLE"), position.at("/Position/value"_json_pointer).get<string>());
}
