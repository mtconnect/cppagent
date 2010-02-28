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
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", a->getTypeString(true));
  CPPUNIT_ASSERT_EQUAL((string) "Acceleration", a->getTypeString(false));
  CPPUNIT_ASSERT_EQUAL((string) "PERCENT", a->getNativeUnits());
  CPPUNIT_ASSERT(a->getSubType().empty());
  CPPUNIT_ASSERT(!a->hasNativeScale());
  
  CPPUNIT_ASSERT_EQUAL((string) "3", b->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DataItemTest2", b->getName());
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", b->getType());
  CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", b->getTypeString(true));
  CPPUNIT_ASSERT_EQUAL((string) "Acceleration", b->getTypeString(false));
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
  CPPUNIT_ASSERT(DataItem::getCamelType("").empty());
  CPPUNIT_ASSERT_EQUAL((string) "Camels", DataItem::getCamelType("CAMELS"));
  CPPUNIT_ASSERT_EQUAL((string) "CamelCase",
    DataItem::getCamelType("CAMEL_CASE"));
  CPPUNIT_ASSERT_EQUAL((string) "ABCc",
    DataItem::getCamelType("A_B_CC"));
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
}

void DataItemTest::testCondition()
{
  CPPUNIT_ASSERT_EQUAL(DataItem::CONDITION, c->getCategory());
}