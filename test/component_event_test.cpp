//
// Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the AMT nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
// BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
// AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
// RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
// (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
// WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
// LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
//
// LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
// PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
// OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
// CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
// WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
// THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
// SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
//
#include "component_event_test.hpp"
#include "data_item.hpp"
#include <list>


using namespace std;

namespace mtconnect {
  namespace test {
    // Registers the fixture into the 'registry'
    CPPUNIT_TEST_SUITE_REGISTRATION(ComponentEventTest);
    
#define TEST_VALUE(attributes, nativeUnits, expected, value) \
testValueHelper(attributes, nativeUnits, expected, value, CPPUNIT_SOURCELINE())
    
    
    void ComponentEventTest::setUp()
    {
      std::map<string, string> attributes1, attributes2;
      
      attributes1["id"] = "1";
      attributes1["name"] = "DataItemTest1";
      attributes1["type"] = "ALARM";
      attributes1["category"] = "EVENT";
      m_dataItem1 = make_unique<DataItem>(attributes1);
      
      attributes2["id"] = "3";
      attributes2["name"] = "DataItemTest2";
      attributes2["type"] = "POSITION";
      attributes2["nativeUnits"] = "MILLIMETER";
      attributes2["subType"] = "ACTUAL";
      attributes2["category"] = "SAMPLE";
      m_dataItem2 = make_unique<DataItem>(attributes2);
      
      string time("NOW"), value("CODE|NATIVE|CRITICAL|ACTIVE|DESCRIPTION");
      m_compEventA = new ComponentEvent(*m_dataItem1, 2, time, value);
      
      time = "LATER";
      value = "1.1231";
      m_compEventB = new ComponentEvent(*m_dataItem2, 4, time, value);
    }
    
    
    void ComponentEventTest::tearDown()
    {
      m_compEventA->unrefer();
      m_compEventB->unrefer();
      m_dataItem1.reset();
      m_dataItem2.reset();
    }
    
    
    void ComponentEventTest::testConstructors()
    {
      auto ce = new ComponentEvent(*m_compEventA);
      
      // Copy constructor allocates different objects, so it has different addresses
      CPPUNIT_ASSERT(m_compEventA != ce);
      
      // But the values should be the same
      CPPUNIT_ASSERT_EQUAL(m_compEventA->getDataItem(), ce->getDataItem());
      CPPUNIT_ASSERT_EQUAL(m_compEventA->getValue(), ce->getValue());
      
      ce->unrefer();
    }
    
    
    void ComponentEventTest::testGetAttributes()
    {
      const auto &attr_list1 = m_compEventA->getAttributes();
      map<string, string> attributes1;
      
      for (const auto &attr : attr_list1)
        attributes1[attr.first] = attr.second;
      
      CPPUNIT_ASSERT_EQUAL((string) "1", attributes1["dataItemId"]);
      CPPUNIT_ASSERT_EQUAL((string) "NOW", attributes1["timestamp"]);
      CPPUNIT_ASSERT(attributes1["subType"].empty());
      CPPUNIT_ASSERT_EQUAL((string) "DataItemTest1", attributes1["name"]);
      CPPUNIT_ASSERT_EQUAL((string) "2", attributes1["sequence"]);
      
      // Alarm data
      CPPUNIT_ASSERT_EQUAL((string) "CODE", attributes1["code"]);
      CPPUNIT_ASSERT_EQUAL((string) "NATIVE", attributes1["nativeCode"]);
      CPPUNIT_ASSERT_EQUAL((string) "CRITICAL", attributes1["severity"]);
      CPPUNIT_ASSERT_EQUAL((string) "ACTIVE", attributes1["state"]);
      
      const auto &attr_list2 = m_compEventB->getAttributes();
      map<string, string> attributes2;
      
      for (const auto &attr : attr_list2)
        attributes2[attr.first] = attr.second;
      
      CPPUNIT_ASSERT_EQUAL((string) "3", attributes2["dataItemId"]);
      CPPUNIT_ASSERT_EQUAL((string) "LATER", attributes2["timestamp"]);
      CPPUNIT_ASSERT_EQUAL((string) "ACTUAL", attributes2["subType"]);
      CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2", attributes2["name"]);
      CPPUNIT_ASSERT_EQUAL((string) "4", attributes2["sequence"]);
    }
    
    
    void ComponentEventTest::testGetters()
    {
      CPPUNIT_ASSERT_EQUAL(m_dataItem1.get(), m_compEventA->getDataItem());
      CPPUNIT_ASSERT_EQUAL(m_dataItem2.get(), m_compEventB->getDataItem());
      
      CPPUNIT_ASSERT_EQUAL((string) "DESCRIPTION", m_compEventA->getValue());
      CPPUNIT_ASSERT_EQUAL((string) "1.1231", m_compEventB->getValue());
    }
    
    
    void ComponentEventTest::testConvertValue()
    {
      std::map<string, string> attributes;
      attributes["id"] = "1";
      attributes["name"] = "DataItemTest1";
      attributes["type"] = "ACCELERATION";
      attributes["category"] = "SAMPLE";
      
      string time("NOW"), value("2.0");
      
      TEST_VALUE(attributes, "REVOLUTION/MINUTE", 2.0f, value);
      TEST_VALUE(attributes, "REVOLUTION/SECOND", 2.0f * 60.0f, value);
      TEST_VALUE(attributes, "GRAM/INCH", (2.0f / 1000.0f) / 25.4f, value);
      TEST_VALUE(attributes, "MILLIMETER/MINUTE^3",
                 (2.0f) / (60.0f * 60.0f * 60.0f), value);
      
      attributes["nativeScale"] = "0.5";
      TEST_VALUE(attributes, "MILLIMETER/MINUTE^3",
                 (2.0f) / (60.0f * 60.0f * 60.0f * 0.5f), value);
    }
    
    
    void ComponentEventTest::testConvertSimpleUnits()
    {
      std::map<string, string> attributes;
      attributes["id"] = "1";
      attributes["name"] = "DataItemTest";
      attributes["type"] = "ACCELERATION";
      attributes["category"] = "SAMPLE";
      
      string value("2.0");
      
      TEST_VALUE(attributes, "INCH", 2.0f * 25.4f, value);
      TEST_VALUE(attributes, "FOOT", 2.0f * 304.8f, value);
      TEST_VALUE(attributes, "CENTIMETER", 2.0f * 10.0f, value);
      TEST_VALUE(attributes, "DECIMETER", 2.0f * 100.0f, value);
      TEST_VALUE(attributes, "METER", 2.0f * 1000.0f, value);
      TEST_VALUE(attributes, "FAHRENHEIT",
                 (2.0f - 32.0f) * (5.0f / 9.0f), value);
      TEST_VALUE(attributes, "POUND", 2.0f * 0.45359237f, value);
      TEST_VALUE(attributes, "GRAM", 2.0f / 1000.0f, value);
      TEST_VALUE(attributes, "RADIAN", 2.0f * 57.2957795f, value);
      TEST_VALUE(attributes, "MINUTE", 2.0f * 60.0f, value);
      TEST_VALUE(attributes, "HOUR", 2.0f * 3600.0f, value);
      TEST_VALUE(attributes, "MILLIMETER", 2.0f, value);
      TEST_VALUE(attributes, "PERCENT", 2.0f, value);
    }
    
    
    void ComponentEventTest::testValueHelper(
                                             std::map<string, string> &attributes,
                                             const string &nativeUnits,
                                             float expected,
                                             const string &value,
                                             CPPUNIT_NS::SourceLine sourceLine
                                             )
    {
      string time("NOW");
      
      attributes["nativeUnits"] = nativeUnits;
      DataItem dataItem(attributes);
      
      ComponentEventPtr event(new ComponentEvent(dataItem, 123, time, value), true);
      
      
      CPPUNIT_NS::OStringStream message;
      double diff = abs(expected - atof(event->getValue().c_str()));
      message << "Unit conversion for " << nativeUnits << " failed, expected: " << expected <<
      " and actual "
      << event->getValue() << " differ (" << diff << ") by more than 0.001";
      CPPUNIT_NS::Asserter::failIf(diff > 0.001,
                                   message.str(),
                                   sourceLine);
    }
    
    
    void ComponentEventTest::testRefCounts()
    {
      string time("NOW"), value("111");
      auto event = new ComponentEvent(*m_dataItem1, 123, time, value);
      
      CPPUNIT_ASSERT(event->refCount() == 1);
      
      event->referTo();
      CPPUNIT_ASSERT(event->refCount() == 2);
      
      event->referTo();
      CPPUNIT_ASSERT(event->refCount() == 3);
      
      event->unrefer();
      CPPUNIT_ASSERT(event->refCount() == 2);
      
      event->unrefer();
      CPPUNIT_ASSERT(event->refCount() == 1);
      
      {
        ComponentEventPtr prt(event);
        CPPUNIT_ASSERT(event->refCount() == 2);
      }
      
      CPPUNIT_ASSERT(event->refCount() == 1);
      event->referTo();
      CPPUNIT_ASSERT(event->refCount() == 2);
      {
        ComponentEventPtr prt(event, true);
        CPPUNIT_ASSERT(event->refCount() == 2);
      }
      CPPUNIT_ASSERT(event->refCount() == 1);
      
      {
        ComponentEventPtr prt;
        prt = event;
        CPPUNIT_ASSERT(prt->refCount() == 2);
      }
      CPPUNIT_ASSERT(event->refCount() == 1);
    }
    
    
    void ComponentEventTest::testStlLists()
    {
      std::vector<ComponentEventPtr> vector;
      
      string time("NOW"), value("111");
      auto event = new ComponentEvent(*m_dataItem1, 123, time, value);
      
      CPPUNIT_ASSERT_EQUAL(1, (int) event->refCount());
      vector.push_back(event);
      CPPUNIT_ASSERT_EQUAL(2, (int) event->refCount());
      
      std::list<ComponentEventPtr> list;
      list.push_back(event);
      CPPUNIT_ASSERT_EQUAL(3, (int) event->refCount());
      
    }
    
    
    void ComponentEventTest::testEventChaining()
    {
      string time("NOW"), value("111");
      ComponentEventPtr event1(new ComponentEvent(*m_dataItem1, 123, time, value), true);
      ComponentEventPtr event2(new ComponentEvent(*m_dataItem1, 123, time, value), true);
      ComponentEventPtr event3(new ComponentEvent(*m_dataItem1, 123, time, value), true);
      
      CPPUNIT_ASSERT_EQUAL(event1.getObject(), event1->getFirst());
      
      event1->appendTo(event2);
      CPPUNIT_ASSERT_EQUAL(event1->getFirst(), event2.getObject());
      
      event2->appendTo(event3);
      CPPUNIT_ASSERT_EQUAL(event1->getFirst(), event3.getObject());
      
      CPPUNIT_ASSERT_EQUAL(1, (int) event1->refCount());
      CPPUNIT_ASSERT_EQUAL(2, (int) event2->refCount());
      CPPUNIT_ASSERT_EQUAL(2, (int) event3->refCount());
      
      std::list<ComponentEventPtr> list;
      event1->getList(list);
      CPPUNIT_ASSERT_EQUAL(3, (int) list.size());
      CPPUNIT_ASSERT_EQUAL(list.front().getObject(), event3.getObject());
      CPPUNIT_ASSERT_EQUAL(list.back().getObject(), event1.getObject());
      
      std::list<ComponentEventPtr> list2;
      event2->getList(list2);
      CPPUNIT_ASSERT_EQUAL(2, (int) list2.size());
      CPPUNIT_ASSERT_EQUAL(list2.front().getObject(), event3.getObject());
      CPPUNIT_ASSERT_EQUAL(list2.back().getObject(), event2.getObject());
    }
    
    
    void ComponentEventTest::testCondition()
    {
      string time("NOW");
      std::map<string, string> attributes1;
      attributes1["id"] = "1";
      attributes1["name"] = "DataItemTest1";
      attributes1["type"] = "TEMPERATURE";
      attributes1["category"] = "CONDITION";
      auto d = make_unique<DataItem>(attributes1);
      
      ComponentEventPtr event1(new ComponentEvent(*d, 123, time, (string) "FAULT|4321|1|HIGH|Overtemp"), true);
      
      CPPUNIT_ASSERT_EQUAL(ComponentEvent::FAULT, event1->getLevel());
      CPPUNIT_ASSERT_EQUAL((string) "Overtemp", event1->getValue());
      
      const auto &attr_list1 = event1->getAttributes();
      map<string, string> attrs1;
      
      for (const auto &attr : attr_list1)
        attrs1[attr.first] = attr.second;
      
      CPPUNIT_ASSERT_EQUAL((string) "TEMPERATURE", attrs1["type"]);
      CPPUNIT_ASSERT_EQUAL((string) "123", attrs1["sequence"]);
      CPPUNIT_ASSERT_EQUAL((string) "4321", attrs1["nativeCode"]);
      CPPUNIT_ASSERT_EQUAL((string) "HIGH", attrs1["qualifier"]);
      CPPUNIT_ASSERT_EQUAL((string) "1", attrs1["nativeSeverity"]);
      CPPUNIT_ASSERT_EQUAL((string) "Fault", event1->getLevelString());
      
      ComponentEventPtr event2(new ComponentEvent(*d, 123, time, (string) "fault|4322|2|LOW|Overtemp"), true);
      
      CPPUNIT_ASSERT_EQUAL(ComponentEvent::FAULT, event2->getLevel());
      CPPUNIT_ASSERT_EQUAL((string) "Overtemp", event2->getValue());
      
      const auto &attr_list2 = event2->getAttributes();
      map<string, string> attrs2;
      
      for (const auto &attr : attr_list2)
        attrs2[attr.first] = attr.second;
      
      CPPUNIT_ASSERT_EQUAL((string) "TEMPERATURE", attrs2["type"]);
      CPPUNIT_ASSERT_EQUAL((string) "123", attrs2["sequence"]);
      CPPUNIT_ASSERT_EQUAL((string) "4322", attrs2["nativeCode"]);
      CPPUNIT_ASSERT_EQUAL((string) "LOW", attrs2["qualifier"]);
      CPPUNIT_ASSERT_EQUAL((string) "2", attrs2["nativeSeverity"]);
      CPPUNIT_ASSERT_EQUAL((string) "Fault", event2->getLevelString());
      
      d.reset();
    }
    
    
    void ComponentEventTest::testTimeSeries()
    {
      string time("NOW");
      std::map<string, string> attributes1;
      attributes1["id"] = "1";
      attributes1["name"] = "test";
      attributes1["type"] = "TEMPERATURE";
      attributes1["category"] = "SAMPLE";
      attributes1["representation"] = "TIME_SERIES";
      auto d = make_unique<DataItem>(attributes1);
      
      CPPUNIT_ASSERT(d->isTimeSeries());
      
      ComponentEventPtr event1(new ComponentEvent(*d, 123, time, (string) "6||1 2 3 4 5 6 "), true);
      const auto &attr_list1 = event1->getAttributes();
      map<string, string> attrs1;
      
      for (const auto& attr : attr_list1)
        attrs1[attr.first] = attr.second;
      
      CPPUNIT_ASSERT(event1->isTimeSeries());
      
      CPPUNIT_ASSERT_EQUAL(6, event1->getSampleCount());
      auto values = event1->getTimeSeries();
      
      for (auto i = 0; i < event1->getSampleCount(); i++)
      {
        CPPUNIT_ASSERT_EQUAL((float)(i + 1), values[i]);
      }
      
      CPPUNIT_ASSERT_EQUAL((string) "", event1->getValue());
      CPPUNIT_ASSERT_EQUAL(0, (int) attrs1.count("sampleRate"));
      
      
      ComponentEventPtr event2(new ComponentEvent(*d, 123, time,
                                                  (string) "7|42000|10 20 30 40 50 60 70 "), true);
      const auto &attr_list2 = event2->getAttributes();
      map<string, string> attrs2;
      
      for (const auto& attr : attr_list2)
        attrs2[attr.first] = attr.second;
      
      CPPUNIT_ASSERT(event2->isTimeSeries());
      
      CPPUNIT_ASSERT_EQUAL(7, event2->getSampleCount());
      CPPUNIT_ASSERT_EQUAL((string) "", event2->getValue());
      CPPUNIT_ASSERT_EQUAL((string) "42000", attrs2["sampleRate"]);
      values = event2->getTimeSeries();
      
      for (auto i = 0; i < event1->getSampleCount(); i++)
      {
        CPPUNIT_ASSERT_EQUAL((float)((i + 1) * 10), values[i]);
      }
      
      d.reset();
    }
    
    
    void ComponentEventTest::testDuration()
    {
      string time("2011-02-18T15:52:41Z@200.1232");
      std::map<string, string> attributes1;
      attributes1["id"] = "1";
      attributes1["name"] = "test";
      attributes1["type"] = "TEMPERATURE";
      attributes1["category"] = "SAMPLE";
      attributes1["statistic"] = "AVERAGE";
      auto d = make_unique<DataItem>(attributes1);
      
      ComponentEventPtr event1(new ComponentEvent(*d, 123, time, (string) "11.0"), true);
      const auto &attr_list = event1->getAttributes();
      map<string, string> attrs1;
      
      for (const auto &attr : attr_list)
        attrs1[attr.first] = attr.second;
      
      CPPUNIT_ASSERT_EQUAL((string) "AVERAGE", attrs1["statistic"]);
      CPPUNIT_ASSERT_EQUAL((string) "2011-02-18T15:52:41Z", attrs1["timestamp"]);
      CPPUNIT_ASSERT_EQUAL((string) "200.1232", attrs1["duration"]);
      
      d.reset();
    }
    
    
    void ComponentEventTest::testAssetChanged()
    {
      string time("2011-02-18T15:52:41Z@200.1232");
      std::map<string, string> attributes1;
      attributes1["id"] = "1";
      attributes1["name"] = "ac";
      attributes1["type"] = "ASSET_CHANGED";
      attributes1["category"] = "EVENT";
      auto d = make_unique<DataItem>(attributes1);
      
      CPPUNIT_ASSERT(d->isAssetChanged());
      
      ComponentEventPtr event1(new ComponentEvent(*d, 123, time, (string) "CuttingTool|123"), true);
      const auto &attr_list = event1->getAttributes();
      map<string, string> attrs1;
      
      for (const auto &attr : attr_list)
        attrs1[attr.first] = attr.second;
      
      CPPUNIT_ASSERT_EQUAL((string) "CuttingTool", attrs1["assetType"]);
      CPPUNIT_ASSERT_EQUAL((string) "123", event1->getValue());
      
      d.reset();
    }
  }
}
