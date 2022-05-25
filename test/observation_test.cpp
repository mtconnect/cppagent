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

#include <list>

#include "device_model/data_item/data_item.hpp"
#include "observation/observation.hpp"
#include "pipeline/convert_sample.hpp"
#include "test_utilities.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace mtconnect::entity;
using namespace mtconnect::observation;
using namespace device_model;
using namespace data_item;
using namespace std::literals;
using namespace date::literals;

class ObservationTest : public testing::Test
{
protected:
  void SetUp() override
  {
    std::map<string, string> attributes1, attributes2;

    ErrorList errors;
    m_dataItem1 = DataItem::make(
        {{"id", "1"s}, {"name", "DataItemTest1"s}, {"type", "PROGRAM"s}, {"category", "EVENT"s}},
        errors);
    m_dataItem2 = DataItem::make({{"id", "3"s},
                                  {"name", "DataItemTest2"s},
                                  {"type", "POSITION"s},
                                  {"category", "SAMPLE"s},
                                  {"subType", "ACTUAL"s},
                                  {"units", "MILLIMETER"s},
                                  {"nativeUnits", "MILLIMETER"s}},
                                 errors);

    m_time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

    m_compEventA = Observation::make(m_dataItem1, {{"VALUE", "Test"s}}, m_time, errors);
    m_compEventA->setSequence(2);

    m_compEventB = Observation::make(m_dataItem2, {{"VALUE", 1.1231}}, m_time + 10min, errors);
    m_compEventB->setSequence(4);
  }

  void TearDown() override
  {
    m_compEventA.reset();
    m_compEventB.reset();
    m_dataItem1.reset();
    m_dataItem2.reset();
  }

  ObservationPtr m_compEventA;
  ObservationPtr m_compEventB;
  DataItemPtr m_dataItem1;
  DataItemPtr m_dataItem2;
  Timestamp m_time;
  pipeline::ConvertSample m_converter;

  // Helper to test values
  void testValueHelper(std::map<std::string, std::string> &attributes, const std::string &units,
                       const std::string &nativeUnits, float expected, const double value,
                       const char *file, int line)
  {
    ErrorList errors;
    Properties ps;
    for (auto &p : attributes)
      ps.emplace(p.first, p.second);
    ps["nativeUnits"] = nativeUnits;
    ps["units"] = units;

    auto dataItem = DataItem::make(ps, errors);
    ObservationPtr sample = Observation::make(dataItem, {{"VALUE", value}}, m_time, errors);
    auto converted = m_converter(sample);

    stringstream message;
    double diff = abs(expected - converted->getValue<double>());
    message << "Unit conversion for " << nativeUnits << " failed, expected: " << expected
            << " and actual " << sample->getValue<double>() << " differ (" << diff
            << ") by more than 0.001";

    failIf(diff > 0.001, message.str(), __FILE__, __LINE__);
  }
};

inline ConditionPtr Cond(ObservationPtr ptr) { return dynamic_pointer_cast<Condition>(ptr); }

#define TEST_VALUE(attributes, units, nativeUnits, expected, value) \
  testValueHelper(attributes, units, nativeUnits, expected, value, __FILE__, __LINE__)

TEST_F(ObservationTest, GetAttributes)
{
  ASSERT_EQ("1", m_compEventA->get<string>("dataItemId"));
  ASSERT_EQ(m_time, m_compEventA->get<Timestamp>("timestamp"));
  ASSERT_FALSE(m_compEventA->hasProperty("subType"));
  ASSERT_EQ("DataItemTest1", m_compEventA->get<string>("name"));
  ASSERT_EQ(2, m_compEventA->get<int64_t>("sequence"));

  ASSERT_EQ("Test", m_compEventA->getValue<string>());

  ASSERT_EQ("3", m_compEventB->get<string>("dataItemId"));
  ASSERT_EQ(m_time + 10min, m_compEventB->get<Timestamp>("timestamp"));
  ASSERT_EQ("ACTUAL", m_compEventB->get<string>("subType"));
  ASSERT_EQ("DataItemTest2", m_compEventB->get<string>("name"));
  ASSERT_EQ(4, m_compEventB->get<int64_t>("sequence"));
}

TEST_F(ObservationTest, Getters)
{
  ASSERT_TRUE(m_dataItem1 == m_compEventA->getDataItem());
  ASSERT_TRUE(m_dataItem2 == m_compEventB->getDataItem());

  ASSERT_EQ("Test", m_compEventA->getValue<string>());
  ASSERT_EQ(1.1231, m_compEventB->getValue<double>());
}

TEST_F(ObservationTest, ConvertValue)
{
  std::map<string, string> attributes;
  attributes["id"] = "1";
  attributes["name"] = "DataItemTest1";
  attributes["type"] = "ACCELERATION";
  attributes["category"] = "SAMPLE";

  TEST_VALUE(attributes, "REVOLUTION/MINUTE", "REVOLUTION/MINUTE", 2.0f, 2.0);
  TEST_VALUE(attributes, "REVOLUTION/MINUTE", "REVOLUTION/SECOND", 2.0f * 60.0f, 2.0);
  TEST_VALUE(attributes, "KILOGRAM/MILLIMETER", "GRAM/INCH", (2.0f / 1000.0f) / 25.4f, 2.0);
  TEST_VALUE(attributes, "MILLIMETER/SECOND^3", "MILLIMETER/MINUTE^3",
             (2.0f) / (60.0f * 60.0f * 60.0f), 2.0);

  attributes["nativeScale"] = "0.5";
  TEST_VALUE(attributes, "MILLIMETER/SECOND^3", "MILLIMETER/MINUTE^3",
             (2.0f) / (60.0f * 60.0f * 60.0f * 0.5f), 2.0);
}

TEST_F(ObservationTest, ConvertSimpleUnits)
{
  std::map<string, string> attributes;
  attributes["id"] = "1";
  attributes["name"] = "DataItemTest";
  attributes["type"] = "ACCELERATION";
  attributes["category"] = "SAMPLE";

  TEST_VALUE(attributes, "MILLIMETER", "INCH", 2.0f * 25.4f, 2.0);
  TEST_VALUE(attributes, "MILLIMETER", "FOOT", 2.0f * 304.8f, 2.0);
  TEST_VALUE(attributes, "MILLIMETER", "CENTIMETER", 2.0f * 10.0f, 2.0);
  TEST_VALUE(attributes, "MILLIMETER", "DECIMETER", 2.0f * 100.0f, 2.0);
  TEST_VALUE(attributes, "MILLIMETER", "METER", 2.0f * 1000.0f, 2.0);
  TEST_VALUE(attributes, "CELSIUS", "FAHRENHEIT", (2.0f - 32.0f) * (5.0f / 9.0f), 2.0);
  TEST_VALUE(attributes, "KILOGRAM", "POUND", 2.0f * 0.45359237f, 2.0);
  TEST_VALUE(attributes, "KILOGRAM", "GRAM", 2.0f / 1000.0f, 2.0);
  TEST_VALUE(attributes, "DEGREE", "RADIAN", 2.0f * 57.2957795f, 2.0);
  TEST_VALUE(attributes, "SECOND", "MINUTE", 2.0f * 60.0f, 2.0);
  TEST_VALUE(attributes, "SECOND", "HOUR", 2.0f * 3600.0f, 2.0);
  TEST_VALUE(attributes, "MILLIMETER", "MILLIMETER", 2.0f, 2.0);
  TEST_VALUE(attributes, "PERCENT", "PERCENT", 2.0f, 2.0);
}

TEST_F(ObservationTest, ConditionEventChaining)
{
  ErrorList errors;
  auto dataItem =
      DataItem::make({{"id", "c1"s}, {"category", "CONDITION"s}, {"type", "TEMPERATURE"s}}, errors);

  ConditionPtr event1 = Cond(Observation::make(dataItem, {{"level", "FAULT"s}}, m_time, errors));
  ConditionPtr event2 = Cond(Observation::make(dataItem, {{"level", "FAULT"s}}, m_time, errors));
  ConditionPtr event3 = Cond(Observation::make(dataItem, {{"level", "FAULT"s}}, m_time, errors));

  ASSERT_TRUE(event1 == event1->getFirst());

  event1->appendTo(event2);
  ASSERT_TRUE(event1->getFirst() == event2);

  event2->appendTo(event3);
  ASSERT_TRUE(event1->getFirst() == event3);

  ASSERT_EQ(1, event1.use_count());
  ASSERT_EQ(2, event2.use_count());
  ASSERT_EQ(2, event3.use_count());

  ConditionList list;
  event1->getConditionList(list);
  ASSERT_EQ(3, list.size());
  ASSERT_TRUE(list.front() == event3);
  ASSERT_TRUE(list.back() == event1);

  ConditionList list2;
  event2->getConditionList(list2);
  ASSERT_EQ(2, list2.size());
  ASSERT_TRUE(list2.front() == event3);
  ASSERT_TRUE(list2.back() == event2);
}

TEST_F(ObservationTest, subType_prefix_should_be_passed_through)
{
  ErrorList errors;
  auto dataItem = DataItem::make(
      {{"id", "c1"s}, {"category", "EVENT"s}, {"type", "TOOL_SUFFIX"s}, {"subType", "x:auto"s}},
      errors);

  auto event = Observation::make(dataItem, {{"VALUE", "Test"s}}, m_time, errors);
  ASSERT_EQ(0, errors.size());

  ASSERT_EQ("x:AUTO", dataItem->get<string>("subType"));
  ASSERT_EQ("x:AUTO", event->get<string>("subType"));
}

// TODO: Make sure these tests are covered someplace else. Refactoring
//       Moved this functionality outside the observation.

#if 0
TEST_F(ObservationTest, Condition)
{
  DataItem dataItem({{"id", "c1"}, {"category", "EVENT"},
    {"type","TOOL_SUFFIX"}, {"subType", "x:AUTO"}
  });

  ErrorList errors;
  auto event = Observation::make(&dataItem, {{ "VALUE", "Test"s }}, m_time, errors);
  ASSERT_EQ(0, errors.size());
  
  ASSERT_EQ("x:AUTO", dataItem.getSubType());
  ASSERT_EQ("x:AUTO", event->get<string>("subType"));
}
