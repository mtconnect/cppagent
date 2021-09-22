//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "device_model/data_item.hpp"
#include "observation/observation.hpp"
#include "test_utilities.hpp"

#include <list>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::adapter;
using namespace mtconnect::entity;
using namespace mtconnect::observation;
using namespace std::literals;
using namespace date::literals;

class ObservationTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    std::map<string, string> attributes1, attributes2;

    attributes1["id"] = "1";
    attributes1["name"] = "DataItemTest1";
    attributes1["type"] = "PROGRAM";
    attributes1["category"] = "EVENT";
    m_dataItem1 = make_unique<DataItem>(attributes1);

    attributes2["id"] = "3";
    attributes2["name"] = "DataItemTest2";
    attributes2["type"] = "POSITION";
    attributes2["nativeUnits"] = "MILLIMETER";
    attributes2["subType"] = "ACTUAL";
    attributes2["category"] = "SAMPLE";
    m_dataItem2 = make_unique<DataItem>(attributes2);

    m_time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
    
    ErrorList errors;
    m_compEventA = Observation::make(m_dataItem1.get(), {{ "VALUE", "Test"s }}, m_time, errors);
    m_compEventA->setSequence(2);
    
    m_compEventB = Observation::make(m_dataItem2.get(), {{ "VALUE", 1.1231 }}, m_time + 10min, errors);
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
  std::unique_ptr<DataItem> m_dataItem1;
  std::unique_ptr<DataItem> m_dataItem2;
  Timestamp m_time;
};

inline ConditionPtr Cond(ObservationPtr ptr)
{
  return dynamic_pointer_cast<Condition>(ptr);
}

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
  ASSERT_TRUE(m_dataItem1.get() == m_compEventA->getDataItem());
  ASSERT_TRUE(m_dataItem2.get() == m_compEventB->getDataItem());

  ASSERT_EQ("Test", m_compEventA->getValue<string>());
  ASSERT_EQ(1.1231, m_compEventB->getValue<double>());
}

TEST_F(ObservationTest, ConditionEventChaining)
{
  DataItem dataItem({{"id", "c1"}, {"category", "CONDITION"},
    {"type","TEMPERATURE"}
  });
  
  ErrorList errors;
  ConditionPtr event1 = Cond(Observation::make(&dataItem, {{"level", "FAULT"s}}, m_time, errors));
  ConditionPtr event2 = Cond(Observation::make(&dataItem, {{"level", "FAULT"s}}, m_time, errors));
  ConditionPtr event3 = Cond(Observation::make(&dataItem, {{"level", "FAULT"s}}, m_time, errors));

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
  DataItem dataItem({{"id", "c1"}, {"category", "EVENT"},
    {"type","TOOL_SUFFIX"}, {"subType", "x:AUTO"}
  });

  ErrorList errors;
  auto event = Observation::make(&dataItem, {{ "VALUE", "Test"s }}, m_time, errors);
  ASSERT_EQ(0, errors.size());
  
  ASSERT_EQ("x:AUTO", dataItem.getSubType());
  ASSERT_EQ("x:AUTO", event->get<string>("subType"));
}
