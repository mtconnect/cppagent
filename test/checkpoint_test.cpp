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

#include "adapter.hpp"
#include "agent.hpp"
#include "checkpoint.hpp"
#include "agent_test_helper.hpp"

using namespace std;
using namespace mtconnect;

class CheckpointTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml",
                                   8, 4, "1.3", 25);
    m_checkpoint = nullptr;
    m_agentId = int64ToString(getCurrentTimeInSec());
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

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::unique_ptr<Checkpoint> m_checkpoint;
  std::string m_agentId;
  std::unique_ptr<DataItem> m_dataItem1;
  std::unique_ptr<DataItem> m_dataItem2;
};

TEST_F(CheckpointTest, AddObservations)
{
  ObservationPtr p1, p2, p3, p4, p5, p6;
  string time("NOW"), value("123"), warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."), normal("NORMAL|||"), unavailable("UNAVAILABLE|||");

  p1 = new Observation(*m_dataItem1, time, warning1, 2);
  p1->unrefer();

  ASSERT_EQ(1, (int)p1->refCount());
  m_checkpoint->addObservation(p1);
  ASSERT_EQ(2, (int)p1->refCount());

  p2 = new Observation(*m_dataItem1, time, warning2, 2);
  p2->unrefer();

  m_checkpoint->addObservation(p2);
  ASSERT_TRUE(p1.getObject() == p2->getPrev());

  p3 = new Observation(*m_dataItem1, time, normal, 2);
  p3->unrefer();

  m_checkpoint->addObservation(p3);
  ASSERT_TRUE(nullptr == p3->getPrev());
  ASSERT_EQ(2, (int)p1->refCount());
  ASSERT_EQ(1, (int)p2->refCount());

  p4 = new Observation(*m_dataItem1, time, warning1, 2);
  p4->unrefer();

  m_checkpoint->addObservation(p4);
  ASSERT_TRUE(nullptr == p4->getPrev());
  ASSERT_EQ(1, (int)p3->refCount());

  // Test non condition
  p3 = new Observation(*m_dataItem2, time, value, 2);
  p3->unrefer();
  m_checkpoint->addObservation(p3);
  ASSERT_EQ(2, (int)p3->refCount());

  p4 = new Observation(*m_dataItem2, time, value, 2);
  p4->unrefer();

  m_checkpoint->addObservation(p4);
  ASSERT_EQ(2, (int)p4->refCount());

  ASSERT_TRUE(nullptr == p4->getPrev());
  ASSERT_EQ(1, (int)p3->refCount());
}

TEST_F(CheckpointTest, Copy)
{
  ObservationPtr p1, p2, p3, p4, p5, p6;
  string time("NOW"), value("123"), warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."), normal("NORMAL|||");

  p1 = new Observation(*m_dataItem1, time, warning1, 2);
  p1->unrefer();
  m_checkpoint->addObservation(p1);
  ASSERT_EQ(2, (int)p1->refCount());

  p2 = new Observation(*m_dataItem1, time, warning2, 2);
  p2->unrefer();
  m_checkpoint->addObservation(p2);
  ASSERT_TRUE(p1.getObject() == p2->getPrev());
  ASSERT_EQ(2, (int)p2->refCount());

  auto copy = new Checkpoint(*m_checkpoint);
  ASSERT_EQ(2, (int)p1->refCount());
  ASSERT_EQ(3, (int)p2->refCount());
  delete copy;
  copy = nullptr;
  ASSERT_EQ(2, (int)p2->refCount());
}

TEST_F(CheckpointTest, GetObservations)
{
  ObservationPtr p;
  string time("NOW"), value("123"), warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."), normal("NORMAL|||");
  std::set<string> filter;
  filter.insert(m_dataItem1->getId());

  p = new Observation(*m_dataItem1, time, warning1, 2);
  m_checkpoint->addObservation(p);
  p->unrefer();

  p = new Observation(*m_dataItem1, time, warning2, 2);
  m_checkpoint->addObservation(p);
  p->unrefer();

  filter.insert(m_dataItem2->getId());
  p = new Observation(*m_dataItem2, time, value, 2);
  m_checkpoint->addObservation(p);
  p->unrefer();

  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  auto d1 = make_unique<DataItem>(attributes);
  filter.insert(d1->getId());

  p = new Observation(*d1, time, value, 2);
  m_checkpoint->addObservation(p);
  p->unrefer();

  ObservationPtrArray list;
  m_checkpoint->getObservations(list, filter);

  ASSERT_EQ(4, (int)list.size());

  std::set<string> filter2;
  filter2.insert(m_dataItem1->getId());

  ObservationPtrArray list2;
  m_checkpoint->getObservations(list2, filter2);

  ASSERT_EQ(2, (int)list2.size());

  d1.reset();
}

TEST_F(CheckpointTest, Filter)
{
  ObservationPtr p1, p2, p3, p4;
  string time("NOW"), value("123"), warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."), normal("NORMAL|||");
  std::set<string> filter;
  filter.insert(m_dataItem1->getId());

  p1 = new Observation(*m_dataItem1, time, warning1);
  m_checkpoint->addObservation(p1);
  p1->unrefer();

  p2 = new Observation(*m_dataItem1, time, warning2);
  m_checkpoint->addObservation(p2);
  p2->unrefer();

  p3 = new Observation(*m_dataItem2, time, value);
  m_checkpoint->addObservation(p3);
  p3->unrefer();

  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  auto d1 = make_unique<DataItem>(attributes);

  p4 = new Observation(*d1, time, value);
  m_checkpoint->addObservation(p4);
  p4->unrefer();

  ObservationPtrArray list;
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
  ObservationPtr p;
  string time("NOW"), value("123"), warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."), warning3("WARNING|CODE3|HIGH|Over..."),
      normal("NORMAL|||");
  std::set<string> filter;
  filter.insert(m_dataItem1->getId());

  p = new Observation(*m_dataItem1, time, warning1);
  m_checkpoint->addObservation(p);
  p->unrefer();

  p = new Observation(*m_dataItem1, time, warning2);
  m_checkpoint->addObservation(p);
  p->unrefer();

  p = new Observation(*m_dataItem2, time, value);
  m_checkpoint->addObservation(p);
  p->unrefer();

  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  auto d1 = make_unique<DataItem>(attributes);

  p = new Observation(*d1, time, value);
  m_checkpoint->addObservation(p);
  p->unrefer();

  ObservationPtrArray list;
  m_checkpoint->getObservations(list);

  ASSERT_EQ(4, (int)list.size());
  list.clear();

  Checkpoint check(*m_checkpoint, filter);
  check.getObservations(list);

  ASSERT_EQ(2, (int)list.size());
  list.clear();

  p = new Observation(*m_dataItem1, time, warning3);
  check.addObservation(p);
  p->unrefer();

  check.getObservations(list);

  ASSERT_EQ(3, (int)list.size());
  list.clear();

  p = new Observation(*d1, time, value);
  m_checkpoint->addObservation(p);
  p->unrefer();

  check.getObservations(list);
  ASSERT_EQ(3, (int)list.size());

  d1.reset();
}

TEST_F(CheckpointTest, ConditionChaining)
{
  ObservationPtr p1, p2, p3, p4, p5, p6;
  string time("NOW"), value("123"), warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."), fault2("FAULT|CODE2|HIGH|Over..."),
      warning3("WARNING|CODE3|HIGH|Over..."), normal("NORMAL|||"), normal1("NORMAL|CODE1||"),
      normal2("NORMAL|CODE2||"), unavailable("UNAVAILABLE|||");

  std::set<string> filter;
  filter.insert(m_dataItem1->getId());
  ObservationPtrArray list;

  p1 = new Observation(*m_dataItem1, time, warning1);
  p1->unrefer();
  m_checkpoint->addObservation(p1);

  m_checkpoint->getObservations(list);
  ASSERT_EQ(1, (int)list.size());
  list.clear();

  p2 = new Observation(*m_dataItem1, time, warning2);
  p2->unrefer();

  m_checkpoint->addObservation(p2);
  ASSERT_TRUE(p1.getObject() == p2->getPrev());

  m_checkpoint->getObservations(list);
  ASSERT_EQ(2, (int)list.size());
  list.clear();

  p3 = new Observation(*m_dataItem1, time, warning3);
  p3->unrefer();

  m_checkpoint->addObservation(p3);
  ASSERT_TRUE(p2.getObject() == p3->getPrev());
  ASSERT_TRUE(p1.getObject() == p2->getPrev());
  ASSERT_TRUE(nullptr == p1->getPrev());

  m_checkpoint->getObservations(list);
  ASSERT_EQ(3, (int)list.size());
  list.clear();

  // Replace Warning on CODE 2 with a fault
  p4 = new Observation(*m_dataItem1, time, fault2);
  p4->unrefer();

  m_checkpoint->addObservation(p4);
  ASSERT_EQ(2, (int)p4->refCount());
  ASSERT_EQ(1, (int)p3->refCount());
  ASSERT_EQ(2, (int)p2->refCount());
  ASSERT_EQ(2, (int)p1->refCount());

  // Should have been deep copyied
  ASSERT_TRUE(p3.getObject() != p4->getPrev());

  // Codes should still match
  ASSERT_EQ(p3->getCode(), p4->getPrev()->getCode());
  ASSERT_EQ(1, (int)p4->getPrev()->refCount());
  ASSERT_EQ(p1->getCode(), p4->getPrev()->getPrev()->getCode());
  ASSERT_EQ(1, (int)p4->getPrev()->getPrev()->refCount());
  ASSERT_TRUE(nullptr == p4->getPrev()->getPrev()->getPrev());

  m_checkpoint->getObservations(list);
  ASSERT_EQ(3, (int)list.size());
  list.clear();

  // Clear Code 2
  p5 = new Observation(*m_dataItem1, time, normal2);
  p5->unrefer();

  m_checkpoint->addObservation(p5);
  ASSERT_TRUE(nullptr == p5->getPrev());

  // Check cleanup
  ObservationPtr *p7 = m_checkpoint->getEvents().at(std::string("1"));
  ASSERT_EQ(1, (int)(*p7)->refCount());

  ASSERT_TRUE(p7);
  ASSERT_TRUE(p5.getObject() != (*p7).getObject());
  ASSERT_EQ(std::string("CODE3"), (*p7)->getCode());
  ASSERT_EQ(std::string("CODE1"), (*p7)->getPrev()->getCode());
  ASSERT_TRUE(nullptr == (*p7)->getPrev()->getPrev());

  m_checkpoint->getObservations(list);
  ASSERT_EQ(2, (int)list.size());
  list.clear();

  // Clear all
  p6 = new Observation(*m_dataItem1, time, normal);
  p6->unrefer();

  m_checkpoint->addObservation(p6);
  ASSERT_TRUE(nullptr == p6->getPrev());

  m_checkpoint->getObservations(list);
  ASSERT_EQ(1, (int)list.size());
  list.clear();
}

TEST_F(CheckpointTest, LastConditionNormal)
{
  ObservationPtr p1, p2, p3;

  string time("NOW"), fault1("FAULT|CODE1|HIGH|Over..."), normal1("NORMAL|CODE1||");

  std::set<string> filter;
  filter.insert(m_dataItem1->getId());
  ObservationPtrArray list;

  p1 = new Observation(*m_dataItem1, time, fault1);
  p1->unrefer();
  m_checkpoint->addObservation(p1);

  m_checkpoint->getObservations(list);
  ASSERT_EQ(1, (int)list.size());
  list.clear();

  p2 = new Observation(*m_dataItem1, time, normal1);
  p2->unrefer();
  m_checkpoint->addObservation(p2);

  m_checkpoint->getObservations(list, filter);
  ASSERT_EQ(1, (int)list.size());

  p3 = list[0];
  ASSERT_EQ(Observation::NORMAL, p3->getLevel());
  ASSERT_EQ(string(""), p3->getCode());
}
