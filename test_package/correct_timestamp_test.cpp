//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/correct_timestamp.hpp"
#include "mtconnect/pipeline/deliver.hpp"
#include "mtconnect/pipeline/delta_filter.hpp"
#include "mtconnect/pipeline/period_filter.hpp"
#include "mtconnect/pipeline/pipeline.hpp"
#include "mtconnect/pipeline/shdr_token_mapper.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::asset;
using namespace device_model;
using namespace data_item;
using namespace entity;
using namespace std;
using namespace std::literals;
using namespace std::chrono_literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MockPipelineContract : public PipelineContract
{
public:
  MockPipelineContract(std::map<string, DataItemPtr> &items) : m_dataItems(items) {}
  DevicePtr findDevice(const std::string &device) override { return nullptr; }
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
  int32_t getSchemaVersion() const override { return IntDefaultSchemaVersion(); }
  bool isValidating() const override { return false; }
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return nullptr; }

  std::map<string, DataItemPtr> &m_dataItems;
};

class ValidateTimestampTest : public testing::Test
{
protected:
  void SetUp() override
  {
    ErrorList errors;
    m_component = Component::make("Linear", {{"id", "x"s}, {"name", "X"s}}, errors);

    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_dataItems);
    m_mapper = make_shared<ShdrTokenMapper>(m_context);
    m_mapper->bind(make_shared<NullTransform>(TypeGuard<Observations>(RUN)));
  }

  void TearDown() override { m_dataItems.clear(); }

  DataItemPtr makeDataItem(Properties attributes)
  {
    ErrorList errors;
    auto di = DataItem::make(attributes, errors);
    m_dataItems.emplace(di->getId(), di);
    m_component->addDataItem(di, errors);

    return di;
  }

  const EntityPtr observe(TokenList tokens, Timestamp now)
  {
    auto ts = make_shared<Timestamped>();
    ts->m_tokens = tokens;
    ts->m_timestamp = now;
    ts->setProperty("timestamp", ts->m_timestamp);

    return (*m_mapper)(ts);
  }

  shared_ptr<ShdrTokenMapper> m_mapper;
  std::map<string, DataItemPtr> m_dataItems;
  shared_ptr<PipelineContext> m_context;
  ComponentPtr m_component;
};

TEST_F(ValidateTimestampTest, should_not_change_timestamp_if_time_is_moving_forward)
{
  makeDataItem({{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto filter = make_shared<CorrectTimestamp>(m_context);
  m_mapper->bind(filter);
  filter->bind(make_shared<DeliverObservation>(m_context));

  auto now = chrono::system_clock::now();

  auto os1 = observe({"a", "READY"}, now);
  auto list1 = os1->getValue<EntityList>();
  ASSERT_EQ(1, list1.size());

  auto obs1 = dynamic_pointer_cast<Observation>(list1.front());
  ASSERT_EQ(now, obs1->getTimestamp());

  auto os2 = observe({"a", "ACTIVE"}, now + 1s);
  auto list2 = os2->getValue<EntityList>();
  ASSERT_EQ(1, list2.size());

  auto obs2 = dynamic_pointer_cast<Observation>(list2.front());
  ASSERT_EQ(now + 1s, obs2->getTimestamp());
}

TEST_F(ValidateTimestampTest, should_change_timestamp_if_time_is_moving_backward)
{
  makeDataItem({{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto filter = make_shared<CorrectTimestamp>(m_context);
  m_mapper->bind(filter);
  filter->bind(make_shared<DeliverObservation>(m_context));

  auto now = chrono::system_clock::now();

  auto os1 = observe({"a", "READY"}, now);
  auto list1 = os1->getValue<EntityList>();
  ASSERT_EQ(1, list1.size());

  auto obs1 = dynamic_pointer_cast<Observation>(list1.front());
  ASSERT_EQ(now, obs1->getTimestamp());

  auto os2 = observe({"a", "ACTIVE"}, now - 1s);
  auto list2 = os2->getValue<EntityList>();
  ASSERT_EQ(1, list2.size());

  auto obs2 = dynamic_pointer_cast<Observation>(list2.front());
  ASSERT_NE(now - 1s, obs2->getTimestamp());
  ASSERT_LE(now, obs2->getTimestamp());
}

TEST_F(ValidateTimestampTest, should_handle_timestamp_in_the_future)
{
  makeDataItem({{"id", "a"s}, {"type", "EXECUTION"s}, {"category", "EVENT"s}});

  auto filter = make_shared<CorrectTimestamp>(m_context);
  m_mapper->bind(filter);
  filter->bind(make_shared<DeliverObservation>(m_context));

  auto now = chrono::system_clock::now();

  {
    auto os = observe({"a", "READY"}, now - 1s);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());

    auto obs = dynamic_pointer_cast<Observation>(list.front());
    ASSERT_EQ(now - 1s, obs->getTimestamp());
  }

  {
    auto os = observe({"a", "ACTIVE"}, now + 1s);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());

    auto obs = dynamic_pointer_cast<Observation>(list.front());
    ASSERT_EQ(now + 1s, obs->getTimestamp());
  }

  {
    auto os = observe({"a", "READY"}, now);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());

    auto obs = dynamic_pointer_cast<Observation>(list.front());
    ASSERT_LT(now, obs->getTimestamp());
    ASSERT_GT(now + 10ms, obs->getTimestamp());
  }

  {
    auto os = observe({"a", "ACTIVE"}, now + 2s);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());

    auto obs = dynamic_pointer_cast<Observation>(list.front());
    ASSERT_EQ(now + 2s, obs->getTimestamp());
  }
}
