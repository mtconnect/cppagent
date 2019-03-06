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

#include "checkpoint_test.hpp"


using namespace std;

namespace mtconnect {
  namespace test {
    // Registers the fixture into the 'registry'
    CPPUNIT_TEST_SUITE_REGISTRATION(CheckpointTest);
    
    void CheckpointTest::setUp()
    {
      // Create an agent with only 16 slots and 8 data items.
      m_agent = nullptr;
      m_checkpoint = nullptr;
      m_agent = make_unique<Agent>("../samples/min_config.xml", 4, 4, "1.5");
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
    
    
    void CheckpointTest::testAddComponentEvents()
    {
      ComponentEventPtr p1, p2, p3, p4, p5, p6;
      string time("NOW"), value("123"),
      warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."),
      normal("NORMAL|||"),
      unavailable("UNAVAILABLE|||");
      
      p1 = new ComponentEvent(*m_dataItem1, 2, time, warning1);
      p1->unrefer();
      
      CPPUNIT_ASSERT_EQUAL(1, (int) p1->refCount());
      m_checkpoint->addComponentEvent(p1);
      CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
      
      p2 = new ComponentEvent(*m_dataItem1, 2, time, warning2);
      p2->unrefer();
      
      m_checkpoint->addComponentEvent(p2);
      CPPUNIT_ASSERT_EQUAL(p1.getObject(), p2->getPrev());
      
      p3 = new ComponentEvent(*m_dataItem1, 2, time, normal);
      p3->unrefer();
      
      m_checkpoint->addComponentEvent(p3);
      CPPUNIT_ASSERT_EQUAL((void *) 0, (void *) p3->getPrev());
      CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
      CPPUNIT_ASSERT_EQUAL(1, (int) p2->refCount());
      
      p4 = new ComponentEvent(*m_dataItem1, 2, time, warning1);
      p4->unrefer();
      
      m_checkpoint->addComponentEvent(p4);
      CPPUNIT_ASSERT_EQUAL((void *) 0, (void *) p4->getPrev());
      CPPUNIT_ASSERT_EQUAL(1, (int) p3->refCount());
      
      // Test non condition
      p3 = new ComponentEvent(*m_dataItem2, 2, time, value);
      p3->unrefer();
      m_checkpoint->addComponentEvent(p3);
      CPPUNIT_ASSERT_EQUAL(2, (int) p3->refCount());
      
      p4 = new ComponentEvent(*m_dataItem2, 2, time, value);
      p4->unrefer();
      
      m_checkpoint->addComponentEvent(p4);
      CPPUNIT_ASSERT_EQUAL(2, (int) p4->refCount());
      
      CPPUNIT_ASSERT_EQUAL((void *) 0, (void *) p4->getPrev());
      CPPUNIT_ASSERT_EQUAL(1, (int) p3->refCount());
    }
    
    
    void CheckpointTest::testCopy()
    {
      ComponentEventPtr p1, p2, p3, p4, p5, p6;
      string time("NOW"), value("123"),
      warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."),
      normal("NORMAL|||");
      
      p1 = new ComponentEvent(*m_dataItem1, 2, time, warning1);
      p1->unrefer();
      m_checkpoint->addComponentEvent(p1);
      CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
      
      p2 = new ComponentEvent(*m_dataItem1, 2, time, warning2);
      p2->unrefer();
      m_checkpoint->addComponentEvent(p2);
      CPPUNIT_ASSERT_EQUAL(p1.getObject(), p2->getPrev());
      CPPUNIT_ASSERT_EQUAL(2, (int) p2->refCount());
      
      auto copy = new Checkpoint(*m_checkpoint);
      CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
      CPPUNIT_ASSERT_EQUAL(3, (int) p2->refCount());
      delete copy; copy = nullptr;
      CPPUNIT_ASSERT_EQUAL(2, (int) p2->refCount());
    }
    
    
    void CheckpointTest::testGetComponentEvents()
    {
      ComponentEventPtr p;
      string time("NOW"), value("123"),
      warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."),
      normal("NORMAL|||");
      std::set<string> filter;
      filter.insert(m_dataItem1->getId());
      
      p = new ComponentEvent(*m_dataItem1, 2, time, warning1);
      m_checkpoint->addComponentEvent(p);
      p->unrefer();
      
      p = new ComponentEvent(*m_dataItem1, 2, time, warning2);
      m_checkpoint->addComponentEvent(p);
      p->unrefer();
      
      filter.insert(m_dataItem2->getId());
      p = new ComponentEvent(*m_dataItem2, 2, time, value);
      m_checkpoint->addComponentEvent(p);
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
      
      p = new ComponentEvent(*d1, 2, time, value);
      m_checkpoint->addComponentEvent(p);
      p->unrefer();
      
      ComponentEventPtrArray list;
      m_checkpoint->getComponentEvents(list, &filter);
      
      CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
      
      std::set<string> filter2;
      filter2.insert(m_dataItem1->getId());
      
      ComponentEventPtrArray list2;
      m_checkpoint->getComponentEvents(list2, &filter2);
      
      CPPUNIT_ASSERT_EQUAL(2, (int) list2.size());
      
      d1.reset();
    }
    
    
    void CheckpointTest::testFilter()
    {
      ComponentEventPtr p1, p2, p3, p4;
      string time("NOW"), value("123"),
      warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."),
      normal("NORMAL|||");
      std::set<string> filter;
      filter.insert(m_dataItem1->getId());
      
      p1 = new ComponentEvent(*m_dataItem1, 2, time, warning1);
      m_checkpoint->addComponentEvent(p1);
      p1->unrefer();
      
      p2 = new ComponentEvent(*m_dataItem1, 2, time, warning2);
      m_checkpoint->addComponentEvent(p2);
      p2->unrefer();
      
      p3 = new ComponentEvent(*m_dataItem2, 2, time, value);
      m_checkpoint->addComponentEvent(p3);
      p3->unrefer();
      
      std::map<string, string> attributes;
      attributes["id"] = "4";
      attributes["name"] = "DataItemTest2";
      attributes["type"] = "POSITION";
      attributes["nativeUnits"] = "MILLIMETER";
      attributes["subType"] = "ACTUAL";
      attributes["category"] = "SAMPLE";
      auto d1 = make_unique<DataItem>(attributes);
      
      p4 = new ComponentEvent(*d1, 2, time, value);
      m_checkpoint->addComponentEvent(p4);
      p4->unrefer();
      
      ComponentEventPtrArray list;
      m_checkpoint->getComponentEvents(list);
      
      CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
      list.clear();
      
      m_checkpoint->filter(filter);
      m_checkpoint->getComponentEvents(list);
      
      CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
      d1.reset();
    }
    
    
    void CheckpointTest::testCopyAndFilter()
    {
      ComponentEventPtr p;
      string time("NOW"), value("123"),
      warning1("WARNING|CODE1|HIGH|Over..."),
      warning2("WARNING|CODE2|HIGH|Over..."),
      warning3("WARNING|CODE3|HIGH|Over..."),
      normal("NORMAL|||");
      std::set<string> filter;
      filter.insert(m_dataItem1->getId());
      
      p = new ComponentEvent(*m_dataItem1, 2, time, warning1);
      m_checkpoint->addComponentEvent(p);
      p->unrefer();
      
      p = new ComponentEvent(*m_dataItem1, 2, time, warning2);
      m_checkpoint->addComponentEvent(p);
      p->unrefer();
      
      p = new ComponentEvent(*m_dataItem2, 2, time, value);
      m_checkpoint->addComponentEvent(p);
      p->unrefer();
      
      std::map<string, string> attributes;
      attributes["id"] = "4";
      attributes["name"] = "DataItemTest2";
      attributes["type"] = "POSITION";
      attributes["nativeUnits"] = "MILLIMETER";
      attributes["subType"] = "ACTUAL";
      attributes["category"] = "SAMPLE";
      auto d1 = make_unique<DataItem>(attributes);
      
      p = new ComponentEvent(*d1, 2, time, value);
      m_checkpoint->addComponentEvent(p);
      p->unrefer();
      
      ComponentEventPtrArray list;
      m_checkpoint->getComponentEvents(list);
      
      CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
      list.clear();
      
      Checkpoint check(*m_checkpoint, &filter);
      check.getComponentEvents(list);
      
      CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
      list.clear();
      
      p = new ComponentEvent(*m_dataItem1, 2, time, warning3);
      check.addComponentEvent(p);
      p->unrefer();
      
      check.getComponentEvents(list);
      
      CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
      list.clear();
      
      p = new ComponentEvent(*d1, 2, time, value);
      m_checkpoint->addComponentEvent(p);
      p->unrefer();
      
      check.getComponentEvents(list);
      CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
      
      d1.reset();
    }
    
    
    void CheckpointTest::testConditionChaining()
    {
      ComponentEventPtr p1, p2, p3, p4, p5, p6;
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
      ComponentEventPtrArray list;
      
      p1 = new ComponentEvent(*m_dataItem1, 2, time, warning1);
      p1->unrefer();
      m_checkpoint->addComponentEvent(p1);
      
      m_checkpoint->getComponentEvents(list);
      CPPUNIT_ASSERT_EQUAL(1, (int) list.size());
      list.clear();
      
      p2 = new ComponentEvent(*m_dataItem1, 2, time, warning2);
      p2->unrefer();
      
      m_checkpoint->addComponentEvent(p2);
      CPPUNIT_ASSERT_EQUAL(p1.getObject(), p2->getPrev());
      
      m_checkpoint->getComponentEvents(list);
      CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
      list.clear();
      
      p3 = new ComponentEvent(*m_dataItem1, 2, time, warning3);
      p3->unrefer();
      
      m_checkpoint->addComponentEvent(p3);
      CPPUNIT_ASSERT_EQUAL(p2.getObject(), p3->getPrev());
      CPPUNIT_ASSERT_EQUAL(p1.getObject(), p2->getPrev());
      CPPUNIT_ASSERT_EQUAL((ComponentEvent *) 0, p1->getPrev());
      
      m_checkpoint->getComponentEvents(list);
      CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
      list.clear();
      
      // Replace Warning on CODE 2 with a fault
      p4 = new ComponentEvent(*m_dataItem1, 2, time, fault2);
      p4->unrefer();
      
      m_checkpoint->addComponentEvent(p4);
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
      CPPUNIT_ASSERT_EQUAL((ComponentEvent *) 0, p4->getPrev()->getPrev()->getPrev());
      
      m_checkpoint->getComponentEvents(list);
      CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
      list.clear();
      
      // Clear Code 2
      p5 = new ComponentEvent(*m_dataItem1, 2, time, normal2);
      p5->unrefer();
      
      m_checkpoint->addComponentEvent(p5);
      CPPUNIT_ASSERT_EQUAL((ComponentEvent *) 0, p5->getPrev());
      
      // Check cleanup
      ComponentEventPtr *p7 = m_checkpoint->getEvents().at(std::string("1"));
      CPPUNIT_ASSERT_EQUAL(1, (int)(*p7)->refCount());
      
      CPPUNIT_ASSERT(p7);
      CPPUNIT_ASSERT(p5.getObject() != (*p7).getObject());
      CPPUNIT_ASSERT_EQUAL(std::string("CODE3"), (*p7)->getCode());
      CPPUNIT_ASSERT_EQUAL(std::string("CODE1"), (*p7)->getPrev()->getCode());
      CPPUNIT_ASSERT_EQUAL((ComponentEvent *) 0, (*p7)->getPrev()->getPrev());
      
      m_checkpoint->getComponentEvents(list);
      CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
      list.clear();
      
      // Clear all
      p6 = new ComponentEvent(*m_dataItem1, 2, time, normal);
      p6->unrefer();
      
      m_checkpoint->addComponentEvent(p6);
      CPPUNIT_ASSERT_EQUAL((ComponentEvent *) 0, p6->getPrev());
      
      m_checkpoint->getComponentEvents(list);
      CPPUNIT_ASSERT_EQUAL(1, (int) list.size());
      list.clear();
    }
    
    
    void CheckpointTest::testLastConditionNormal()
    {
      ComponentEventPtr p1, p2, p3;
      
      string time("NOW"),
      fault1("FAULT|CODE1|HIGH|Over..."),
      normal1("NORMAL|CODE1||");
      
      std::set<string> filter;
      filter.insert(m_dataItem1->getId());
      ComponentEventPtrArray list;
      
      
      p1 = new ComponentEvent(*m_dataItem1, 2, time, fault1);
      p1->unrefer();
      m_checkpoint->addComponentEvent(p1);
      
      m_checkpoint->getComponentEvents(list);
      CPPUNIT_ASSERT_EQUAL(1, (int) list.size());
      list.clear();
      
      p2 = new ComponentEvent(*m_dataItem1, 2, time, normal1);
      p2->unrefer();
      m_checkpoint->addComponentEvent(p2);
      
      m_checkpoint->getComponentEvents(list, &filter);
      CPPUNIT_ASSERT_EQUAL(1, (int) list.size());
      
      p3 = list[0];
      CPPUNIT_ASSERT_EQUAL(ComponentEvent::NORMAL, p3->getLevel());
      CPPUNIT_ASSERT_EQUAL(string(""), p3->getCode());
    }
  }
}
