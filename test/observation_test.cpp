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

#include "gtest/gtest.h"

#include "data_item.hpp"
#include "observation.hpp"
#include "test_globals.hpp"

#include <list>

using namespace std;
using namespace mtconnect;

class ObservationTest : public testing::Test {
 protected:
  void SetUp() override {
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
    m_compEventA = new Observation(*m_dataItem1, 2, time, value);

    time = "LATER";
    value = "1.1231";
    m_compEventB = new Observation(*m_dataItem2, 4, time, value);
  }

  void TearDown() override {
    m_compEventA->unrefer();
    m_compEventB->unrefer();
    m_dataItem1.reset();
    m_dataItem2.reset();
  }

  Observation *m_compEventA;
  Observation *m_compEventB;
  std::unique_ptr<DataItem> m_dataItem1;
  std::unique_ptr<DataItem> m_dataItem2;

  // Helper to test values
  void testValueHelper(std::map<std::string, std::string> &attributes,
                       const std::string &nativeUnits, float expected, const std::string &value,
                       const char *file, int line);
};

#define TEST_VALUE(attributes, nativeUnits, expected, value) \
  testValueHelper(attributes, nativeUnits, expected, value, __FILE__, __LINE__)

TEST_F(ObservationTest, Constructors) {
  auto ce = new Observation(*m_compEventA);

  // Copy constructor allocates different objects, so it has different addresses
  ASSERT_TRUE(m_compEventA != ce);

  // But the values should be the same
  ASSERT_TRUE(m_compEventA->getDataItem() == ce->getDataItem());
  ASSERT_TRUE(m_compEventA->getValue() == ce->getValue());

  ce->unrefer();
}

TEST_F(ObservationTest, GetAttributes) {
  const auto &attr_list1 = m_compEventA->getAttributes();
  map<string, string> attributes1;

  for (const auto &attr : attr_list1) attributes1[attr.first] = attr.second;

  ASSERT_EQ((string) "1", attributes1["dataItemId"]);
  ASSERT_EQ((string) "NOW", attributes1["timestamp"]);
  ASSERT_TRUE(attributes1["subType"].empty());
  ASSERT_EQ((string) "DataItemTest1", attributes1["name"]);
  ASSERT_EQ((string) "2", attributes1["sequence"]);

  // Alarm data
  ASSERT_EQ((string) "CODE", attributes1["code"]);
  ASSERT_EQ((string) "NATIVE", attributes1["nativeCode"]);
  ASSERT_EQ((string) "CRITICAL", attributes1["severity"]);
  ASSERT_EQ((string) "ACTIVE", attributes1["state"]);

  const auto &attr_list2 = m_compEventB->getAttributes();
  map<string, string> attributes2;

  for (const auto &attr : attr_list2) attributes2[attr.first] = attr.second;

  ASSERT_EQ((string) "3", attributes2["dataItemId"]);
  ASSERT_EQ((string) "LATER", attributes2["timestamp"]);
  ASSERT_EQ((string) "ACTUAL", attributes2["subType"]);
  ASSERT_EQ((string) "DataItemTest2", attributes2["name"]);
  ASSERT_EQ((string) "4", attributes2["sequence"]);
}

TEST_F(ObservationTest, Getters) {
  ASSERT_TRUE(m_dataItem1.get() == m_compEventA->getDataItem());
  ASSERT_TRUE(m_dataItem2.get() == m_compEventB->getDataItem());

  ASSERT_EQ((string) "DESCRIPTION", m_compEventA->getValue());
  ASSERT_EQ((string) "1.1231", m_compEventB->getValue());
}

TEST_F(ObservationTest, ConvertValue) {
  std::map<string, string> attributes;
  attributes["id"] = "1";
  attributes["name"] = "DataItemTest1";
  attributes["type"] = "ACCELERATION";
  attributes["category"] = "SAMPLE";

  string time("NOW"), value("2.0");

  TEST_VALUE(attributes, "REVOLUTION/MINUTE", 2.0f, value);
  TEST_VALUE(attributes, "REVOLUTION/SECOND", 2.0f * 60.0f, value);
  TEST_VALUE(attributes, "GRAM/INCH", (2.0f / 1000.0f) / 25.4f, value);
  TEST_VALUE(attributes, "MILLIMETER/MINUTE^3", (2.0f) / (60.0f * 60.0f * 60.0f), value);

  attributes["nativeScale"] = "0.5";
  TEST_VALUE(attributes, "MILLIMETER/MINUTE^3", (2.0f) / (60.0f * 60.0f * 60.0f * 0.5f), value);
}

TEST_F(ObservationTest, ConvertSimpleUnits) {
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
  TEST_VALUE(attributes, "FAHRENHEIT", (2.0f - 32.0f) * (5.0f / 9.0f), value);
  TEST_VALUE(attributes, "POUND", 2.0f * 0.45359237f, value);
  TEST_VALUE(attributes, "GRAM", 2.0f / 1000.0f, value);
  TEST_VALUE(attributes, "RADIAN", 2.0f * 57.2957795f, value);
  TEST_VALUE(attributes, "MINUTE", 2.0f * 60.0f, value);
  TEST_VALUE(attributes, "HOUR", 2.0f * 3600.0f, value);
  TEST_VALUE(attributes, "MILLIMETER", 2.0f, value);
  TEST_VALUE(attributes, "PERCENT", 2.0f, value);
}

void ObservationTest::testValueHelper(std::map<string, string> &attributes,
                                      const string &nativeUnits, float expected,
                                      const string &value, const char *file, int line) {
  string time("NOW");

  attributes["nativeUnits"] = nativeUnits;
  DataItem dataItem(attributes);

  ObservationPtr event(new Observation(dataItem, 123, time, value), true);

  stringstream message;
  double diff = abs(expected - atof(event->getValue().c_str()));
  message << "Unit conversion for " << nativeUnits << " failed, expected: " << expected
          << " and actual " << event->getValue() << " differ (" << diff << ") by more than 0.001";

  failIf(diff > 0.001, message.str(), __FILE__, __LINE__);
}

TEST_F(ObservationTest, RefCounts) {
  string time("NOW"), value("111");
  auto event = new Observation(*m_dataItem1, 123, time, value);

  ASSERT_TRUE(event->refCount() == 1);

  event->referTo();
  ASSERT_TRUE(event->refCount() == 2);

  event->referTo();
  ASSERT_TRUE(event->refCount() == 3);

  event->unrefer();
  ASSERT_TRUE(event->refCount() == 2);

  event->unrefer();
  ASSERT_TRUE(event->refCount() == 1);

  {
    ObservationPtr prt(event);
    ASSERT_TRUE(event->refCount() == 2);
  }

  ASSERT_TRUE(event->refCount() == 1);
  event->referTo();
  ASSERT_TRUE(event->refCount() == 2);
  {
    ObservationPtr prt(event, true);
    ASSERT_TRUE(event->refCount() == 2);
  }
  ASSERT_TRUE(event->refCount() == 1);

  {
    ObservationPtr prt;
    prt = event;
    ASSERT_TRUE(prt->refCount() == 2);
  }
  ASSERT_TRUE(event->refCount() == 1);
}

TEST_F(ObservationTest, StlLists) {
  std::vector<ObservationPtr> vector;

  string time("NOW"), value("111");
  auto event = new Observation(*m_dataItem1, 123, time, value);

  ASSERT_EQ(1, (int)event->refCount());
  vector.push_back(event);
  ASSERT_EQ(2, (int)event->refCount());

  std::list<ObservationPtr> list;
  list.push_back(event);
  ASSERT_EQ(3, (int)event->refCount());
}

TEST_F(ObservationTest, EventChaining) {
  string time("NOW"), value("111");
  ObservationPtr event1(new Observation(*m_dataItem1, 123, time, value), true);
  ObservationPtr event2(new Observation(*m_dataItem1, 123, time, value), true);
  ObservationPtr event3(new Observation(*m_dataItem1, 123, time, value), true);

  ASSERT_TRUE(event1.getObject() == event1->getFirst());

  event1->appendTo(event2);
  ASSERT_TRUE(event1->getFirst() == event2.getObject());

  event2->appendTo(event3);
  ASSERT_TRUE(event1->getFirst() == event3.getObject());

  ASSERT_EQ(1, (int)event1->refCount());
  ASSERT_EQ(2, (int)event2->refCount());
  ASSERT_EQ(2, (int)event3->refCount());

  std::list<ObservationPtr> list;
  event1->getList(list);
  ASSERT_EQ(3, (int)list.size());
  ASSERT_TRUE(list.front().getObject() == event3.getObject());
  ASSERT_TRUE(list.back().getObject() == event1.getObject());

  std::list<ObservationPtr> list2;
  event2->getList(list2);
  ASSERT_EQ(2, (int)list2.size());
  ASSERT_TRUE(list2.front().getObject() == event3.getObject());
  ASSERT_TRUE(list2.back().getObject() == event2.getObject());
}

TEST_F(ObservationTest, Condition) {
  string time("NOW");
  std::map<string, string> attributes1;
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "TEMPERATURE";
  attributes1["category"] = "CONDITION";
  auto d = make_unique<DataItem>(attributes1);

  ObservationPtr event1(new Observation(*d, 123, time, (string) "FAULT|4321|1|HIGH|Overtemp"),
                        true);

  ASSERT_EQ(Observation::FAULT, event1->getLevel());
  ASSERT_EQ((string) "Overtemp", event1->getValue());

  const auto &attr_list1 = event1->getAttributes();
  map<string, string> attrs1;

  for (const auto &attr : attr_list1) attrs1[attr.first] = attr.second;

  ASSERT_EQ((string) "TEMPERATURE", attrs1["type"]);
  ASSERT_EQ((string) "123", attrs1["sequence"]);
  ASSERT_EQ((string) "4321", attrs1["nativeCode"]);
  ASSERT_EQ((string) "HIGH", attrs1["qualifier"]);
  ASSERT_EQ((string) "1", attrs1["nativeSeverity"]);
  ASSERT_EQ((string) "Fault", event1->getLevelString());

  ObservationPtr event2(new Observation(*d, 123, time, (string) "fault|4322|2|LOW|Overtemp"), true);

  ASSERT_EQ(Observation::FAULT, event2->getLevel());
  ASSERT_EQ((string) "Overtemp", event2->getValue());

  const auto &attr_list2 = event2->getAttributes();
  map<string, string> attrs2;

  for (const auto &attr : attr_list2) attrs2[attr.first] = attr.second;

  ASSERT_EQ((string) "TEMPERATURE", attrs2["type"]);
  ASSERT_EQ((string) "123", attrs2["sequence"]);
  ASSERT_EQ((string) "4322", attrs2["nativeCode"]);
  ASSERT_EQ((string) "LOW", attrs2["qualifier"]);
  ASSERT_EQ((string) "2", attrs2["nativeSeverity"]);
  ASSERT_EQ((string) "Fault", event2->getLevelString());

  d.reset();
}

TEST_F(ObservationTest, TimeSeries) {
  string time("NOW");
  std::map<string, string> attributes1;
  attributes1["id"] = "1";
  attributes1["name"] = "test";
  attributes1["type"] = "TEMPERATURE";
  attributes1["category"] = "SAMPLE";
  attributes1["representation"] = "TIME_SERIES";
  auto d = make_unique<DataItem>(attributes1);

  ASSERT_TRUE(d->isTimeSeries());

  ObservationPtr event1(new Observation(*d, 123, time, (string) "6||1 2 3 4 5 6 "), true);
  const auto &attr_list1 = event1->getAttributes();
  map<string, string> attrs1;

  for (const auto &attr : attr_list1) attrs1[attr.first] = attr.second;

  ASSERT_TRUE(event1->isTimeSeries());

  ASSERT_EQ(6, event1->getSampleCount());
  auto values = event1->getTimeSeries();

  for (auto i = 0; i < event1->getSampleCount(); i++) {
    ASSERT_EQ((float)(i + 1), values[i]);
  }

  ASSERT_EQ((string) "", event1->getValue());
  ASSERT_EQ(0, (int)attrs1.count("sampleRate"));

  ObservationPtr event2(new Observation(*d, 123, time, (string) "7|42000|10 20 30 40 50 60 70 "),
                        true);
  const auto &attr_list2 = event2->getAttributes();
  map<string, string> attrs2;

  for (const auto &attr : attr_list2) attrs2[attr.first] = attr.second;

  ASSERT_TRUE(event2->isTimeSeries());

  ASSERT_EQ(7, event2->getSampleCount());
  ASSERT_EQ((string) "", event2->getValue());
  ASSERT_EQ((string) "42000", attrs2["sampleRate"]);
  values = event2->getTimeSeries();

  for (auto i = 0; i < event1->getSampleCount(); i++) {
    ASSERT_EQ((float)((i + 1) * 10), values[i]);
  }

  d.reset();
}

TEST_F(ObservationTest, Duration) {
  string time("2011-02-18T15:52:41Z@200.1232");
  std::map<string, string> attributes1;
  attributes1["id"] = "1";
  attributes1["name"] = "test";
  attributes1["type"] = "TEMPERATURE";
  attributes1["category"] = "SAMPLE";
  attributes1["statistic"] = "AVERAGE";
  auto d = make_unique<DataItem>(attributes1);

  ObservationPtr event1(new Observation(*d, 123, time, (string) "11.0"), true);
  const auto &attr_list = event1->getAttributes();
  map<string, string> attrs1;

  for (const auto &attr : attr_list) attrs1[attr.first] = attr.second;

  ASSERT_EQ((string) "AVERAGE", attrs1["statistic"]);
  ASSERT_EQ((string) "2011-02-18T15:52:41Z", attrs1["timestamp"]);
  ASSERT_EQ((string) "200.1232", attrs1["duration"]);

  d.reset();
}

TEST_F(ObservationTest, AssetChanged) {
  string time("2011-02-18T15:52:41Z@200.1232");
  std::map<string, string> attributes1;
  attributes1["id"] = "1";
  attributes1["name"] = "ac";
  attributes1["type"] = "ASSET_CHANGED";
  attributes1["category"] = "EVENT";
  auto d = make_unique<DataItem>(attributes1);

  ASSERT_TRUE(d->isAssetChanged());

  ObservationPtr event1(new Observation(*d, 123, time, (string) "CuttingTool|123"), true);
  const auto &attr_list = event1->getAttributes();
  map<string, string> attrs1;

  for (const auto &attr : attr_list) attrs1[attr.first] = attr.second;

  ASSERT_EQ((string) "CuttingTool", attrs1["assetType"]);
  ASSERT_EQ((string) "123", event1->getValue());

  d.reset();
}
