/*
 * Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the AMT nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
 * BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
 * AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
 * RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
 * (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
 * WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
 * LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
 * MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 
 
 * LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
 * PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
 * OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
 * CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
 * WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
 * THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
 * SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
 */

/*
 * Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the AMT nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
 * BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
 * AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
 * RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
 * (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
 * WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
 * LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
 * MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 
 
 * LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
 * PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
 * OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
 * CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
 * WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
 * THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
 * SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include "checkpoint_test.hpp"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(CheckpointTest);

using namespace std;

void CheckpointTest::setUp()
{
  // Create an agent with only 16 slots and 8 data items.
  mAgent = new Agent("../samples/min_config.xml", 4);
  mAgentId = intToString(getCurrentTimeInSec());
  mAdapter = NULL;
  mCheckpoint = new Checkpoint();
  
  std::map<string, string> attributes1, attributes2;
  
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "LOAD";
  attributes1["category"] = "CONDITION";
  mDataItem1 = new DataItem(attributes1);
  
  attributes2["id"] = "3";
  attributes2["name"] = "DataItemTest2";
  attributes2["type"] = "POSITION";
  attributes2["nativeUnits"] = "MILLIMETER";
  attributes2["subType"] = "ACTUAL";
  attributes2["category"] = "SAMPLE";
  mDataItem2 = new DataItem(attributes2);
}

void CheckpointTest::tearDown()
{
  delete mAgent;
  delete mCheckpoint;
}

void CheckpointTest::testAddComponentEvents()
{
  ComponentEventPtr p1, p2, p3, p4, p5, p6;
  string time("NOW"), value("123"), warning("WARNING|CODE|HIGH|Over..."),
    normal("NORMAL|CODE|HIGH|Over..."),
    unavailable("UNAVAILABLE|CODE|HIGH|Over...");
  
  p1 = new ComponentEvent(*mDataItem1, 2, time, warning);
  p1->unrefer();
  
  CPPUNIT_ASSERT_EQUAL(1, (int) p1->refCount());
  mCheckpoint->addComponentEvent(p1);
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  
  p2 = new ComponentEvent(*mDataItem1, 2, time, warning);
  p2->unrefer();

  mCheckpoint->addComponentEvent(p2);
  CPPUNIT_ASSERT_EQUAL(p1.getObject(), p2->getPrev());

  p3 = new ComponentEvent(*mDataItem1, 2, time, normal);
  p3->unrefer();

  mCheckpoint->addComponentEvent(p3);
  CPPUNIT_ASSERT_EQUAL((void*) 0, (void*) p3->getPrev());
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  CPPUNIT_ASSERT_EQUAL(1, (int) p2->refCount());

  p4 = new ComponentEvent(*mDataItem1, 2, time, warning);
  p4->unrefer();

  mCheckpoint->addComponentEvent(p4);
  CPPUNIT_ASSERT_EQUAL((void*) 0, (void*) p4->getPrev());
  CPPUNIT_ASSERT_EQUAL(1, (int) p3->refCount());

  // Test non condition
  p3 = new ComponentEvent(*mDataItem2, 2, time, value);
  p3->unrefer();
  mCheckpoint->addComponentEvent(p3);
  CPPUNIT_ASSERT_EQUAL(2, (int) p3->refCount());
  
  p4 = new ComponentEvent(*mDataItem2, 2, time, value);
  p4->unrefer();

  mCheckpoint->addComponentEvent(p4);
  CPPUNIT_ASSERT_EQUAL(2, (int) p4->refCount());
  
  CPPUNIT_ASSERT_EQUAL((void*) 0, (void*) p4->getPrev());
  CPPUNIT_ASSERT_EQUAL(1, (int) p3->refCount());
}

void CheckpointTest::testCopy()
{
  ComponentEventPtr p1, p2, p3, p4, p5, p6;
  string time("NOW"), value("123"), warning("WARNING|CODE|HIGH|Over..."),
    normal("NORMAL|CODE|HIGH|Over...");
  
  p1 = new ComponentEvent(*mDataItem1, 2, time, warning);
  p1->unrefer();
  mCheckpoint->addComponentEvent(p1);
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  
  p2 = new ComponentEvent(*mDataItem1, 2, time, warning);
  p2->unrefer();
  mCheckpoint->addComponentEvent(p2);
  CPPUNIT_ASSERT_EQUAL(p1.getObject(), p2->getPrev());
  CPPUNIT_ASSERT_EQUAL(2, (int) p2->refCount());

  Checkpoint *copy = new Checkpoint(*mCheckpoint);
  CPPUNIT_ASSERT_EQUAL(2, (int) p1->refCount());
  CPPUNIT_ASSERT_EQUAL(3, (int) p2->refCount());
  delete copy;
  CPPUNIT_ASSERT_EQUAL(2, (int) p2->refCount());
}

void CheckpointTest::testGetComponentEvents()
{
  ComponentEventPtr p;
  string time("NOW"), value("123"), warning("WARNING|CODE|HIGH|Over..."),
    normal("NORMAL|CODE|HIGH|Over...");
  std::set<string> filter;
  filter.insert(mDataItem1->getId());
  
  p = new ComponentEvent(*mDataItem1, 2, time, warning);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();
  
  p = new ComponentEvent(*mDataItem1, 2, time, warning);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();

  filter.insert(mDataItem2->getId());
  p = new ComponentEvent(*mDataItem2, 2, time, value);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();
  
  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  DataItem *d1 = new DataItem(attributes);
  filter.insert(d1->getId());
  
  p = new ComponentEvent(*d1, 2, time, value);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();

  vector<ComponentEventPtr> list;
  mCheckpoint->getComponentEvents(list, &filter);

  CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
  
  std::set<string> filter2;
  filter2.insert(mDataItem1->getId());

  vector<ComponentEventPtr> list2;
  mCheckpoint->getComponentEvents(list2, &filter2);

  CPPUNIT_ASSERT_EQUAL(2, (int) list2.size());
  
  delete d1;
}

void CheckpointTest::testFilter()
{
  ComponentEventPtr p;
  string time("NOW"), value("123"), warning("WARNING|CODE|HIGH|Over..."),
    normal("NORMAL|CODE|HIGH|Over...");
  std::set<string> filter;
  filter.insert(mDataItem1->getId());
  
  p = new ComponentEvent(*mDataItem1, 2, time, warning);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();
  
  p = new ComponentEvent(*mDataItem1, 2, time, warning);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();

  p = new ComponentEvent(*mDataItem2, 2, time, value);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();
  
  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  DataItem *d1 = new DataItem(attributes);
  
  p = new ComponentEvent(*d1, 2, time, value);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();

  vector<ComponentEventPtr> list;
  mCheckpoint->getComponentEvents(list);

  CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
  list.clear();
  
  mCheckpoint->filter(filter);
  mCheckpoint->getComponentEvents(list);

  CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
}

void CheckpointTest::testCopyAndFilter()
{
  ComponentEventPtr p;
  string time("NOW"), value("123"), warning("WARNING|CODE|HIGH|Over..."),
    normal("NORMAL|CODE|HIGH|Over...");
  std::set<string> filter;
  filter.insert(mDataItem1->getId());
  
  p = new ComponentEvent(*mDataItem1, 2, time, warning);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();
  
  p = new ComponentEvent(*mDataItem1, 2, time, warning);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();

  p = new ComponentEvent(*mDataItem2, 2, time, value);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();
  
  std::map<string, string> attributes;
  attributes["id"] = "4";
  attributes["name"] = "DataItemTest2";
  attributes["type"] = "POSITION";
  attributes["nativeUnits"] = "MILLIMETER";
  attributes["subType"] = "ACTUAL";
  attributes["category"] = "SAMPLE";
  DataItem *d1 = new DataItem(attributes);
  
  p = new ComponentEvent(*d1, 2, time, value);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();

  vector<ComponentEventPtr> list;
  mCheckpoint->getComponentEvents(list);

  CPPUNIT_ASSERT_EQUAL(4, (int) list.size());
  list.clear();

  Checkpoint check(*mCheckpoint, &filter);
  check.getComponentEvents(list);
  
  CPPUNIT_ASSERT_EQUAL(2, (int) list.size());
  list.clear();

  p = new ComponentEvent(*mDataItem1, 2, time, warning);
  check.addComponentEvent(p);
  p->unrefer();

  check.getComponentEvents(list);
  
  CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
  list.clear();

  p = new ComponentEvent(*d1, 2, time, value);
  mCheckpoint->addComponentEvent(p);
  p->unrefer();

  check.getComponentEvents(list);
  CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
}

