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

#include "Cuti.h"
#include "checkpoint.hpp"
#include "adapter.hpp"
#include "agent.hpp"
#include "test_globals.hpp"


using namespace std;
using namespace mtconnect;

TEST_CLASS(CheckpointTest)
{
protected:
  std::unique_ptr<Checkpoint> m_checkpoint;
  std::unique_ptr<Agent> m_agent;
  std::string m_agentId;
  std::unique_ptr<DataItem> m_dataItem1;
  std::unique_ptr<DataItem> m_dataItem2;
  
public:
  void testAddObservations();
  void testCopy();
  void testGetObservations();
  void testFilter();
  void testCopyAndFilter();
  void testConditionChaining();
  void testLastConditionNormal();
  
  SET_UP();
  TEAR_DOWN();
  
  CPPUNIT_TEST_SUITE(CheckpointTest);
  CPPUNIT_TEST(testAddObservations);
  CPPUNIT_TEST(testCopy);
  CPPUNIT_TEST(testGetObservations);
  CPPUNIT_TEST(testFilter);
  CPPUNIT_TEST(testCopyAndFilter);
  CPPUNIT_TEST(testConditionChaining);
  CPPUNIT_TEST(testLastConditionNormal);
  CPPUNIT_TEST_SUITE_END();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(CheckpointTest);

void CheckpointTest::setUp()
{
  // Create an agent with only 16 slots and 8 data items.
  m_agent = nullptr;
  m_checkpoint = nullptr;
  m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/min_config.xml", 4, 4, "1.5");
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


void CheckpointTest::tearDown()
{
  m_agent.reset();
  m_checkpoint.reset();
  m_dataItem1.reset();
  m_dataItem2.reset();
}


void CheckpointTest::testAddObservations()
{
  ObservationPtr p1, p2, p3, p4, p5, p6;
  string time("NOW"), value("123"),
  warning1("WARNING|CODE1|HIGH|Over..."),
  warning2("WARNING|CODE2|HIGH|Over..."),
  normal("NORMAL|||"),
  unavailable("UNAVAILABLE|||");
  
  p1 = new Observation(*m_dataItem1, 2, time, warning1);
  p1->unrefer();
  
  CPPUNIT_ASSERT_EQUAL(1, (int) p1->refCount());
  m_checkpoint->addObservation(p1);
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  
  p2 = new Observation(*m_dataItem1, 2, time, warning2);
  p2->unrefer();
  
  m_checkpoint->addObservation(p2);
  CPPUNIT_ASSERT(p1.getObject() == p2->getPrev());
  
  p3 = new Observation(*m_dataItem1, 2, time, normal);
  p3->unrefer();
  
  m_checkpoint->addObservation(p3);
  CPPUNIT_ASSERT(nullptr ==  p3->getPrev());
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  CPPUNIT_ASSERT_EQUAL(1, (int) p2->refCount());
  
  p4 = new Observation(*m_dataItem1, 2, time, warning1);
  p4->unrefer();
  
  m_checkpoint->addObservation(p4);
  CPPUNIT_ASSERT(nullptr == p4->getPrev());
  CPPUNIT_ASSERT_EQUAL(1, (int) p3->refCount());
  
  // Test non condition
  p3 = new Observation(*m_dataItem2, 2, time, value);
  p3->unrefer();
  m_checkpoint->addObservation(p3);
  CPPUNIT_ASSERT_EQUAL(2, (int) p3->refCount());
  
  p4 = new Observation(*m_dataItem2, 2, time, value);
  p4->unrefer();
  
  m_checkpoint->addObservation(p4);
  CPPUNIT_ASSERT_EQUAL(2, (int) p4->refCount());
  
  CPPUNIT_ASSERT(nullptr ==  p4->getPrev());
  CPPUNIT_ASSERT_EQUAL(1, (int) p3->refCount());
}


void CheckpointTest::testCopy()
{
  ObservationPtr p1, p2, p3, p4, p5, p6;
  string time("NOW"), value("123"),
  warning1("WARNING|CODE1|HIGH|Over..."),
  warning2("WARNING|CODE2|HIGH|Over..."),
  normal("NORMAL|||");
  
  p1 = new Observation(*m_dataItem1, 2, time, warning1);
  p1->unrefer();
  m_checkpoint->addObservation(p1);
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  
  p2 = new Observation(*m_dataItem1, 2, time, warning2);
  p2->unrefer();
  m_checkpoint->addObservation(p2);
  CPPUNIT_ASSERT(p1.getObject() == p2->getPrev());
  CPPUNIT_ASSERT_EQUAL(2, (int) p2->refCount());
  
  auto copy = new Checkpoint(*m_checkpoint);
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  CPPUNIT_ASSERT_EQUAL(3, (int) p2->refCount());
  delete copy; copy = nullptr;
  CPPUNIT_ASSERT_EQUAL(2, (int) p2->refCount());
}


void CheckpointTest::testGetObservations()
{
  ObservationPtr p;
  string time("NOW"), value("123"),
  warning1("WARNING|CODE1|HIGH|Over..."),
  warning2("WARNING|CODE2|HIGH|Over..."),
  normal("NORMAL|||");
  std::set<string> filter;
  filter.insert(m_dataItem1->getId());
  
  p = new Observation(*m_dataItem1, 2, time, warning1);
  m_checkpoint->addObservation(p);
  p->unrefer();
  
  p = new Observation(*m_dataItem1, 2, time, warning2);
  m_checkpoint->addObservation(p);
  p->unrefer();
  
  filter.insert(m_dataItem2->getId());
  p = new Observation(*m_dataItem2, 2, time, value);
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
  
  p = new Observation(*d1, 2, time, value);
  m_checkpoint->addObservation(p);
  p->unrefer();
  
  ObservationPtrArray list;
  m_checkpoint->getObservations(list, &filter);
  
  CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
  
  std::set<string> filter2;
  filter2.insert(m_dataItem1->getId());
  
  ObservationPtrArray list2;
  m_checkpoint->getObservations(list2, &filter2);
  
  CPPUNIT_ASSERT_EQUAL(2, (int) list2.size());
  
  d1.reset();
}


void CheckpointTest::testFilter()
{
  ObservationPtr p1, p2, p3, p4;
  string time("NOW"), value("123"),
  warning1("WARNING|CODE1|HIGH|Over..."),
  warning2("WARNING|CODE2|HIGH|Over..."),
  normal("NORMAL|||");
  std::set<string> filter;
  filter.insert(m_dataItem1->getId());
  
  p1 = new Observation(*m_dataItem1, 2, time, warning1);
  m_checkpoint->addObservation(p1);
  p1->unrefer();
  
  p2 = new Observation(*m_dataItem1, 2, time, warning2);
  m_checkpoint->addObservation(p2);
  p2->unrefer();
  
  p3 = new Observation(*m_dataItem2, 2, time, value);
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
  
  p4 = new Observation(*d1, 2, time, value);
  m_checkpoint->addObservation(p4);
  p4->unrefer();
  
  ObservationPtrArray list;
  m_checkpoint->getObservations(list);
  
  CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
  list.clear();
  
  m_checkpoint->filter(filter);
  m_checkpoint->getObservations(list);
  
  CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
  d1.reset();
}


void CheckpointTest::testCopyAndFilter()
{
  ObservationPtr p;
  string time("NOW"), value("123"),
  warning1("WARNING|CODE1|HIGH|Over..."),
  warning2("WARNING|CODE2|HIGH|Over..."),
  warning3("WARNING|CODE3|HIGH|Over..."),
  normal("NORMAL|||");
  std::set<string> filter;
  filter.insert(m_dataItem1->getId());
  
  p = new Observation(*m_dataItem1, 2, time, warning1);
  m_checkpoint->addObservation(p);
  p->unrefer();
  
  p = new Observation(*m_dataItem1, 2, time, warning2);
  m_checkpoint->addObservation(p);
  p->unrefer();
  
  p = new Observation(*m_dataItem2, 2, time, value);
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
  
  p = new Observation(*d1, 2, time, value);
  m_checkpoint->addObservation(p);
  p->unrefer();
  
  ObservationPtrArray list;
  m_checkpoint->getObservations(list);
  
  CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
  list.clear();
  
  Checkpoint check(*m_checkpoint, &filter);
  check.getObservations(list);
  
  CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
  list.clear();
  
  p = new Observation(*m_dataItem1, 2, time, warning3);
  check.addObservation(p);
  p->unrefer();
  
  check.getObservations(list);
  
  CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
  list.clear();
  
  p = new Observation(*d1, 2, time, value);
  m_checkpoint->addObservation(p);
  p->unrefer();
  
  check.getObservations(list);
  CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
  
  d1.reset();
}


void CheckpointTest::testConditionChaining()
{
  ObservationPtr p1, p2, p3, p4, p5, p6;
  string time("NOW"),
  value("123"),
  warning1("WARNING|CODE1|HIGH|Over..."),
  warning2("WARNING|CODE2|HIGH|Over..."),
  fault2("FAULT|CODE2|HIGH|Over..."),
  warning3("WARNING|CODE3|HIGH|Over..."),
  normal("NORMAL|||"),
  normal1("NORMAL|CODE1||"),
  normal2("NORMAL|CODE2||"),
  unavailable("UNAVAILABLE|||");
  
  std::set<string> filter;
  filter.insert(m_dataItem1->getId());
  ObservationPtrArray list;
  
  p1 = new Observation(*m_dataItem1, 2, time, warning1);
  p1->unrefer();
  m_checkpoint->addObservation(p1);
  
  m_checkpoint->getObservations(list);
  CPPUNIT_ASSERT_EQUAL(1, (int) list.size());
  list.clear();
  
  p2 = new Observation(*m_dataItem1, 2, time, warning2);
  p2->unrefer();
  
  m_checkpoint->addObservation(p2);
  CPPUNIT_ASSERT(p1.getObject() == p2->getPrev());
  
  m_checkpoint->getObservations(list);
  CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
  list.clear();
  
  p3 = new Observation(*m_dataItem1, 2, time, warning3);
  p3->unrefer();
  
  m_checkpoint->addObservation(p3);
  CPPUNIT_ASSERT(p2.getObject() == p3->getPrev());
  CPPUNIT_ASSERT(p1.getObject() == p2->getPrev());
  CPPUNIT_ASSERT(nullptr == p1->getPrev());
  
  m_checkpoint->getObservations(list);
  CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
  list.clear();
  
  // Replace Warning on CODE 2 with a fault
  p4 = new Observation(*m_dataItem1, 2, time, fault2);
  p4->unrefer();
  
  m_checkpoint->addObservation(p4);
  CPPUNIT_ASSERT_EQUAL(2, (int) p4->refCount());
  CPPUNIT_ASSERT_EQUAL(1, (int) p3->refCount());
  CPPUNIT_ASSERT_EQUAL(2, (int) p2->refCount());
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  
  // Should have been deep copyied
  CPPUNIT_ASSERT(p3.getObject() != p4->getPrev());
  
  // Codes should still match
  CPPUNIT_ASSERT_EQUAL(p3->getCode(), p4->getPrev()->getCode());
  CPPUNIT_ASSERT_EQUAL(1, (int) p4->getPrev()->refCount());
  CPPUNIT_ASSERT_EQUAL(p1->getCode(), p4->getPrev()->getPrev()->getCode());
  CPPUNIT_ASSERT_EQUAL(1, (int) p4->getPrev()->getPrev()->refCount());
  CPPUNIT_ASSERT(nullptr == p4->getPrev()->getPrev()->getPrev());
  
  m_checkpoint->getObservations(list);
  CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
  list.clear();
  
  // Clear Code 2
  p5 = new Observation(*m_dataItem1, 2, time, normal2);
  p5->unrefer();
  
  m_checkpoint->addObservation(p5);
  CPPUNIT_ASSERT(nullptr == p5->getPrev());
  
  // Check cleanup
  ObservationPtr *p7 = m_checkpoint->getEvents().at(std::string("1"));
  CPPUNIT_ASSERT_EQUAL(1, (int)(*p7)->refCount());
  
  CPPUNIT_ASSERT(p7);
  CPPUNIT_ASSERT(p5.getObject() != (*p7).getObject());
  CPPUNIT_ASSERT_EQUAL(std::string("CODE3"), (*p7)->getCode());
  CPPUNIT_ASSERT_EQUAL(std::string("CODE1"), (*p7)->getPrev()->getCode());
  CPPUNIT_ASSERT(nullptr == (*p7)->getPrev()->getPrev());
  
  m_checkpoint->getObservations(list);
  CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
  list.clear();
  
  // Clear all
  p6 = new Observation(*m_dataItem1, 2, time, normal);
  p6->unrefer();
  
  m_checkpoint->addObservation(p6);
  CPPUNIT_ASSERT(nullptr == p6->getPrev());
  
  m_checkpoint->getObservations(list);
  CPPUNIT_ASSERT_EQUAL(1, (int) list.size());
  list.clear();
}


void CheckpointTest::testLastConditionNormal()
{
  ObservationPtr p1, p2, p3;
  
  string time("NOW"),
  fault1("FAULT|CODE1|HIGH|Over..."),
  normal1("NORMAL|CODE1||");
  
  std::set<string> filter;
  filter.insert(m_dataItem1->getId());
  ObservationPtrArray list;
  
  
  p1 = new Observation(*m_dataItem1, 2, time, fault1);
  p1->unrefer();
  m_checkpoint->addObservation(p1);
  
  m_checkpoint->getObservations(list);
  CPPUNIT_ASSERT_EQUAL(1, (int) list.size());
  list.clear();
  
  p2 = new Observation(*m_dataItem1, 2, time, normal1);
  p2->unrefer();
  m_checkpoint->addObservation(p2);
  
  m_checkpoint->getObservations(list, &filter);
  CPPUNIT_ASSERT_EQUAL(1, (int) list.size());
  
  p3 = list[0];
  CPPUNIT_ASSERT_EQUAL(Observation::NORMAL, p3->getLevel());
  CPPUNIT_ASSERT_EQUAL(string(""), p3->getCode());
}
