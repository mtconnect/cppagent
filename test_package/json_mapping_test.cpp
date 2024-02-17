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

/// @file
/// JSON Sink tests

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <chrono>

#include "mtconnect/asset/cutting_tool.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/json_mapper.hpp"
#include "mtconnect/pipeline/pipeline_context.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::asset;
using namespace device_model;
using namespace data_item;
using namespace std;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MockPipelineContract : public PipelineContract
{
public:
  MockPipelineContract(std::map<string, DataItemPtr> &items, std::map<string, DevicePtr> &devices)
    : m_dataItems(items), m_devices(devices)
  {}
  DevicePtr findDevice(const std::string &name) override { return m_devices[name]; }
  DataItemPtr findDataItem(const std::string &device, const std::string &name) override
  {
    return m_dataItems[name];
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override {}
  void deliverAsset(AssetPtr) override {}
  void deliverDevices(std::list<DevicePtr>) override {}
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }
  int32_t getSchemaVersion() const override { return SCHEMA_VERSION(2, 3); };

  std::map<string, DataItemPtr> &m_dataItems;
  std::map<string, DevicePtr> &m_devices;
};

class JsonMappingTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_dataItems, m_devices);
    m_mapper = make_shared<JsonMapper>(m_context);
    m_mapper->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  }

  void TearDown() override
  {
    m_dataItems.clear();
    m_devices.clear();
  }

  DataItemPtr makeDataItem(const std::string &device, const Properties &props)
  {
    auto dev = m_devices.find(device);
    if (dev == m_devices.end())
    {
      EXPECT_TRUE(false) << "Cannot find device: " << device;
      return nullptr;
    }

    Properties ps(props);
    ErrorList errors;
    auto di = DataItem::make(ps, errors);
    m_dataItems.emplace(di->getId(), di);

    dev->second->addDataItem(di, errors);

    return di;
  }

  DevicePtr makeDevice(const std::string &name, const Properties &props)
  {
    ErrorList errors;
    Properties ps(props);
    DevicePtr d = dynamic_pointer_cast<device_model::Device>(
        device_model::Device::getFactory()->make("Device", ps, errors));
    m_devices.emplace(d->getId(), d);

    return d;
  }

  shared_ptr<PipelineContext> m_context;
  shared_ptr<JsonMapper> m_mapper;
  std::map<string, DataItemPtr> m_dataItems;
  std::map<string, DevicePtr> m_devices;
};

inline DataSetEntry operator"" _E(const char *c, std::size_t) { return DataSetEntry(c); }
using namespace date::literals;

/// @test verify the json mapper can map an object with a timestamp and a series of observations
TEST_F(JsonMappingTest, should_parse_simple_observations)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device", {{"id", "b"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  Properties props {{"VALUE", R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": "ACTIVE",
  "b": 123.456
}
)"s}};

  auto msg = std::make_shared<JsonMessage>("JsonMessage", props);
  msg->m_device = dev;

  auto res = (*m_mapper)(std::move(msg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(2, list.size());

  auto time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 20min;
  auto it = list.begin();

  auto obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ("ACTIVE", obs->getValue<string>());
  it++;

  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ(123.456, obs->getValue<double>());
}

/// @test verify the json mapper can map an object with a timestamp to a condition and message
TEST_F(JsonMappingTest, should_parse_conditions_and_messages)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s}, {"type", "TEMPERATURE"s}, {"category", "CONDITION"s}});
  makeDataItem("device", {{"id", "b"s}, {"type", "MESSAGE"s}, {"category", "EVENT"s}});

  Properties props {{"VALUE", R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": {
    "level": "fault",
    "nativeCode": "BAD!!!!",
    "nativeSeverity": 1000,
    "qualifier": "HIGH",
    "value": "high temperature fault"
  },
  "b": {
    "nativeCode": "ABC",
    "value": "some text"
  }
}
)"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(2, list.size());

  auto time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 20min;
  auto it = list.begin();

  auto cond = dynamic_pointer_cast<Condition>(*it);
  ASSERT_TRUE(cond);
  ASSERT_EQ("Fault", cond->getName());
  ASSERT_EQ("BAD!!!!", cond->get<std::string>("nativeCode"));
  ASSERT_EQ("HIGH", cond->get<std::string>("qualifier"));
  ASSERT_EQ("high temperature fault", cond->getValue<std::string>());
  ASSERT_EQ(time, cond->getTimestamp());

  it++;
  auto msg = dynamic_pointer_cast<Message>(*it);
  ASSERT_TRUE(msg);
  ASSERT_EQ("ABC", msg->get<std::string>("nativeCode"));
  ASSERT_EQ("some text", msg->getValue<std::string>());
  ASSERT_EQ(time, msg->getTimestamp());
}

/// @test verify the json mapper can handle path postitions
TEST_F(JsonMappingTest, should_parse_path_positions)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s},
                          {"type", "PATH_POSITION"s},
                          {"category", "SAMPLE"s},
                          {"units", "MILLIMETER_3D"s}});

  Properties props {{"VALUE", R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": [1.1, 2.2, 3.3]
}
)"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(1, list.size());

  auto time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 20min;
  auto it = list.begin();

  auto obs = dynamic_pointer_cast<ThreeSpaceSample>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("PathPosition", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  auto pos = obs->getValue<Vector>();
  ASSERT_EQ(3, pos.size());
  ASSERT_EQ(1.1, pos[0]);
  ASSERT_EQ(2.2, pos[1]);
  ASSERT_EQ(3.3, pos[2]);
}

/// @test verify the json mapper can map an array of objects with a timestamps and a series of
/// observations
TEST_F(JsonMappingTest, should_parse_an_array_of_objects)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device", {{"id", "b"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  Properties props {{"VALUE", R"(
[{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": "ACTIVE",
  "b": 100.0
},
{
  "timestamp": "2023-11-09T11:21:00Z",
  "a": "READY",
  "b": 101.0
},
{
  "timestamp": "2023-11-09T11:22:00Z",
  "a": "STOPPED",
  "b": 102.0
}]
)"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(6, list.size());

  auto time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 20min;
  auto it = list.begin();

  auto obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ("ACTIVE", obs->getValue<string>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ(100.0, obs->getValue<double>());

  // Next set
  time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 21min;

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ("READY", obs->getValue<string>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ(101.0, obs->getValue<double>());

  // Next set
  time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 22min;

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ("STOPPED", obs->getValue<string>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ(102.0, obs->getValue<double>());
}

/// @test verify the json mapper recognizes the device key
TEST_F(JsonMappingTest, should_parse_to_multiple_devices_with_device_key)
{
  auto dev1 =
      makeDevice("Device", {{"id", "device1"s}, {"name", "device1"s}, {"uuid", "device1"s}});
  auto dev2 =
      makeDevice("Device", {{"id", "device2"s}, {"name", "device2"s}, {"uuid", "device2"s}});
  makeDataItem("device1",
               {{"id", "a"s}, {"name", "e"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device1",
               {{"id", "b"s}, {"name", "p"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  makeDataItem("device2",
               {{"id", "c"s}, {"name", "e"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device2",
               {{"id", "d"s}, {"name", "p"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  Properties props {{"VALUE", R"(
{
  "device1": {
    "timestamp": "2023-11-09T11:20:00Z",
    "e": "ACTIVE",
    "p": 100.0
  },
  "device2": {
    "timestamp": "2023-11-09T11:21:00Z",
    "e": "READY",
    "p": 101.0
  }
})"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(4, list.size());

  auto it = list.begin();

  auto obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());
  ASSERT_EQ("ACTIVE", obs->getValue<string>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());
  ASSERT_EQ("b", obs->getDataItem()->getId());
  ASSERT_EQ(100.0, obs->getValue<double>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());
  ASSERT_EQ("c", obs->getDataItem()->getId());
  ASSERT_EQ("READY", obs->getValue<string>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());
  ASSERT_EQ("d", obs->getDataItem()->getId());
  ASSERT_EQ(101.0, obs->getValue<double>());
}

/// @test verify the json mapper can handle time series arrays
TEST_F(JsonMappingTest, should_parse_time_series_arrays)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s},
                          {"type", "POSITION"s},
                          {"category", "SAMPLE"s},
                          {"representation", "TIME_SERIES"s},
                          {"units", "MILLIMETER"s}});

  Properties props {{"VALUE", R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": {
    "sampleRate": 8000,
    "value": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
  }
}
)"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(1, list.size());

  auto obs = dynamic_pointer_cast<Observation>(list.front());
  ASSERT_TRUE(obs);
  ASSERT_EQ("PositionTimeSeries", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());

  auto ts = obs->getValue<Vector>();
  ASSERT_EQ(10, ts.size());

  for (double v = 1.0, i = 0; i < 10; v++, i++)
  {
    ASSERT_EQ(v, ts[i]);
  }
}

/// @test verify the json mapper defaults the timestamp when it is not given in the object
TEST_F(JsonMappingTest, should_default_the_time_to_now_when_not_given)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  Properties props {{"VALUE", R"(
{
  "a": "ACTIVE"
}
)"s}};

  auto msg = std::make_shared<JsonMessage>("JsonMessage", props);
  msg->m_device = dev;

  auto res = (*m_mapper)(std::move(msg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(1, list.size());

  auto time = std::chrono::system_clock::now();

  auto obs = dynamic_pointer_cast<Observation>(list.front());
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());

  ASSERT_NEAR(std::chrono::system_clock::to_time_t(time),
              std::chrono::system_clock::to_time_t(obs->getTimestamp()), 1);

  ASSERT_EQ("ACTIVE", obs->getValue<string>());
}

/// @test verify the json mapper can handle reset triggered for statistics
TEST_F(JsonMappingTest, should_parse_reset_triggered)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s},
                          {"type", "PATH_POSITION"s},
                          {"category", "SAMPLE"s},
                          {"units", "MILLIMETER_3D"s}});

  Properties props {{"VALUE", R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": {
    "value": [1.1, 2.2, 3.3],
    "resetTriggered": "DAILY"
  }
}
)"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(1, list.size());

  auto time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 20min;
  auto it = list.begin();

  auto obs = dynamic_pointer_cast<ThreeSpaceSample>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("PathPosition", obs->getName());

  ASSERT_EQ("DAILY", obs->get<string>("resetTriggered"));

  ASSERT_EQ(time, obs->getTimestamp());
  auto pos = obs->getValue<Vector>();
  ASSERT_EQ(3, pos.size());
  ASSERT_EQ(1.1, pos[0]);
  ASSERT_EQ(2.2, pos[1]);
  ASSERT_EQ(3.3, pos[2]);
}

/// @test verify the json mapper  can handle data sets and tables
TEST_F(JsonMappingTest, should_parse_data_sets)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s},
                          {"type", "VARIABLE"s},
                          {"category", "EVENT"s},
                          {"representation", "DATA_SET"s}});

  Properties props {{"VALUE", R"(
[{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": { 
    "k1": 123.45,
    "k2": "ABCDEF",
    "k3": 6789
  }
},
{
  "timestamp": "2023-11-09T11:20:01Z",
  "a": {
    "resetTriggered": "NEW",
    "value": {
      "k1": 123.45,
      "k2": "ABCDEF",
      "k3": 6789
    }
  }
},
{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": {
    "k1": 123.45,
    "k2": "ABCDEF",
    "k3": null
  }
}]
)"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(3, list.size());

  auto it = list.begin();

  auto obs = dynamic_pointer_cast<DataSetEvent>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("VariableDataSet", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());

  auto &set1 = obs->getDataSet();
  ASSERT_EQ(3, set1.size());

  auto dsi = set1.begin();
  ASSERT_EQ("k1", dsi->m_key);
  ASSERT_EQ(123.45, get<double>(dsi->m_value));

  dsi++;
  ASSERT_EQ("k2", dsi->m_key);
  ASSERT_EQ("ABCDEF", get<string>(dsi->m_value));

  dsi++;
  ASSERT_EQ("k3", dsi->m_key);
  ASSERT_EQ(6789, get<int64_t>(dsi->m_value));

  it++;
  obs = dynamic_pointer_cast<DataSetEvent>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("VariableDataSet", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());

  auto &set2 = obs->getDataSet();
  ASSERT_EQ(3, set2.size());

  ASSERT_EQ("NEW", obs->get<string>("resetTriggered"));

  dsi = set2.begin();
  ASSERT_EQ("k1", dsi->m_key);
  ASSERT_EQ(123.45, get<double>(dsi->m_value));

  dsi++;
  ASSERT_EQ("k2", dsi->m_key);
  ASSERT_EQ("ABCDEF", get<string>(dsi->m_value));

  dsi++;
  ASSERT_EQ("k3", dsi->m_key);
  ASSERT_EQ(6789, get<int64_t>(dsi->m_value));

  it++;
  obs = dynamic_pointer_cast<DataSetEvent>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("VariableDataSet", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());

  auto &set3 = obs->getDataSet();
  ASSERT_EQ(3, set3.size());

  dsi = set3.begin();
  ASSERT_EQ("k1", dsi->m_key);
  ASSERT_EQ(123.45, get<double>(dsi->m_value));

  dsi++;
  ASSERT_EQ("k2", dsi->m_key);
  ASSERT_EQ("ABCDEF", get<string>(dsi->m_value));

  dsi++;
  ASSERT_EQ("k3", dsi->m_key);
  ASSERT_TRUE(dsi->m_removed);
}

/// @test verify the json mapper  can handle data sets and tables
TEST_F(JsonMappingTest, should_parse_tables)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s},
                          {"type", "WORK_OFFSETS"s},
                          {"category", "EVENT"s},
                          {"representation", "TABLE"s}});

  Properties props {{"VALUE", R"(
[{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": {
    "r1": {
      "k1": 123.45
    },
    "r2": {
      "k2": "ABCDEF",
      "k3": 6789
    }
  }
},
{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": {
    "resetTriggered": "NEW",
    "value": {
      "r1": {
        "k1": 123.45,
        "k3": 6789
      },
      "r2": null
    }
  }
}]
)"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(2, list.size());

  auto it = list.begin();

  auto obs = dynamic_pointer_cast<TableEvent>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("WorkOffsetsTable", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());

  auto &set1 = obs->getDataSet();
  ASSERT_EQ(2, set1.size());
  auto dsi = set1.begin();

  ASSERT_EQ("r1", dsi->m_key);
  ASSERT_TRUE(holds_alternative<DataSet>(dsi->m_value));

  auto &row1 = get<DataSet>(dsi->m_value);
  ASSERT_EQ(1, row1.size());

  auto ri = row1.begin();
  ASSERT_EQ("k1", ri->m_key);
  ASSERT_EQ(123.45, get<double>(ri->m_value));

  dsi++;
  auto &row2 = get<DataSet>(dsi->m_value);
  ASSERT_EQ(2, row2.size());

  ri = row2.begin();
  ASSERT_EQ("k2", ri->m_key);
  ASSERT_EQ("ABCDEF", get<string>(ri->m_value));

  ri++;
  ASSERT_EQ("k3", ri->m_key);
  ASSERT_EQ(6789, get<int64_t>(ri->m_value));

  it++;
  obs = dynamic_pointer_cast<TableEvent>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("WorkOffsetsTable", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());

  auto &set2 = obs->getDataSet();
  ASSERT_EQ(2, set2.size());

  ASSERT_EQ("NEW", obs->get<string>("resetTriggered"));

  dsi = set2.begin();

  ASSERT_EQ("r1", dsi->m_key);
  ASSERT_TRUE(holds_alternative<DataSet>(dsi->m_value));

  auto &row3 = get<DataSet>(dsi->m_value);
  ASSERT_EQ(2, row3.size());

  ri = row3.begin();
  ASSERT_EQ("k1", ri->m_key);
  ASSERT_EQ(123.45, get<double>(ri->m_value));

  ri++;
  ASSERT_EQ("k3", ri->m_key);
  ASSERT_EQ(6789, get<int64_t>(ri->m_value));

  dsi++;
  ASSERT_EQ("r2", dsi->m_key);
  ASSERT_TRUE(dsi->m_removed);
}

/// @test support timestamp at the end of the object instead of the beginning
TEST_F(JsonMappingTest, should_not_require_ordered_object_keys)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device", {{"id", "b"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  Properties props {{"VALUE", R"(
{
  "a": "ACTIVE",
  "b": 123.456,
  "timestamp": "2023-11-09T11:20:00Z"
}
)"s}};

  auto msg = std::make_shared<JsonMessage>("JsonMessage", props);
  msg->m_device = dev;

  auto res = (*m_mapper)(std::move(msg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(2, list.size());

  auto time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 20min;
  auto it = list.begin();

  auto obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ("ACTIVE", obs->getValue<string>());
  it++;

  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());

  ASSERT_EQ(time, obs->getTimestamp());
  ASSERT_EQ(123.456, obs->getValue<double>());
}

/// @test verify the json mapper recognizes the device key with a top level timestamp
TEST_F(JsonMappingTest, should_parse_devices_with_common_timestamp)
{
  auto dev1 =
      makeDevice("Device", {{"id", "device1"s}, {"name", "device1"s}, {"uuid", "device1"s}});
  auto dev2 =
      makeDevice("Device", {{"id", "device2"s}, {"name", "device2"s}, {"uuid", "device2"s}});
  makeDataItem("device1",
               {{"id", "a"s}, {"name", "e"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device1",
               {{"id", "b"s}, {"name", "p"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  makeDataItem("device2",
               {{"id", "c"s}, {"name", "e"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device2",
               {{"id", "d"s}, {"name", "p"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  Properties props {{"VALUE", R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "device1": {
    "e": "ACTIVE",
    "p": 100.0
  },
  "device2": {
    "e": "READY",
    "p": 101.0
  }
})"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto time = Timestamp(date::sys_days(2023_y / nov / 9_d)) + 11h + 20min;

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(4, list.size());

  auto it = list.begin();

  auto obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());
  ASSERT_EQ("ACTIVE", obs->getValue<string>());
  ASSERT_EQ(time, obs->getTimestamp());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());
  ASSERT_EQ("b", obs->getDataItem()->getId());
  ASSERT_EQ(100.0, obs->getValue<double>());
  ASSERT_EQ(time, obs->getTimestamp());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());
  ASSERT_EQ("c", obs->getDataItem()->getId());
  ASSERT_EQ("READY", obs->getValue<string>());
  ASSERT_EQ(time, obs->getTimestamp());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());
  ASSERT_EQ("d", obs->getDataItem()->getId());
  ASSERT_EQ(101.0, obs->getValue<double>());
  ASSERT_EQ(time, obs->getTimestamp());
}

/// @test verify the the data item can contain a device name as well separated by a :
TEST_F(JsonMappingTest, should_parse_device_and_data_item_key)
{
  auto dev1 =
      makeDevice("Device", {{"id", "device1"s}, {"name", "device1"s}, {"uuid", "device1"s}});
  auto dev2 =
      makeDevice("Device", {{"id", "device2"s}, {"name", "device2"s}, {"uuid", "device2"s}});
  makeDataItem("device1",
               {{"id", "a"s}, {"name", "e"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device1",
               {{"id", "b"s}, {"name", "p"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  makeDataItem("device2",
               {{"id", "c"s}, {"name", "e"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device2",
               {{"id", "d"s}, {"name", "p"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  Properties props {{"VALUE", R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "device1:e": "ACTIVE",
  "device1:p": 100.0,
  "device2:e": "READY",
  "device2:p": 101.0
})"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(4, list.size());

  auto it = list.begin();

  auto obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());
  ASSERT_EQ("a", obs->getDataItem()->getId());
  ASSERT_EQ("ACTIVE", obs->getValue<string>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());
  ASSERT_EQ("b", obs->getDataItem()->getId());
  ASSERT_EQ(100.0, obs->getValue<double>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Execution", obs->getName());
  ASSERT_EQ("c", obs->getDataItem()->getId());
  ASSERT_EQ("READY", obs->getValue<string>());

  it++;
  obs = dynamic_pointer_cast<Observation>(*it);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Position", obs->getName());
  ASSERT_EQ("d", obs->getDataItem()->getId());
  ASSERT_EQ(101.0, obs->getValue<double>());
}

/// @test verify the json mapper can an asset in XML
TEST_F(JsonMappingTest, should_parse_xml_asset)
{
  using namespace mtconnect::asset;

  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  Properties props {{"VALUE", R"(
{
  "assets": {
    "M8010N9172N:1.0": "<CuttingToolArchetype assetId='M8010N9172N:1.0'>  <CuttingToolLifeCycle><ToolLife countDirection='UP' initial='0' limit='100' type='MINUTES'/><ToolLife countDirection='DOWN' initial='25' limit='1' type='PART_COUNT'/><ProgramToolGroup>A</ProgramToolGroup><ProgramToolNumber>10</ProgramToolNumber></CuttingToolLifeCycle></CuttingToolArchetype>"
  }
})"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(1, list.size());

  auto asset = dynamic_pointer_cast<Asset>(list.front());
  ASSERT_EQ("M8010N9172N:1.0", asset->getAssetId());
  ASSERT_EQ("CuttingToolArchetype", asset->getName());
}

/// @test if  observation is incorrect, skip levels
TEST_F(JsonMappingTest, should_skip_erroneous_values)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});
  makeDataItem("device", {{"id", "b"s}, {"type", "CONTROLLER_MODE"s}, {"category", "EVENT"s}});

  Properties props {{"VALUE", R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "a": {
      "r1": {
        "k1": 123.45
      },
      "r2": {
        "k2": "ABCDEF",
        "k3": 6789
      }
    },
   "b": "MANUAL"
})"s}};

  auto jmsg = std::make_shared<JsonMessage>("JsonMessage", props);
  jmsg->m_device = dev;

  auto res = (*m_mapper)(std::move(jmsg));
  ASSERT_TRUE(res);

  auto value = res->getValue();
  ASSERT_TRUE(std::holds_alternative<EntityList>(value));
  auto list = get<EntityList>(value);
  ASSERT_EQ(1, list.size());

  auto obs = dynamic_pointer_cast<Observation>(list.front());
  ASSERT_TRUE(obs);
  ASSERT_EQ("ControllerMode", obs->getName());
  ASSERT_EQ("b", obs->getDataItem()->getId());
  ASSERT_EQ("MANUAL", obs->getValue<string>());
}

/// @test verify the json mapper can an asset in json
TEST_F(JsonMappingTest, should_parse_json_asset) { GTEST_SKIP(); }
