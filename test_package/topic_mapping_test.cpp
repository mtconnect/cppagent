//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <chrono>

#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/pipeline_context.hpp"
#include "mtconnect/pipeline/topic_mapper.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::asset;
using namespace device_model;
using namespace data_item;
using namespace std;

// main
int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MockPipelineContract : public PipelineContract
{
public:
  MockPipelineContract(std::map<string, DataItemPtr>& items, std::map<string, DevicePtr>& devices)
    : m_dataItems(items), m_devices(devices)
  {}
  DevicePtr findDevice(const std::string& name) override { return m_devices[name]; }
  DataItemPtr findDataItem(const std::string& device, const std::string& name) override
  {
    return m_dataItems[name];
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override {}
  void deliverAsset(AssetPtr) override {}
  void deliverDevices(std::list<DevicePtr>) override {}
  void deliverDevice(DevicePtr) override {}
  void deliverAssetCommand(entity::EntityPtr) override {}
  int32_t getSchemaVersion() const override { return IntDefaultSchemaVersion(); }
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList&, bool) override {}
  void sourceFailed(const std::string& id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr& obs) const override { return obs; }
  bool isValidating() const override { return false; }

  std::map<string, DataItemPtr>& m_dataItems;
  std::map<string, DevicePtr>& m_devices;
};

class TopicMappingTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_dataItems, m_devices);
    m_mapper = make_shared<TopicMapper>(m_context, "");
    m_mapper->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  }

  void TearDown() override
  {
    m_dataItems.clear();
    m_devices.clear();
  }

  DataItemPtr makeDataItem(const std::string& device, const Properties& props)
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

  DevicePtr makeDevice(const std::string& name, const Properties& props)
  {
    ErrorList errors;
    Properties ps(props);
    DevicePtr d = dynamic_pointer_cast<device_model::Device>(
        device_model::Device::getFactory()->make("Device", ps, errors));
    m_devices.emplace(d->getId(), d);

    return d;
  }

  /// @brief add a data item to a device WITHOUT registering it in the
  ///        findDataItem lookup map (forces resolution via the device scan path)
  DataItemPtr makeDeviceOnlyDataItem(const std::string& device, const Properties& props)
  {
    auto dev = m_devices.find(device);
    EXPECT_NE(m_devices.end(), dev) << "Cannot find device: " << device;
    if (dev == m_devices.end())
      return nullptr;

    Properties ps(props);
    ErrorList errors;
    auto di = DataItem::make(ps, errors);
    dev->second->addDataItem(di, errors);
    return di;
  }

  /// @brief build a TopicMapper with the given default device, bound to a pass-through
  std::shared_ptr<TopicMapper> makeMapper(const std::string& defaultDevice = "")
  {
    auto m = make_shared<TopicMapper>(m_context, defaultDevice);
    m->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
    return m;
  }

  /// @brief run a message body (and optional topic) through a mapper
  PipelineMessagePtr map(std::shared_ptr<TopicMapper>& mapper, const std::string& body,
                         std::optional<std::string> topic = std::nullopt)
  {
    Properties props {{"VALUE", body}};
    if (topic)
      props.insert({"topic", *topic});
    auto e = make_shared<Entity>("Message", props);
    return dynamic_pointer_cast<PipelineMessage>((*mapper)(std::move(e)));
  }

  shared_ptr<PipelineContext> m_context;
  shared_ptr<TopicMapper> m_mapper;
  std::map<string, DataItemPtr> m_dataItems;
  std::map<string, DevicePtr> m_devices;
};

inline DataSetEntry operator""_E(const char* c, std::size_t) { return DataSetEntry(c); }

TEST_F(TopicMappingTest, should_find_data_item_for_topic)
{
  makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  Properties props {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}};
  auto di = makeDataItem("device", props);
}

/// @test a JSON object body is wrapped as a JsonMessage carrying the default device
TEST_F(TopicMappingTest, should_map_json_object_to_json_message)
{
  auto dev = makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  auto mapper = makeMapper("device");

  auto res = map(mapper, R"({"a":"ACTIVE"})", "device/whatever");
  ASSERT_TRUE(res);
  ASSERT_TRUE(dynamic_pointer_cast<JsonMessage>(res));
  ASSERT_FALSE(dynamic_pointer_cast<DataMessage>(res));
  // The default device is attached; topic is not resolved for JSON messages
  ASSERT_EQ(dev, res->m_device.lock());
  ASSERT_FALSE(res->m_dataItem);
}

/// @test a JSON array body is also wrapped as a JsonMessage
TEST_F(TopicMappingTest, should_map_json_array_to_json_message)
{
  auto mapper = makeMapper();
  auto res = map(mapper, R"([{"a":1}])", "topic");
  ASSERT_TRUE(res);
  ASSERT_TRUE(dynamic_pointer_cast<JsonMessage>(res));
}

/// @test non-JSON body resolves a data item from `<device>/<name>`
TEST_F(TopicMappingTest, should_resolve_data_item_by_device_and_name)
{
  makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  auto di = makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto mapper = makeMapper();
  auto res = map(mapper, "ACTIVE", "device/a");
  ASSERT_TRUE(res);
  ASSERT_TRUE(dynamic_pointer_cast<DataMessage>(res));
  ASSERT_EQ(di, res->m_dataItem);
}

/// @test a single-segment topic resolves against the default device + name
TEST_F(TopicMappingTest, should_resolve_data_item_by_default_device_and_name)
{
  makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  auto di = makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto mapper = makeMapper("device");
  auto res = map(mapper, "ACTIVE", "a");
  ASSERT_TRUE(res);
  ASSERT_EQ(di, res->m_dataItem);
}

/// @test when the second segment misses, the last path segment is tried
TEST_F(TopicMappingTest, should_resolve_data_item_by_last_path_segment)
{
  makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  auto di = makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto mapper = makeMapper();
  // "foo" (segment 1) and "dev/foo/a" (whole) miss; back() == "a" resolves
  auto res = map(mapper, "ACTIVE", "dev/foo/a");
  ASSERT_TRUE(res);
  ASSERT_EQ(di, res->m_dataItem);
}

/// @test fall back to scanning the path for a device, then its data items
TEST_F(TopicMappingTest, should_resolve_device_and_data_item_by_path_scan)
{
  auto dev = makeDevice("Device", {{"id", "scan"s}, {"name", "scan"s}, {"uuid", "scan"s}});
  // data item lives on the device but is NOT in the findDataItem map, so the
  // first three lookups miss and resolution must fall through to the scan.
  auto di = makeDeviceOnlyDataItem("scan",
                                   {{"id", "z"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto mapper = makeMapper();
  auto res = map(mapper, "ACTIVE", "scan/z");
  ASSERT_TRUE(res);
  ASSERT_EQ(dev, res->m_device.lock());
  ASSERT_EQ(di, res->m_dataItem);
}

/// @test the second lookup for a topic is served from the resolution cache
TEST_F(TopicMappingTest, should_cache_resolved_topic)
{
  makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  auto di = makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto mapper = makeMapper();
  auto first = map(mapper, "ACTIVE", "device/a");
  ASSERT_EQ(di, first->m_dataItem);

  // Remove from the lookup map; a cache hit must still resolve the data item.
  m_dataItems.clear();
  auto second = map(mapper, "STOPPED", "device/a");
  ASSERT_TRUE(second);
  ASSERT_EQ(di, second->m_dataItem);
}

/// @test an unresolvable topic yields a DataMessage with no data item or device
TEST_F(TopicMappingTest, should_return_unresolved_data_message_when_no_match)
{
  auto mapper = makeMapper();
  auto res = map(mapper, "ACTIVE", "no/such/topic");
  ASSERT_TRUE(res);
  ASSERT_TRUE(dynamic_pointer_cast<DataMessage>(res));
  ASSERT_FALSE(res->m_dataItem);
  ASSERT_FALSE(res->m_device.lock());
}

/// @test a non-JSON message with no topic property is passed through unresolved
TEST_F(TopicMappingTest, should_handle_message_without_topic)
{
  auto mapper = makeMapper();
  auto res = map(mapper, "ACTIVE");  // no topic
  ASSERT_TRUE(res);
  ASSERT_TRUE(dynamic_pointer_cast<DataMessage>(res));
  ASSERT_FALSE(res->m_dataItem);
}
