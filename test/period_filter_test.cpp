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

#include <chrono>

#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/deliver.hpp"
#include "mtconnect/pipeline/delta_filter.hpp"
#include "mtconnect/pipeline/duplicate_filter.hpp"
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

struct MockPipelineContract : public PipelineContract
{
  MockPipelineContract(std::map<string, DataItemPtr> &items) : m_dataItems(items) {}
  DevicePtr findDevice(const std::string &device) override { return nullptr; }
  DataItemPtr findDataItem(const std::string &device, const std::string &name) override
  {
    return m_dataItems[name];
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override
  {
    m_observations.push_back(obs);
  }
  void deliverAsset(AssetPtr) override {}
  void deliverDevice(DevicePtr) override {}
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }

  std::map<string, DataItemPtr> &m_dataItems;

  std::vector<ObservationPtr> m_observations;
};

class PeriodFilterTest : public testing::Test
{
public:
  PeriodFilterTest() : m_strand(m_ioContext) {}

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

  void TearDown() override
  {
    m_dataItems.clear();
    m_context.reset();
    m_mapper.reset();
  }

  DataItemPtr makeDataItem(Properties attributes)
  {
    ErrorList errors;
    auto di = DataItem::make(attributes, errors);
    m_dataItems.emplace(di->getId(), di);
    m_component->addDataItem(di, errors);

    return di;
  }

  const EntityPtr observe(TokenList tokens, Timestamp now = chrono::system_clock::now())
  {
    auto ts = make_shared<Timestamped>();
    ts->m_tokens = tokens;
    ts->m_timestamp = now;
    ts->setProperty("timestamp", ts->m_timestamp);

    return (*m_mapper)(ts);
  }

  void createDataItem()
  {
    ErrorList errors;
    auto f = Filter::getFactory()->create("Filter", {{"type", "PERIOD"s}, {"VALUE", 1.0}}, errors);
    EntityList list {f};
    auto filters = DataItem::getFactory()->factoryFor("DataItem")->create("Filters", list, errors);

    makeDataItem({{"id", "a"s},
                  {"type", "POSITION"s},
                  {"category", "SAMPLE"s},
                  {"units", "MILLIMETER"s},
                  {"Filters", filters}});
  }

  auto makeFilter()
  {
    auto rate = make_shared<PeriodFilter>(m_context, m_strand);
    m_mapper->bind(rate);

    auto delivery = make_shared<DeliverObservation>(m_context);
    rate->bind(delivery);

    return rate;
  }

  auto &observations()
  {
    return static_cast<MockPipelineContract *>(m_context->m_contract.get())->m_observations;
  }

  shared_ptr<ShdrTokenMapper> m_mapper;
  std::map<string, DataItemPtr> m_dataItems;
  ComponentPtr m_component;
  shared_ptr<PipelineContext> m_context;
  boost::asio::io_context m_ioContext;
  boost::asio::io_context::strand m_strand;
};

TEST_F(PeriodFilterTest, test_simple_time_series)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();

  {
    auto os = observe({"a", "1"}, now);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "2"}, now + 200ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "3"}, now + 500ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "4"}, now + 1100ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(2, observations().size());
  }

  m_ioContext.run_for(1s);

  auto &obs = observations();
  ASSERT_EQ(3, obs.size());
  ASSERT_EQ(1.0, obs[0]->getValue<double>());
  ASSERT_EQ(3.0, obs[1]->getValue<double>());
  ASSERT_EQ(4.0, obs[2]->getValue<double>());
}

TEST_F(PeriodFilterTest, delayed_delivery)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();

  {
    auto os = observe({"a", "1"}, now);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "2"}, now + 500ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }

  m_ioContext.run_for(750ms);

  auto obs = observations();
  ASSERT_EQ(2, obs.size());
  auto end = obs.back();
  ASSERT_EQ(2.0, end->getValue<double>());
}

TEST_F(PeriodFilterTest, delayed_delivery_with_replace)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();

  {
    auto os = observe({"a", "1"}, now);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "2"}, now + 500ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "3"}, now + 750ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }

  m_ioContext.run_for(750ms);

  auto obs = observations();
  ASSERT_EQ(2, obs.size());
  auto end = obs.back();
  ASSERT_EQ(3.0, end->getValue<double>());
}

TEST_F(PeriodFilterTest, delayed_delivery_with_cancel)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();

  {
    auto os = observe({"a", "1"}, now);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "2"}, now + 500ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "3"}, now + 750ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "4"}, now + 1250ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(2, observations().size());
  }

  m_ioContext.run_for(1s);

  auto &obs = observations();
  ASSERT_EQ(3, obs.size());
  ASSERT_EQ(1.0, obs[0]->getValue<double>());
  ASSERT_EQ(3.0, obs[1]->getValue<double>());
  ASSERT_EQ(4.0, obs[2]->getValue<double>());
}

TEST_F(PeriodFilterTest, deliver_after_delayed_delivery)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();

  {
    auto os = observe({"a", "1"}, now);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "2"}, now + 500ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }

  m_ioContext.run_for(750ms);
  m_ioContext.restart();
  auto &obs = observations();
  {
    ASSERT_EQ(2, obs.size());
    ASSERT_EQ(1.0, obs[0]->getValue<double>());
    ASSERT_EQ(2.0, obs[1]->getValue<double>());
  }

  {
    auto os = observe({"a", "3"}, now + 1600ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(2, observations().size());
  }

  m_ioContext.run_for(750ms);
  m_ioContext.restart();

  {
    ASSERT_EQ(3, obs.size());
    ASSERT_EQ(3.0, obs[2]->getValue<double>());
  }

  {
    auto os = observe({"a", "4"}, now + 2600ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(3, observations().size());
  }
  {
    auto os = observe({"a", "5"}, now + 3200ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(4, observations().size());
  }

  m_ioContext.run_for(1s);
  m_ioContext.restart();

  {
    ASSERT_EQ(5, obs.size());
    ASSERT_EQ(4.0, obs[3]->getValue<double>());
    ASSERT_EQ(5.0, obs[4]->getValue<double>());
  }

  {
    auto os = observe({"a", "6"}, now + 3600ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(6, observations().size());
    ASSERT_EQ(6.0, obs[5]->getValue<double>());
  }
  {
    auto os = observe({"a", "7"}, now + 5000ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(7, observations().size());
    ASSERT_EQ(7.0, obs[6]->getValue<double>());
  }

  m_ioContext.run_for(750ms);
  m_ioContext.restart();

  ASSERT_EQ(7, obs.size());
}

TEST_F(PeriodFilterTest, streaming_observations_closely_packed)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();
  auto &obs = observations();

  {
    auto os = observe({"a", "1"}, now + 100ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "2"}, now + 400ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "3"}, now + 600ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "4"}, now + 1200ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(2, observations().size());
    ASSERT_EQ(3.0, obs[1]->getValue<double>());
  }
  {
    auto os = observe({"a", "5"}, now + 1900ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(2, observations().size());
  }
  {
    auto os = observe({"a", "6"}, now + 3100ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(4, observations().size());
    ASSERT_EQ(5.0, obs[2]->getValue<double>());
    ASSERT_EQ(6.0, obs[3]->getValue<double>());
  }
  {
    auto os = observe({"a", "7"}, now + 4500ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(5, observations().size());
  }

  m_ioContext.run_for(1s);

  ASSERT_EQ(5, obs.size());
  ASSERT_EQ(1.0, obs[0]->getValue<double>());
  ASSERT_EQ(3.0, obs[1]->getValue<double>());
  ASSERT_EQ(5.0, obs[2]->getValue<double>());
  ASSERT_EQ(6.0, obs[3]->getValue<double>());
  ASSERT_EQ(7.0, obs[4]->getValue<double>());
}

TEST_F(PeriodFilterTest, time_moving_backward)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();

  {
    auto os = observe({"a", "1"}, now + 1000ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "2"}, now + 400ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(2, observations().size());
  }
  {
    auto os = observe({"a", "3"}, now + 600ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(2, observations().size());
  }
  {
    auto os = observe({"a", "4"}, now + 1200ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(2, observations().size());
  }

  m_ioContext.run_for(1s);

  auto &obs = observations();
  ASSERT_EQ(3, obs.size());
  ASSERT_EQ(1.0, obs[0]->getValue<double>());
  ASSERT_EQ(2.0, obs[1]->getValue<double>());
  ASSERT_EQ(4.0, obs[2]->getValue<double>());
}

TEST_F(PeriodFilterTest, exact_period_spacing)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();
  auto &obs = observations();

  {
    auto os = observe({"a", "1"}, now + 0ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "2"}, now + 1000ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(2, observations().size());
  }
  {
    auto os = observe({"a", "3"}, now + 2000ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(3, observations().size());
  }

  ASSERT_EQ(3, obs.size());
  ASSERT_EQ(1.0, obs[0]->getValue<double>());
  ASSERT_EQ(2.0, obs[1]->getValue<double>());
  ASSERT_EQ(3.0, obs[2]->getValue<double>());
}

TEST_F(PeriodFilterTest, streaming_observations_spaced_temporally)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();
  auto &obs = observations();

  {
    auto os = observe({"a", "1"}, now + 100ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }

  m_ioContext.run_for(400ms);
  m_ioContext.reset();

  {
    auto os = observe({"a", "2"}, now + 400ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }

  m_ioContext.run_for(200ms);
  m_ioContext.reset();

  {
    auto os = observe({"a", "3"}, now + 600ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(1, observations().size());
  }

  m_ioContext.run_for(600ms);
  m_ioContext.reset();

  {
    auto os = observe({"a", "4"}, now + 1200ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(2, observations().size());
    ASSERT_EQ(3.0, obs[1]->getValue<double>());
  }

  m_ioContext.run_for(700ms);
  m_ioContext.reset();

  {
    auto os = observe({"a", "5"}, now + 1900ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(0, list.size());
    ASSERT_EQ(2, observations().size());
  }

  m_ioContext.run_for(1200ms);
  m_ioContext.reset();

  {
    auto os = observe({"a", "6"}, now + 3100ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(4, observations().size());
    ASSERT_EQ(5.0, obs[2]->getValue<double>());
    ASSERT_EQ(6.0, obs[3]->getValue<double>());
  }

  m_ioContext.run_for(1400ms);
  m_ioContext.reset();

  {
    auto os = observe({"a", "7"}, now + 4500ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(5, observations().size());
  }

  m_ioContext.run_for(1s);

  ASSERT_EQ(5, obs.size());
  ASSERT_EQ(1.0, obs[0]->getValue<double>());
  ASSERT_EQ(3.0, obs[1]->getValue<double>());
  ASSERT_EQ(5.0, obs[2]->getValue<double>());
  ASSERT_EQ(6.0, obs[3]->getValue<double>());
  ASSERT_EQ(7.0, obs[4]->getValue<double>());
}

TEST_F(PeriodFilterTest, unavailable_behavior)
{
  createDataItem();
  makeFilter();

  Timestamp now = chrono::system_clock::now();

  auto &obs = observations();

  {
    auto os = observe({"a", "UNAVAILABLE"}, now + 100ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, observations().size());
  }
  {
    auto os = observe({"a", "1.0"}, now + 200ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(2, observations().size());
  }
  {
    auto os = observe({"a", "UNAVAILABLE"}, now + 300ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(3, observations().size());
  }
  {
    auto os = observe({"a", "2.0"}, now + 400ms);
    auto list = os->getValue<EntityList>();
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(4, observations().size());
  }

  m_ioContext.run_for(1s);

  ASSERT_EQ(4, obs.size());
  ASSERT_TRUE(obs[0]->isUnavailable());
  ASSERT_EQ(1.0, obs[1]->getValue<double>());
  ASSERT_TRUE(obs[2]->isUnavailable());
  ASSERT_EQ(2.0, obs[3]->getValue<double>());
}
