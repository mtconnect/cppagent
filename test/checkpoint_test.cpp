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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "observation/checkpoint.hpp"
#include "agent_test_helper.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::observation;
using namespace mtconnect::adapter;
using namespace std::literals;
using namespace date::literals;

inline ConditionPtr Cond(ObservationPtr &ptr)
{
  return dynamic_pointer_cast<Condition>(ptr);
}

class CheckpointTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    m_checkpoint = make_unique<Checkpoint>();

    std::map<string, string> attributes1, attributes2;

    attributes1["id"] = "1";
    attributes1["name"] = "DataItemTest1";
    attributes1["type"] = "LOAD";
    attributes1["category"] = "CONDITION";
    m_dataItem1 = make_unique<DataItem>(attributes1);

    attributes2["id"] = "3";
    attributes2["name"] = "DataItemTest2";
    attributes2["type"] = "POSITION";
    attributes2["nativeUnits"] = "MILLIMETER";
    attributes2["subType"] = "ACTUAL";
    attributes2["category"] = "SAMPLE";
    m_dataItem2 = make_unique<DataItem>(attributes2);
  }

  void TearDown() override
  {
    m_checkpoint.reset();
    m_dataItem1.reset();
    m_dataItem2.reset();
  }

  std::unique_ptr<Checkpoint> m_checkpoint;
  std::unique_ptr<DataItem> m_dataItem1;
  std::unique_ptr<DataItem> m_dataItem2;
};

TEST_F(CheckpointTest, AddObservations)
{
  entity::ErrorList errors;
  Timestamp time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto warning1 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE1"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto warning2 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE2"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto normal = entity::Properties{
    {"level", "NORMAL"}
  };
  auto unavailable = entity::Properties{
    {"level", "UNAVAILABLE"}
  };
  auto value = entity::Properties{{"VALUE", "123"}};

  auto p1 = observation::Observation::make(m_dataItem1.get(), warning1, time, errors);
  ASSERT_TRUE(p1);
  ASSERT_EQ(1, p1.use_count());
  m_checkpoint->addObservation(p1);
  ASSERT_EQ(2, p1.use_count());

  auto p2 = observation::Observation::make(m_dataItem1.get(), warning2, time, errors);
  ASSERT_TRUE(p2);
  m_checkpoint->addObservation(p2);
  
  auto prev = dynamic_pointer_cast<Condition>(p2)->getPrev();
  ASSERT_TRUE(prev);
  ASSERT_EQ(p1, prev);
  
  auto p3 = observation::Observation::make(m_dataItem1.get(), normal, time, errors);
  ASSERT_TRUE(p3);
  m_checkpoint->addObservation(p3);

  prev = dynamic_pointer_cast<Condition>(p3)->getPrev();
  ASSERT_FALSE(prev);
  ASSERT_EQ(2, p1.use_count());
  ASSERT_EQ(1, p2.use_count());

  auto p4 = observation::Observation::make(m_dataItem1.get(), warning1, time, errors);
  m_checkpoint->addObservation(p4);

  prev = dynamic_pointer_cast<Condition>(p4)->getPrev();
  ASSERT_FALSE(prev);
  ASSERT_EQ(1, p3.use_count());
  
  auto p5 = observation::Observation::make(m_dataItem2.get(), value, time, errors);
  m_checkpoint->addObservation(p5);
  ASSERT_EQ(2, p5.use_count());

  auto p6 = observation::Observation::make(m_dataItem2.get(), value, time, errors);
  m_checkpoint->addObservation(p6);
  ASSERT_EQ(2, p6.use_count());
  ASSERT_EQ(1, p5.use_count());
}

TEST_F(CheckpointTest, Copy)
{
  entity::ErrorList errors;
  Timestamp time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto warning1 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE1"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto warning2 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE2"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto value = entity::Properties{{"VALUE", "123"}};
  
  auto p1 = observation::Observation::make(m_dataItem1.get(), warning1, time, errors);
  m_checkpoint->addObservation(p1);
  ASSERT_EQ(2, p1.use_count());

  auto p2 = observation::Observation::make(m_dataItem1.get(), warning2, time, errors);
  m_checkpoint->addObservation(p2);
  ASSERT_EQ(2, p2.use_count());

  auto copy = make_unique<Checkpoint>(*m_checkpoint);
  ASSERT_EQ(2, p1.use_count());
  ASSERT_EQ(3, p2.use_count());
  copy.reset();
  ASSERT_EQ(2, p2.use_count());
}

TEST_F(CheckpointTest, GetObservations)
{
  entity::ErrorList errors;
  Timestamp time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto warning1 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE1"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto warning2 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE2"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto value = entity::Properties{{"VALUE", "123"}};
  FilterSet filter;
  filter.insert(m_dataItem1->getId());
  filter.insert(m_dataItem2->getId());

  auto p = observation::Observation::make(m_dataItem1.get(), warning1, time, errors);
  m_checkpoint->addObservation(p);
  p = observation::Observation::make(m_dataItem1.get(), warning2, time, errors);
  m_checkpoint->addObservation(p);
  p = observation::Observation::make(m_dataItem2.get(), value, time, errors);
  m_checkpoint->addObservation(p);

  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  
  auto d1 = make_unique<DataItem>(attributes);
  filter.insert(d1->getId());

  p = observation::Observation::make(d1.get(), value, time, errors);
  m_checkpoint->addObservation(p);

  ObservationList list;
  m_checkpoint->getObservations(list, filter);

  ASSERT_EQ(4, list.size());

  FilterSet filter2;
  filter2.insert(m_dataItem1->getId());

  ObservationList list2;
  m_checkpoint->getObservations(list2, filter2);

  ASSERT_EQ(2, list2.size());
}

TEST_F(CheckpointTest, Filter)
{
  entity::ErrorList errors;
  Timestamp time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto warning1 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE1"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto warning2 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE2"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto value = entity::Properties{{"VALUE", "123"}};
  FilterSet filter;
  filter.insert(m_dataItem1->getId());

  auto p1 = observation::Observation::make(m_dataItem1.get(), warning1, time, errors);
  m_checkpoint->addObservation(p1);
  auto p2 = observation::Observation::make(m_dataItem1.get(), warning2, time, errors);
  m_checkpoint->addObservation(p2);
  auto p3 = observation::Observation::make(m_dataItem2.get(), value, time, errors);
  m_checkpoint->addObservation(p3);

  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  auto d1 = make_unique<DataItem>(attributes);
  auto p4 = observation::Observation::make(d1.get(), value, time, errors);
  m_checkpoint->addObservation(p4);

  ObservationList list;
  m_checkpoint->getObservations(list);

  ASSERT_EQ(4, (int)list.size());
  list.clear();

  m_checkpoint->filter(filter);
  m_checkpoint->getObservations(list);

  ASSERT_EQ(2, (int)list.size());
  d1.reset();
}

TEST_F(CheckpointTest, CopyAndFilter)
{
  entity::ErrorList errors;
  Timestamp time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto warning1 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE1"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto warning2 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE2"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto warning3 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE3"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto value = entity::Properties{{"VALUE", "123"}};
  FilterSet filter;
  filter.insert(m_dataItem1->getId());
  
  auto p1 = observation::Observation::make(m_dataItem1.get(), warning1, time, errors);
  m_checkpoint->addObservation(p1);
  auto p2 = observation::Observation::make(m_dataItem1.get(), warning2, time, errors);
  m_checkpoint->addObservation(p2);
  auto p3 = observation::Observation::make(m_dataItem2.get(), value, time, errors);
  m_checkpoint->addObservation(p3);

  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  auto d1 = make_unique<DataItem>(attributes);

  auto p4 = observation::Observation::make(d1.get(), value, time, errors);
  m_checkpoint->addObservation(p4);

  ObservationList list;
  m_checkpoint->getObservations(list);
  ASSERT_EQ(4, list.size());

  Checkpoint check(*m_checkpoint, filter);
  list.clear();
  check.getObservations(list);
  ASSERT_EQ(2, list.size());

  auto p5 = observation::Observation::make(m_dataItem1.get(), warning3, time, errors);
  check.addObservation(p5);

  list.clear();
  check.getObservations(list);
  ASSERT_EQ(3, list.size());

  auto p6 = observation::Observation::make(d1.get(), value, time, errors);
  m_checkpoint->addObservation(p6);
  
  list.clear();
  check.getObservations(list);
  ASSERT_EQ(3, list.size());
}

TEST_F(CheckpointTest, ConditionChaining)
{
  entity::ErrorList errors;
  Timestamp time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto warning1 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE1"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto warning2 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE2"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto warning3 = entity::Properties{
    {"level", "WARNING"},
    {"nativeCode", "CODE3"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto fault2 = entity::Properties{
    {"level", "FAULT"},
    {"nativeCode", "CODE2"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto normal = entity::Properties{
    {"level", "NORMAL"}
  };
  auto normal1 = entity::Properties{
    {"nativeCode", "CODE1"},
    {"level", "NORMAL"}
  };
  auto normal2 = entity::Properties{
    {"nativeCode", "CODE2"},
    {"level", "NORMAL"}
  };
  auto unavailable = entity::Properties{
    {"level", "UNAVAILABLE"}
  };
  auto value = entity::Properties{{"VALUE", "123"}};

  FilterSet filter;
  filter.insert(m_dataItem1->getId());
  ObservationList list;
  
  auto p1 = observation::Observation::make(m_dataItem1.get(), warning1, time, errors);
  m_checkpoint->addObservation(p1);
  ASSERT_EQ(2, p1.use_count());

  m_checkpoint->getObservations(list);
  ASSERT_EQ(1, list.size());
  list.clear();

  auto p2 = observation::Observation::make(m_dataItem1.get(), warning2, time, errors);
  m_checkpoint->addObservation(p2);
  ASSERT_EQ(2, p2.use_count());
  ASSERT_EQ(2, p1.use_count());

  list.clear();
  m_checkpoint->getObservations(list);
  ASSERT_EQ(2, list.size());
  ASSERT_EQ(p1, dynamic_pointer_cast<Condition>(p2)->getPrev());
  list.clear();

  auto p3 = observation::Observation::make(m_dataItem1.get(), warning3, time, errors);
  m_checkpoint->addObservation(p3);
  ASSERT_EQ(2, p3.use_count());
  ASSERT_EQ(2, p2.use_count());
  ASSERT_EQ(2, p1.use_count());
  
  ASSERT_EQ(p2, dynamic_pointer_cast<Condition>(p3)->getPrev());
  ASSERT_EQ(p1, dynamic_pointer_cast<Condition>(p2)->getPrev());
  ASSERT_FALSE(dynamic_pointer_cast<Condition>(p1)->getPrev());

  list.clear();
  m_checkpoint->getObservations(list);
  ASSERT_EQ(3, list.size());
  list.clear();

  // Replace Warning on CODE 2 with a fault
  auto p4 = observation::Observation::make(m_dataItem1.get(), fault2, time, errors);
  m_checkpoint->addObservation(p4);
  ASSERT_EQ(2, p4.use_count());
  ASSERT_EQ(1, p3.use_count());
  ASSERT_EQ(2, p2.use_count());
  ASSERT_EQ(2, p1.use_count());
  
  // Should have been deep copyied
  ASSERT_NE(p3, Cond(p4)->getPrev());

  // Codes should still match
  ASSERT_EQ(Cond(p3)->getCode(), Cond(p4)->getPrev()->getCode());
  ASSERT_EQ(2, Cond(p4)->getPrev().use_count());
  ASSERT_EQ(Cond(p1)->getCode(), Cond(p4)->getPrev()->getPrev()->getCode());
  ASSERT_EQ(2, Cond(p4)->getPrev()->getPrev().use_count());
  ASSERT_FALSE(Cond(p4)->getPrev()->getPrev()->getPrev());

  list.clear();
  m_checkpoint->getObservations(list);
  ASSERT_EQ(3, list.size());
  list.clear();

  auto p5 = observation::Observation::make(m_dataItem1.get(), normal2, time, errors);
  m_checkpoint->addObservation(p5);
  ASSERT_FALSE(Cond(p5)->getPrev());
  
  // Check cleanup
  ObservationPtr p7 = m_checkpoint->getEvents().at(std::string("1"));
  ASSERT_TRUE(p7);
  ASSERT_EQ(2, p7.use_count());
  ASSERT_NE(p5, p7);
  ASSERT_EQ(std::string("CODE3"), Cond(p7)->getCode());
  ASSERT_EQ(std::string("CODE1"), Cond(p7)->getPrev()->getCode());
  ASSERT_FALSE(Cond(p7)->getPrev()->getPrev());


  list.clear();
  m_checkpoint->getObservations(list);
  ASSERT_EQ(2, list.size());
  list.clear();

  // Clear all
  auto p6 = observation::Observation::make(m_dataItem1.get(), normal, time, errors);
  m_checkpoint->addObservation(p6);
  ASSERT_FALSE(Cond(p6)->getPrev());

  list.clear();
  m_checkpoint->getObservations(list);
  ASSERT_EQ(1, (int)list.size());
}

TEST_F(CheckpointTest, LastConditionNormal)
{
  entity::ErrorList errors;
  Timestamp time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  auto fault1 = entity::Properties{
    {"level", "FAULT"},
    {"nativeCode", "CODE1"},
    {"qualifier", "HIGH"},
    {"VALUE", "Over..."},
  };
  auto normal1 = entity::Properties{
    {"nativeCode", "CODE1"},
    {"level", "NORMAL"}
  };

  FilterSet filter;
  filter.insert(m_dataItem1->getId());
  ObservationList list;

  auto p1 = observation::Observation::make(m_dataItem1.get(), fault1, time, errors);
  m_checkpoint->addObservation(p1);

  list.clear();
  m_checkpoint->getObservations(list);
  ASSERT_EQ(1, (int)list.size());
  list.clear();

  auto p2 = observation::Observation::make(m_dataItem1.get(), normal1, time, errors);
  m_checkpoint->addObservation(p2);

  list.clear();
  m_checkpoint->getObservations(list, filter);
  ASSERT_EQ(1, list.size());

  auto p3 = Cond(list.front());
  ASSERT_EQ(Condition::NORMAL, p3->getLevel());
  ASSERT_EQ(string(""), p3->getCode());
}
