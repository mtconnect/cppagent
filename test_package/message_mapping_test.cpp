//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include <string>

#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/pipeline_context.hpp"
// adapter_pipeline.hpp defines source::adapter::Handler, and message_mapper.hpp
// references bare `string`/`Handler`, so both must be visible before it is parsed.
#include "mtconnect/source/adapter/adapter_pipeline.hpp"
using namespace std;
#include "mtconnect/pipeline/message_mapper.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::asset;
using namespace device_model;
using namespace data_item;

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
  void deliverDevice(DevicePtr) override {}
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }
  int32_t getSchemaVersion() const override { return IntDefaultSchemaVersion(); }
  bool isValidating() const override { return false; }

  std::map<string, DataItemPtr> &m_dataItems;
  std::map<string, DevicePtr> &m_devices;
};

/// @brief records every entity forwarded to it so tests can inspect what the
///        DataMapper emitted (the mapper itself returns nullptr on the
///        forwarding paths)
class CaptureTransform : public Transform
{
public:
  CaptureTransform() : Transform("CaptureTransform") { m_guard = TypeGuard<entity::Entity>(RUN); }
  entity::EntityPtr operator()(entity::EntityPtr &&entity) override
  {
    m_last = entity;
    m_count++;
    return entity;
  }

  entity::EntityPtr m_last;
  int m_count = 0;
};

class MessageMappingTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_dataItems, m_devices);
    m_mapper = make_shared<DataMapper>(m_context, nullptr);
    m_capture = make_shared<CaptureTransform>();
    m_mapper->bind(m_capture);
  }

  void TearDown() override
  {
    m_dataItems.clear();
    m_devices.clear();
  }

  DataItemPtr makeDataItem(const std::string &device, const Properties &props)
  {
    auto dev = m_devices.find(device);
    EXPECT_NE(m_devices.end(), dev) << "Cannot find device: " << device;
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

  std::shared_ptr<DataMessage> makeMessage(const Properties &props)
  {
    Properties ps(props);
    return make_shared<DataMessage>("DataMessage", ps);
  }

  shared_ptr<PipelineContext> m_context;
  shared_ptr<DataMapper> m_mapper;
  shared_ptr<CaptureTransform> m_capture;
  std::map<string, DataItemPtr> m_dataItems;
  std::map<string, DevicePtr> m_devices;
};

/// @test a message with a resolved data item is turned into an observation and the
///       data item's data source is recorded
TEST_F(MessageMappingTest, should_create_observation_for_resolved_data_item)
{
  makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  auto di = makeDataItem("device", {{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto msg = makeMessage({{"VALUE", "ACTIVE"s}, {"source", "shdr1"s}});
  msg->m_dataItem = di;

  auto res = (*m_mapper)(std::move(msg));
  ASSERT_TRUE(res);

  auto obs = dynamic_pointer_cast<Observation>(res);
  ASSERT_TRUE(obs);
  ASSERT_EQ(di, obs->getDataItem());
  ASSERT_EQ("ACTIVE", obs->getValue<string>());

  // forwarded exactly once and the data source was recorded from "source"
  ASSERT_EQ(1, m_capture->m_count);
  ASSERT_TRUE(di->getDataSource());
  ASSERT_EQ("shdr1", *di->getDataSource());
}

/// @test an invalid value for the data item type produces no observation
TEST_F(MessageMappingTest, should_return_null_for_invalid_value)
{
  makeDevice("Device", {{"id", "device"s}, {"name", "device"s}, {"uuid", "device"s}});
  // POSITION is a SAMPLE (numeric); a non-numeric value cannot be made into an observation
  auto di = makeDataItem("device", {{"id", "p"s}, {"type", "POSITION"s}, {"category", "SAMPLE"s}});

  auto msg = makeMessage({{"VALUE", "not-a-number"s}});
  msg->m_dataItem = di;

  auto res = (*m_mapper)(std::move(msg));
  ASSERT_FALSE(res);
  ASSERT_EQ(0, m_capture->m_count);
}

/// @test a message without a resolved data item is forwarded as raw SHDR data for
///       re-processing downstream
TEST_F(MessageMappingTest, should_reprocess_as_shdr_when_no_data_item)
{
  auto msg = makeMessage({{"VALUE", "2021-02-01T12:00:00Z|execution|READY"s}});
  // m_dataItem intentionally left null

  auto res = (*m_mapper)(std::move(msg));
  ASSERT_FALSE(res);  // the mapper returns null but forwards a Data entity

  ASSERT_EQ(1, m_capture->m_count);
  ASSERT_TRUE(m_capture->m_last);
  ASSERT_EQ("Data", m_capture->m_last->getName());
  ASSERT_EQ("2021-02-01T12:00:00Z|execution|READY", m_capture->m_last->getValue<string>());
}
