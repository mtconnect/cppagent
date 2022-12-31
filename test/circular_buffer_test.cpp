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

#include "agent_test_helper.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/buffer/circular_buffer.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::buffer;
using namespace mtconnect::observation;
using namespace mtconnect::source::adapter;
using namespace mtconnect::sink::rest_sink;
using namespace device_model;
using namespace entity;
using namespace data_item;
using namespace std::literals;
using namespace date::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

inline ConditionPtr Cond(ObservationPtr &ptr) { return dynamic_pointer_cast<Condition>(ptr); }

class CircularBufferTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_circularBuffer = make_unique<CircularBuffer>(4, 4);

    ErrorList errors;
    Properties d1 {
        {"id", "1"s}, {"name", "DeviceTest1"s}, {"uuid", "UnivUniqId1"s}, {"iso841Class", "4"s}};
    m_device = dynamic_pointer_cast<Device>(Device::getFactory()->make("Device", d1, errors));

    Properties c1 {{"id", "2"s}, {"name", "Comp1"s}};
    m_comp1 = Component::make("Comp1", {{"id", "2"s}, {"name", "Comp1"s}}, errors);
    m_device->addChild(m_comp1, errors);

    m_comp2 = Component::make("Comp2", {{"id", "3"s}, {"name", "Comp2"s}}, errors);
    m_device->addChild(m_comp2, errors);

    m_dataItem1 = DataItem::make(
        {{"id", "1"s}, {"type", "LOAD"s}, {"category", "CONDITION"s}, {"name", "DataItemTest1"s}},
        errors);
    m_comp1->addDataItem(m_dataItem1, errors);

    m_dataItem2 = DataItem::make({{"id", "3"s},
                                  {"type", "POSITION"s},
                                  {"category", "SAMPLE"s},
                                  {"name", "DataItemTest2"s},
                                  {"subType", "ACTUAL"s},
                                  {"units", "MILLIMETER"s},
                                  {"nativeUnits", "MILLIMETER"s}},
                                 errors);
    m_comp2->addDataItem(m_dataItem2, errors);
  }

  void TearDown() override
  {
    m_circularBuffer.reset();
    m_dataItem1.reset();
    m_dataItem2.reset();
  }

  void addSomeObservations()
  {
    entity::ErrorList errors;
    Timestamp time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
    auto warning1 = entity::Properties {
        {"level", "WARNING"s},
        {"nativeCode", "CODE1"s},
        {"qualifier", "HIGH"s},
        {"VALUE", "Over..."s},
    };
    auto warning2 = entity::Properties {
        {"level", "WARNING"s},
        {"nativeCode", "CODE2"s},
        {"qualifier", "HIGH"s},
        {"VALUE", "Over..."s},
    };
    auto normal = entity::Properties {{"level", "NORMAL"s}};
    auto unavailable = entity::Properties {{"level", "UNAVAILABLE"s}};
    auto value = entity::Properties {{"VALUE", "123"s}};

    auto p1 = observation::Observation::make(m_dataItem1, warning1, time, errors);
    m_circularBuffer->addToBuffer(p1);

    auto p2 = observation::Observation::make(m_dataItem1, warning2, time, errors);
    m_circularBuffer->addToBuffer(p2);

    auto p3 = observation::Observation::make(m_dataItem1, normal, time, errors);
    m_circularBuffer->addToBuffer(p3);

    auto p4 = observation::Observation::make(m_dataItem1, warning1, time, errors);
    m_circularBuffer->addToBuffer(p4);

    auto p5 = observation::Observation::make(m_dataItem2, value, time, errors);
    m_circularBuffer->addToBuffer(p5);

    auto p6 = observation::Observation::make(m_dataItem2, value, time, errors);
    m_circularBuffer->addToBuffer(p6);
  }

  std::unique_ptr<CircularBuffer> m_circularBuffer;
  DataItemPtr m_dataItem1;
  DataItemPtr m_dataItem2;
  DevicePtr m_device;
  ComponentPtr m_comp1;
  ComponentPtr m_comp2;
};

TEST_F(CircularBufferTest, should_add_observations_and_get_list)
{
  addSomeObservations();

  ASSERT_EQ(7, m_circularBuffer->getSequence());

  std::optional<SequenceNumber_t> start {1}, stop;
  SequenceNumber_t first, end;
  bool eob = false;
  FilterSetOpt opt;
  auto list {m_circularBuffer->getObservations(100, opt, start, stop, end, first, eob)};

  ASSERT_EQ(6, list->size());
  ASSERT_EQ(1, first);
  ASSERT_EQ(7, end);
  ASSERT_TRUE(eob);
}

TEST_F(CircularBufferTest, should_add_observations_and_get_limited)
{
  addSomeObservations();

  ASSERT_EQ(7, m_circularBuffer->getSequence());

  std::optional<SequenceNumber_t> start {1}, stop;
  SequenceNumber_t first, end;
  bool eob = false;
  FilterSetOpt opt;
  auto list {m_circularBuffer->getObservations(4, opt, start, stop, end, first, eob)};

  ASSERT_EQ(4, list->size());
  ASSERT_EQ(1, first);
  ASSERT_EQ(5, end);
  ASSERT_FALSE(eob);
}

TEST_F(CircularBufferTest, should_skip_orphaned_observations)
{
  addSomeObservations();

  ASSERT_EQ(7, m_circularBuffer->getSequence());

  m_dataItem1.reset();
  ASSERT_TRUE(m_device->removeFromList("Components", m_comp1));
  m_comp1.reset();
  ASSERT_EQ(1, m_device->getChildren()->size());

  std::optional<SequenceNumber_t> start {1}, stop;
  SequenceNumber_t first, end;
  bool eob = false;
  FilterSetOpt opt;
  auto list {m_circularBuffer->getObservations(100, opt, start, stop, end, first, eob)};

  ASSERT_EQ(2, list->size());
  ASSERT_EQ(1, first);
  ASSERT_EQ(7, end);
  ASSERT_TRUE(eob);
}
