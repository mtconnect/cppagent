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
#include <dlib/misc_api.h>
#include "adapter.hpp"
#include "data_item.hpp"

using namespace std;
using namespace mtconnect;

TEST_CLASS(DataItemTest)
{
protected:
  std::unique_ptr<DataItem> m_dataItemA;
  std::unique_ptr<DataItem> m_dataItemB;
  std::unique_ptr<DataItem> m_dataItemC;
  
public:
  void testGetters();
  void testGetAttributes();
  void testHasNameAndSource();
  void testIsSample();
  void testComponent();
  void testGetCamel();
  void testConversion();
  void testCondition();
  void testTimeSeries();
  void testStatistic();
  void testSampleRate();
  void testDuplicates();
  void testFilter();
  void testReferences();
  
  SET_UP();
  TEAR_DOWN();
  
  CPPUNIT_TEST_SUITE(DataItemTest);
  CPPUNIT_TEST(testGetters);
  CPPUNIT_TEST(testGetAttributes);
  CPPUNIT_TEST(testHasNameAndSource);
  CPPUNIT_TEST(testIsSample);
  CPPUNIT_TEST(testComponent);
  CPPUNIT_TEST(testGetCamel);
  CPPUNIT_TEST(testConversion);
  CPPUNIT_TEST(testCondition);
  CPPUNIT_TEST(testTimeSeries);
  CPPUNIT_TEST(testStatistic);
  CPPUNIT_TEST(testSampleRate);
  CPPUNIT_TEST(testDuplicates);
  CPPUNIT_TEST(testFilter);
  CPPUNIT_TEST_SUITE_END();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(DataItemTest);

void DataItemTest::setUp()
{
  std::map<string, string> attributes1, attributes2, attributes3;
  
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "ACCELERATION";
  attributes1["category"] = "SAMPLE";
  attributes1["nativeUnits"] = "PERCENT";
  m_dataItemA = make_unique<DataItem>(attributes1);
  
  attributes2["id"] = "3";
  attributes2["name"] = "DataItemTest2";
  attributes2["type"] = "ACCELERATION";
  attributes2["subType"] = "ACTUAL";
  attributes2["category"] = "EVENT";
  attributes2["units"] = "REVOLUTION/MINUTE";
  attributes2["nativeScale"] = "1.0";
  attributes2["significantDigits"] = "1";
  attributes2["coordinateSystem"] = "testCoordinateSystem";
  m_dataItemB = make_unique<DataItem>(attributes2);
  
  attributes3["id"] = "4";
  attributes3["name"] = "DataItemTest3";
  attributes3["type"] = "LOAD";
  attributes3["category"] = "CONDITION";
  m_dataItemC = make_unique<DataItem>(attributes3);
}


void DataItemTest::tearDown()
{
  m_dataItemA.reset();
  m_dataItemB.reset();
  m_dataItemC.reset();
}


void DataItemTest::testGetters()
{
  CPPUNIT_ASSERT_EQUAL((string) "1", m_dataItemA->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest1", m_dataItemA->getName());
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", m_dataItemA->getType());
  CPPUNIT_ASSERT_EQUAL((string) "Acceleration", m_dataItemA->getElementName());
  CPPUNIT_ASSERT_EQUAL((string) "PERCENT", m_dataItemA->getNativeUnits());
  CPPUNIT_ASSERT(m_dataItemA->getSubType().empty());
  CPPUNIT_ASSERT(!m_dataItemA->hasNativeScale());
  
  CPPUNIT_ASSERT_EQUAL((string) "3", m_dataItemB->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2", m_dataItemB->getName());
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", m_dataItemB->getType());
  CPPUNIT_ASSERT_EQUAL((string) "Acceleration", m_dataItemB->getElementName());
  CPPUNIT_ASSERT_EQUAL((string) "ACTUAL", m_dataItemB->getSubType());
  CPPUNIT_ASSERT_EQUAL(m_dataItemB->getNativeUnits(), m_dataItemB->getUnits());
  CPPUNIT_ASSERT_EQUAL(1.0f, m_dataItemB->getNativeScale());
}


void DataItemTest::testGetAttributes()
{
  const auto &attributes1 = m_dataItemA->getAttributes();
  CPPUNIT_ASSERT_EQUAL((string) "1", attributes1.at("id"));
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest1", attributes1.at("name"));
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", attributes1.at("type"));
  CPPUNIT_ASSERT(attributes1.find("subType") == attributes1.end());
  CPPUNIT_ASSERT_EQUAL((string) "PERCENT", attributes1.at("nativeUnits"));
  CPPUNIT_ASSERT(attributes1.find("getNativeScale") == attributes1.end());
  CPPUNIT_ASSERT(attributes1.find("coordinateSystem") == attributes1.end());
  
  const auto &attributes2 = m_dataItemB->getAttributes();
  CPPUNIT_ASSERT_EQUAL((string) "3", attributes2.at("id"));
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2", attributes2.at("name"));
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", attributes2.at("type"));
  CPPUNIT_ASSERT_EQUAL((string) "ACTUAL", attributes2.at("subType"));
  CPPUNIT_ASSERT_EQUAL(attributes2.at("nativeUnits"), attributes2.at("units"));
  CPPUNIT_ASSERT_EQUAL((string) "1", attributes2.at("nativeScale"));
  CPPUNIT_ASSERT_EQUAL((string) "testCoordinateSystem",
                       attributes2.at("coordinateSystem"));
}


void DataItemTest::testHasNameAndSource()
{
  CPPUNIT_ASSERT(m_dataItemA->hasName("DataItemTest1"));
  CPPUNIT_ASSERT(m_dataItemB->hasName("DataItemTest2"));
  
  CPPUNIT_ASSERT(m_dataItemA->getSource().empty());
  CPPUNIT_ASSERT(m_dataItemB->getSource().empty());
  
  CPPUNIT_ASSERT(!m_dataItemB->hasName("DataItemTest2Source"));
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2", m_dataItemB->getSourceOrName());
  
  m_dataItemB->addSource("DataItemTest2Source", "", "", "");
  CPPUNIT_ASSERT(m_dataItemB->hasName("DataItemTest2Source"));
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2Source", m_dataItemB->getSource());
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2Source", m_dataItemB->getSourceOrName());
}


void DataItemTest::testIsSample()
{
  CPPUNIT_ASSERT(m_dataItemA->isSample());
  CPPUNIT_ASSERT(!m_dataItemB->isSample());
}


void DataItemTest::testComponent()
{
  std::map<string, string> attributes1;
  attributes1["id"] = "3";
  attributes1["name"] = "AxesTestA";
  attributes1["uuid"] = "UniversalUniqueIdA";
  attributes1["sampleRate"] = "100.11";
  
  mtconnect::Component axes("Axes", attributes1);
  m_dataItemA->setComponent(axes);
  
  CPPUNIT_ASSERT(&axes == m_dataItemA->getComponent());
}


void DataItemTest::testGetCamel()
{
  string prefix;
  CPPUNIT_ASSERT(DataItem::getCamelType("", prefix).empty());
  CPPUNIT_ASSERT_EQUAL((string) "Camels", DataItem::getCamelType("CAMELS", prefix));
  CPPUNIT_ASSERT(prefix.empty());
  
  // Test the one exception to the rules...
  CPPUNIT_ASSERT_EQUAL((string) "PH", DataItem::getCamelType("PH", prefix));
  
  CPPUNIT_ASSERT_EQUAL((string) "CamelCase", DataItem::getCamelType("CAMEL_CASE", prefix));
  CPPUNIT_ASSERT_EQUAL((string) "ABCc", DataItem::getCamelType("A_B_CC", prefix));
  prefix.clear();
  CPPUNIT_ASSERT_EQUAL((string) "CamelCase", DataItem::getCamelType("x:CAMEL_CASE", prefix));
  CPPUNIT_ASSERT_EQUAL((string) "x", prefix);
}


void DataItemTest::testConversion()
{
  std::map<string, string> attributes1;
  attributes1["id"] = "p";
  attributes1["name"] = "position";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER_3D";
  attributes1["nativeUnits"] = "INCH_3D";
  attributes1["coordinateSystem"] = "testCoordinateSystem";
  DataItem item1(attributes1);
  item1.conversionRequired();
  
  CPPUNIT_ASSERT_EQUAL((string) "25.4 50.8 76.2", item1.convertValue("1 2 3"));
  CPPUNIT_ASSERT_EQUAL((string) "25.4 50.8 76.2", item1.convertValue("1  2  3"));
  
  std::map<string, string> attributes2;
  attributes2["id"] = "p";
  attributes2["name"] = "position";
  attributes2["type"] = "POSITION";
  attributes2["category"] = "SAMPLE";
  attributes2["units"] = "DEGREE_3D";
  attributes2["nativeUnits"] = "RADIAN_3D";
  attributes2["coordinateSystem"] = "testCoordinateSystem";
  DataItem item2(attributes2);
  item2.conversionRequired();
  
  CPPUNIT_ASSERT_EQUAL((string) "57.29578 114.5916 171.8873", item2.convertValue("1 2 3"));
  
  std::map<string, string> attributes3;
  attributes3["id"] = "p";
  attributes3["name"] = "position";
  attributes3["type"] = "POSITION";
  attributes3["category"] = "SAMPLE";
  attributes3["units"] = "MILLIMETER";
  attributes3["nativeUnits"] = "MILLIMETER";
  attributes3["nativeScale"] = "10";
  attributes3["coordinateSystem"] = "testScale";
  DataItem item3(attributes3);
  item3.conversionRequired();
  
  CPPUNIT_ASSERT_EQUAL((string) "1.3", item3.convertValue("13"));
  
  std::map<string, string> attributes4;
  attributes4["id"] = "p";
  attributes4["name"] = "position";
  attributes4["type"] = "POSITION";
  attributes4["category"] = "SAMPLE";
  attributes4["units"] = "AMPERE";
  attributes4["nativeUnits"] = "KILOAMPERE";
  attributes4["coordinateSystem"] = "testScale";
  DataItem item4(attributes4);
  item4.conversionRequired();
  
  CPPUNIT_ASSERT_EQUAL((string) "130", item4.convertValue("0.13"));
  
  DataItem item5(attributes4);
  
  Adapter adapter("", "", 0);
  adapter.setConversionRequired(false);
  
  item5.setDataSource(&adapter);
  CPPUNIT_ASSERT(!item5.conversionRequired());
  
  CPPUNIT_ASSERT_EQUAL((string) "0.13", item5.convertValue("0.13"));
  
}


void DataItemTest::testCondition()
{
  CPPUNIT_ASSERT_EQUAL(DataItem::CONDITION, m_dataItemC->getCategory());
}


void DataItemTest::testTimeSeries()
{
  std::map<string, string> attributes1;
  
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["representation"] = "TIME_SERIES";
  auto d = make_unique<DataItem>(attributes1);
  
  CPPUNIT_ASSERT_EQUAL(string("PositionTimeSeries"), d->getElementName());
  d.reset();
  
  attributes1.clear();
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["representation"] = "VALUE";
  d = make_unique<DataItem>(attributes1);
  
  CPPUNIT_ASSERT_EQUAL(string("Position"), d->getElementName());
  d.reset();
}


void DataItemTest::testStatistic()
{
  std::map<string, string> attributes1;
  
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["statistic"] = "AVERAGE";
  auto d = make_unique<DataItem>(attributes1);
  
  CPPUNIT_ASSERT_EQUAL(string("AVERAGE"), d->getStatistic());
  d.reset();
}


void DataItemTest::testSampleRate()
{
  std::map<string, string> attributes1;
  
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["statistic"] = "AVERAGE";
  attributes1["representation"] = "TIME_SERIES";
  attributes1["sampleRate"] = "42000";
  auto d = make_unique<DataItem>(attributes1);
  
  CPPUNIT_ASSERT_EQUAL(string("42000"), d->getSampleRate());
  d.reset();
}


void DataItemTest::testDuplicates()
{
  CPPUNIT_ASSERT(!m_dataItemA->isDuplicate("FOO"));
  CPPUNIT_ASSERT(m_dataItemA->isDuplicate("FOO"));
  CPPUNIT_ASSERT(!m_dataItemA->isDuplicate("FOO2"));
}


void DataItemTest::testFilter()
{
  m_dataItemA->setMinmumDelta(5.0);
  
  CPPUNIT_ASSERT(!m_dataItemA->isFiltered(5.0, 0.0));
  CPPUNIT_ASSERT(m_dataItemA->isFiltered(6.0, 0.0));
  CPPUNIT_ASSERT(!m_dataItemA->isFiltered(10.1, 0.0));
  CPPUNIT_ASSERT(m_dataItemA->isFiltered(6.0, 0.0));
  CPPUNIT_ASSERT(!m_dataItemA->isFiltered(5.0, 0.0));
  
  // Test period
  m_dataItemA->setMinmumDelta(1.0);
  m_dataItemA->setMinmumPeriod(1.0);
  
  CPPUNIT_ASSERT(!m_dataItemA->isFiltered(1.0, 0.0));
  CPPUNIT_ASSERT(m_dataItemA->isFiltered(3.0, 0.1));
  CPPUNIT_ASSERT(!m_dataItemA->isFiltered(5, 1.1));
  CPPUNIT_ASSERT(m_dataItemA->isFiltered(7.0, 2.0));
  CPPUNIT_ASSERT(!m_dataItemA->isFiltered(9.0, 2.2));
  
}
