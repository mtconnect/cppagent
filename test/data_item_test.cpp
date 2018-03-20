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
	m_dataItemA = new DataItem(attributes1);

	attributes2["id"] = "3";
	attributes2["name"] = "DataItemTest2";
	attributes2["type"] = "ACCELERATION";
	attributes2["subType"] = "ACTUAL";
	attributes2["category"] = "EVENT";
	attributes2["units"] = "REVOLUTION/MINUTE";
	attributes2["nativeScale"] = "1.0";
	attributes2["significantDigits"] = "1";
	attributes2["coordinateSystem"] = "testCoordinateSystem";
	m_dataItemB = new DataItem(attributes2);

	attributes3["id"] = "4";
	attributes3["name"] = "DataItemTest3";
	attributes3["type"] = "LOAD";
	attributes3["category"] = "CONDITION";
	m_dataItemC = new DataItem(attributes3);
}


void DataItemTest::tearDown()
{
	delete m_dataItemA; m_dataItemA = nullptr;
	delete m_dataItemB; m_dataItemB = nullptr;
	delete m_dataItemC; m_dataItemC = nullptr;
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
	auto const &attributes1 = m_dataItemA->getAttributes();
	CPPUNIT_ASSERT_EQUAL((string) "1", attributes1.at("id"));
	CPPUNIT_ASSERT_EQUAL((string) "DataItemTest1", attributes1.at("name"));
	CPPUNIT_ASSERT_EQUAL((string) "ACCELERATION", attributes1.at("type"));
	CPPUNIT_ASSERT(attributes1.find("subType") == attributes1.end());
	CPPUNIT_ASSERT_EQUAL((string) "PERCENT", attributes1.at("nativeUnits"));
	CPPUNIT_ASSERT(attributes1.find("getNativeScale") == attributes1.end());
	CPPUNIT_ASSERT(attributes1.find("coordinateSystem") == attributes1.end());

	auto const &attributes2 = m_dataItemB->getAttributes();
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

	m_dataItemB->addSource("DataItemTest2Source");
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

	Component axes("Axes", attributes1);
	m_dataItemA->setComponent(axes);

	CPPUNIT_ASSERT_EQUAL(&axes, m_dataItemA->getComponent());
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

	Adapter a("", "", 0);
	a.setConversionRequired(false);

	item5.setDataSource(&a);
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
	DataItem *d = new DataItem(attributes1);

	CPPUNIT_ASSERT_EQUAL(string("PositionTimeSeries"), d->getElementName());
	delete d; d = nullptr;

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
	delete d; d = nullptr;
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
	delete d; d = nullptr;
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
	delete d; d = nullptr;
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
