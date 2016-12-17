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

#include "data_item_test.hpp"
#include <dlib/misc_api.h>
#include "adapter.hpp"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(DataItemTest);

using namespace std;

void DataItemTest::setUp()
{
  std::map<string, string> attributes1, attributes2, attributes3;
  
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "ACCELERATION";
  attributes1["category"] = "SAMPLE";
  attributes1["nativeUnits"] = "PERCENT";
  a = new DataItem(attributes1);
  
  attributes2["id"] = "3";
  attributes2["name"] = "DataItemTest2";
  attributes2["type"] = "ACCELERATION";
  attributes2["subType"] = "ACTUAL";
  attributes2["category"] = "EVENT";
  attributes2["units"] = "REVOLUTION/MINUTE";
  attributes2["nativeScale"] = "1.0";
  attributes2["significantDigits"] = "1";
  attributes2["coordinateSystem"] = "testCoordinateSystem";
  b = new DataItem(attributes2);

  attributes3["id"] = "4";
  attributes3["name"] = "DataItemTest3";
  attributes3["type"] = "LOAD";
  attributes3["category"] = "CONDITION";
  c = new DataItem(attributes3);
}

void DataItemTest::tearDown()
{
  delete a;
  delete b;
  delete c;
}

void DataItemTest::testGetters()
{
  CPPUNIT_ASSERT_EQUAL((string) "1", a->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest1", a->getName());
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", a->getType());
  CPPUNIT_ASSERT_EQUAL((string) "Acceleration", a->getElementName());
  CPPUNIT_ASSERT_EQUAL((string) "PERCENT", a->getNativeUnits());
  CPPUNIT_ASSERT(a->getSubType().empty());
  CPPUNIT_ASSERT(!a->hasNativeScale());
  
  CPPUNIT_ASSERT_EQUAL((string) "3", b->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2", b->getName());
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", b->getType());
  CPPUNIT_ASSERT_EQUAL((string) "Acceleration", b->getElementName());
  CPPUNIT_ASSERT_EQUAL((string) "ACTUAL", b->getSubType());
  CPPUNIT_ASSERT_EQUAL(b->getNativeUnits(), b->getUnits());
  CPPUNIT_ASSERT_EQUAL(1.0f, b->getNativeScale());
}

void DataItemTest::testGetAttributes()
{
  std::map<string, string> &attributes1 = *a->getAttributes();
  CPPUNIT_ASSERT_EQUAL((string) "1", attributes1["id"]);
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest1", attributes1["name"]);
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", attributes1["type"]);
  CPPUNIT_ASSERT(attributes1["subType"].empty());
  CPPUNIT_ASSERT_EQUAL((string) "PERCENT", attributes1["nativeUnits"]);
  CPPUNIT_ASSERT(attributes1["getNativeScale"].empty());
  CPPUNIT_ASSERT(attributes1["coordinateSystem"].empty());
  
  std::map<string, string> &attributes2 = *b->getAttributes();
  CPPUNIT_ASSERT_EQUAL((string) "3", attributes2["id"]);
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2", attributes2["name"]);
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", attributes2["type"]);
  CPPUNIT_ASSERT_EQUAL((string) "ACTUAL", attributes2["subType"]);
  CPPUNIT_ASSERT_EQUAL(attributes2["nativeUnits"], attributes2["units"]);
  CPPUNIT_ASSERT_EQUAL((string) "1", attributes2["nativeScale"]);
  CPPUNIT_ASSERT_EQUAL((string) "testCoordinateSystem",
    attributes2["coordinateSystem"]);
}

void DataItemTest::testHasNameAndSource()
{
  CPPUNIT_ASSERT(a->hasName("DataItemTest1"));
  CPPUNIT_ASSERT(b->hasName("DataItemTest2"));
  
  CPPUNIT_ASSERT(a->getSource().empty());
  CPPUNIT_ASSERT(b->getSource().empty());
  
  CPPUNIT_ASSERT(!b->hasName("DataItemTest2Source"));
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2", b->getSourceOrName());
  
  b->addSource("DataItemTest2Source");
  CPPUNIT_ASSERT(b->hasName("DataItemTest2Source"));
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2Source", b->getSource());
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2Source", b->getSourceOrName());
}

void DataItemTest::testIsSample()
{
  CPPUNIT_ASSERT(a->isSample());
  CPPUNIT_ASSERT(!b->isSample());
}

void DataItemTest::testComponent()
{
  std::map<string, string> attributes1;
  attributes1["id"] = "3";
  attributes1["name"] = "AxesTestA";
  attributes1["uuid"] = "UniversalUniqueIdA";
  attributes1["sampleRate"] = "100.11";
  
  Component axes ("Axes", attributes1);
  a->setComponent(axes);
  
  CPPUNIT_ASSERT_EQUAL(&axes, a->getComponent());
}

void DataItemTest::testGetCamel()
{
  string prefix;
  CPPUNIT_ASSERT(DataItem::getCamelType("", prefix).empty());
  CPPUNIT_ASSERT_EQUAL((string) "Camels", DataItem::getCamelType("CAMELS", prefix));
  CPPUNIT_ASSERT(prefix.empty());
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
  
  Adapter a("", "", 0);
  a.setConversionRequired(false);
  
  item5.setDataSource(&a);
  CPPUNIT_ASSERT(!item5.conversionRequired());
  
  CPPUNIT_ASSERT_EQUAL((string) "0.13", item5.convertValue("0.13"));

}

void DataItemTest::testCondition()
{
  CPPUNIT_ASSERT_EQUAL(DataItem::CONDITION, c->getCategory());
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
  DataItem *d = new DataItem(attributes1);

  CPPUNIT_ASSERT_EQUAL(string("PositionTimeSeries"), d->getElementName());
  delete d;

  attributes1.clear();
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["representation"] = "VALUE";
  d = new DataItem(attributes1);

  CPPUNIT_ASSERT_EQUAL(string("Position"), d->getElementName());
  delete d;
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
  DataItem *d = new DataItem(attributes1);

  CPPUNIT_ASSERT_EQUAL(string("AVERAGE"), d->getStatistic());
  delete d;
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
  DataItem *d = new DataItem(attributes1);

  CPPUNIT_ASSERT_EQUAL(string("42000"), d->getSampleRate());
  delete d;
}

void DataItemTest::testDuplicates()
{
  CPPUNIT_ASSERT(!a->isDuplicate("FOO"));
  CPPUNIT_ASSERT(a->isDuplicate("FOO"));
  CPPUNIT_ASSERT(!a->isDuplicate("FOO2"));
}

void DataItemTest::testFilter()
{
  a->setMinmumDelta(5.0);
  
  CPPUNIT_ASSERT(!a->isFiltered(5.0, 0.0));
  CPPUNIT_ASSERT(a->isFiltered(6.0, 0.0));
  CPPUNIT_ASSERT(!a->isFiltered(10.1, 0.0));
  CPPUNIT_ASSERT(a->isFiltered(6.0, 0.0));
  CPPUNIT_ASSERT(!a->isFiltered(5.0, 0.0));
  
  // Test period
  a->setMinmumDelta(1.0);
  a->setMinmumPeriod(1.0);
  
  CPPUNIT_ASSERT(!a->isFiltered(1.0, 0.0));
  CPPUNIT_ASSERT(a->isFiltered(3.0, 0.1));
  CPPUNIT_ASSERT(!a->isFiltered(5, 1.1));
  CPPUNIT_ASSERT(a->isFiltered(7.0, 2.0));
  CPPUNIT_ASSERT(!a->isFiltered(9.0, 2.2));
  
}
