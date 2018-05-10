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
#include "xml_parser_test.hpp"
#include "test_globals.hpp"
#include <stdexcept>
#include <iostream>
#include <fstream>

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(XmlParserTest);

using namespace std;


void XmlParserTest::setUp()
{
	m_xmlParser = nullptr;

	try
	{
		m_xmlParser = new XmlParser();
		m_devices = m_xmlParser->parseFile("../samples/test_config.xml");
	}
	catch (exception &)
	{
		CPPUNIT_FAIL("Could not locate test xml: ../samples/test_config.xml");
	}
}


void XmlParserTest::tearDown()
{
	if (m_xmlParser)
	{
		delete m_xmlParser;
		m_xmlParser = nullptr;
	}
}


void XmlParserTest::testConstructor()
{
	if (m_xmlParser)
	{
		delete m_xmlParser;
		m_xmlParser = nullptr;
	}

	m_xmlParser = new XmlParser();
	CPPUNIT_ASSERT_THROW(m_xmlParser->parseFile("../samples/badPath.xml"), std::runtime_error);
	delete m_xmlParser; m_xmlParser = nullptr;
	m_xmlParser = new XmlParser();
	CPPUNIT_ASSERT_NO_THROW(m_xmlParser->parseFile("../samples/test_config.xml"));
}


void XmlParserTest::testGetDevices()
{
	CPPUNIT_ASSERT_EQUAL((size_t) 1, m_devices.size());

	const auto device = m_devices.front();

	// Check for Description
	CPPUNIT_ASSERT_EQUAL((string) "Linux CNC Device", device->getDescriptionBody());

	vector<DataItem *> dataItems;
	const auto &dataItemsMap = device->getDeviceDataItems();

	for (auto const &mapItem : dataItemsMap)
		dataItems.push_back(mapItem.second);

	bool hasExec = false, hasZcom = false;

	for (auto const &dataItem : dataItems)
	{
		if (dataItem->getId() == "p5" && dataItem->getName() == "execution")
			hasExec = true;

		if (dataItem->getId() == "z2" && dataItem->getName() == "Zcom")
			hasZcom = true;
	}

	CPPUNIT_ASSERT(hasExec);
	CPPUNIT_ASSERT(hasZcom);
}


void XmlParserTest::testCondition()
{
	CPPUNIT_ASSERT_EQUAL((size_t) 1, m_devices.size());

	const auto device = m_devices.front();
	auto dataItemsMap = device->getDeviceDataItems();

	const auto item = dataItemsMap.at("clc");
	CPPUNIT_ASSERT(item);

	CPPUNIT_ASSERT_EQUAL((string) "clc", item->getId());
	CPPUNIT_ASSERT(item->isCondition());
}


void XmlParserTest::testGetDataItems()
{
	std::set<string> filter;

	m_xmlParser->getDataItems(filter, "//Linear");
	CPPUNIT_ASSERT_EQUAL(13, (int) filter.size());

	filter.clear();
	m_xmlParser->getDataItems(filter, "//Linear//DataItem[@category='CONDITION']");
	CPPUNIT_ASSERT_EQUAL(3, (int) filter.size());

	filter.clear();
	m_xmlParser->getDataItems(filter, "//Controller/electric/*");
	CPPUNIT_ASSERT_EQUAL(0, (int) filter.size());

	filter.clear();
	m_xmlParser->getDataItems(filter, "//Device/DataItems");
	CPPUNIT_ASSERT_EQUAL(2, (int) filter.size());

	filter.clear();
	m_xmlParser->getDataItems(filter, "//Device/DataItems/");
	CPPUNIT_ASSERT_EQUAL(0, (int) filter.size());

	filter.clear();
	m_xmlParser->getDataItems(filter, "//Rotary[@name=\"C\"]//DataItem[@type=\"LOAD\"]");
	CPPUNIT_ASSERT_EQUAL(2, (int) filter.size());

	filter.clear();
	m_xmlParser->getDataItems(filter,
		"//Rotary[@name=\"C\"]//DataItem[@category=\"CONDITION\" or @category=\"SAMPLE\"]");
	CPPUNIT_ASSERT_EQUAL(5, (int) filter.size());
}


void XmlParserTest::testGetDataItemsExt()
{
	std::set<string> filter;

	if(m_xmlParser)
	{
		delete m_xmlParser;
		m_xmlParser = nullptr;
	}

	// For the rest we will check with the extended schema
	try
	{
		m_xmlParser = new XmlParser();
		m_xmlParser->parseFile("../samples/extension.xml");
	}
	catch (exception &)
	{
		CPPUNIT_FAIL("Could not locate test xml: ../samples/extension.xml");
	}

	filter.clear();
	m_xmlParser->getDataItems(filter, "//Device//Pump");
	CPPUNIT_ASSERT_EQUAL(0, (int) filter.size());

	filter.clear();
	m_xmlParser->getDataItems(filter, "//Device//x:Pump");
	CPPUNIT_ASSERT_EQUAL(1, (int) filter.size());
}


void XmlParserTest::testExtendedSchema()
{
	if(m_xmlParser)
	{
		delete m_xmlParser;
		m_xmlParser = nullptr;
	}

	try
	{
		m_xmlParser = new XmlParser();
		m_devices = m_xmlParser->parseFile("../samples/extension.xml");
	}
	catch (exception &)
	{
		CPPUNIT_FAIL("Could not locate test xml: ../samples/extension.xml");
	}

	CPPUNIT_ASSERT_EQUAL((size_t) 1, m_devices.size());

	const auto device = m_devices.front();

	// Check for Description
	CPPUNIT_ASSERT_EQUAL((string) "Extended Schema.", device->getDescriptionBody());

	Component *pump = device->getChildren().front();
	CPPUNIT_ASSERT_EQUAL((string) "pump", pump->getName());
	CPPUNIT_ASSERT_EQUAL((string) "Pump", pump->getClass());
	CPPUNIT_ASSERT_EQUAL((string) "x", pump->getPrefix());

	DataItem *item = pump->getDataItems().front();
	CPPUNIT_ASSERT_EQUAL((string) "x:FLOW", item->getType());
	CPPUNIT_ASSERT_EQUAL((string) "Flow", item->getElementName());
	CPPUNIT_ASSERT_EQUAL((string) "x", item->getPrefix());
}


void XmlParserTest::testTimeSeries()
{
	const auto dev = m_devices[0];
	CPPUNIT_ASSERT(dev);

	auto item = dev->getDeviceDataItem("Xact");
	CPPUNIT_ASSERT(item);

	item->getAttributes();
	CPPUNIT_ASSERT_EQUAL((string)"AVERAGE", item->getStatistic());

	const auto &attrs1 = item->getAttributes();
	CPPUNIT_ASSERT_EQUAL(string("AVERAGE"), attrs1.at("statistic"));


	item = dev->getDeviceDataItem("Xts");
	CPPUNIT_ASSERT(item);
	item->getAttributes();
	CPPUNIT_ASSERT(item->isTimeSeries());
	CPPUNIT_ASSERT_EQUAL(DataItem::TIME_SERIES, item->getRepresentation());

	const auto &attrs2 = item->getAttributes();
	CPPUNIT_ASSERT_EQUAL(string("TIME_SERIES"), attrs2.at("representation"));
}


void XmlParserTest::testConfiguration()
{
	const auto dev = m_devices[0];
	CPPUNIT_ASSERT(dev);

	Component *power = nullptr;
	const auto &children = dev->getChildren();

	for (auto const &iter : children)
	{
		if (iter->getName() == "power")
			power = iter;
	}

	CPPUNIT_ASSERT(power);
	CPPUNIT_ASSERT(!power->getConfiguration().empty());
}


void XmlParserTest::testParseAsset()
{
	auto document = getFile("asset1.xml");
	AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
	CuttingToolPtr tool = (CuttingTool *) asset.getObject();

	CPPUNIT_ASSERT_EQUAL((string) "KSSP300R4SD43L240", tool->getIdentity().at("toolId"));
	CPPUNIT_ASSERT_EQUAL((string) "KSSP300R4SD43L240.1", tool->getAssetId());
	CPPUNIT_ASSERT_EQUAL((string) "1", tool->getIdentity().at("serialNumber"));
	CPPUNIT_ASSERT_EQUAL((string) "KMT,Parlec", tool->getIdentity().at("manufacturers"));
	CPPUNIT_ASSERT_EQUAL((string) "2011-05-11T13:55:22", tool->getTimestamp());
	CPPUNIT_ASSERT_EQUAL(false, tool->isRemoved());

	// Top Level
	CPPUNIT_ASSERT_EQUAL((string) "ISO 13399...", tool->m_values.at("CuttingToolDefinition")->m_value);
	CPPUNIT_ASSERT_EQUAL((string) "EXPRESS",
	                     tool->m_values.at("CuttingToolDefinition")->m_properties.at("format"));
	CPPUNIT_ASSERT_EQUAL((string) "Cutting tool ...", tool->getDescription());

	// Status
	CPPUNIT_ASSERT_EQUAL((string) "NEW", tool->m_status[0]);

	// Values
	CPPUNIT_ASSERT_EQUAL((string) "10000", tool->m_values.at("ProgramSpindleSpeed")->m_value);
	CPPUNIT_ASSERT_EQUAL((string) "222", tool->m_values.at("ProgramFeedRate")->m_value);
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, tool->m_values.at("ProgramFeedRate")->refCount());

	// Measurements
	CPPUNIT_ASSERT_EQUAL((string) "73.25", tool->m_measurements.at("BodyDiameterMax")->m_value);
	CPPUNIT_ASSERT_EQUAL((string) "76.2", tool->m_measurements.at("CuttingDiameterMax")->m_value);
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, tool->m_measurements.at("BodyDiameterMax")->refCount());

	// Items
	CPPUNIT_ASSERT_EQUAL((string) "24", tool->m_itemCount);

	// Item
	CPPUNIT_ASSERT_EQUAL((size_t) 6, tool->m_items.size());
	CuttingItemPtr item = tool->m_items[0];
	CPPUNIT_ASSERT_EQUAL((unsigned int) 2, item->refCount());

	CPPUNIT_ASSERT_EQUAL((string) "SDET43PDER8GB", item->m_identity.at("itemId"));
	CPPUNIT_ASSERT_EQUAL((string) "FLANGE: 1-4, ROW: 1", item->m_values.at("Locus")->m_value);
	CPPUNIT_ASSERT_EQUAL((string) "12.7", item->m_measurements.at("CuttingEdgeLength")->m_value);
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, item->m_measurements.at("CuttingEdgeLength")->refCount());
}


void XmlParserTest::testParseOtherAsset()
{
	string document = "<Workpiece assetId=\"XXX123\" timestamp=\"2014-04-14T01:22:33.123\" "
		"serialNumber=\"A1234\" deviceUuid=\"XXX\" >Data</Workpiece>";
	AssetPtr asset = m_xmlParser->parseAsset("XXX", "Workpiece", document);

	CPPUNIT_ASSERT(asset.getObject());
	CPPUNIT_ASSERT_EQUAL((string) "XXX123", asset->getAssetId());
	CPPUNIT_ASSERT_EQUAL((string) "2014-04-14T01:22:33.123", asset->getTimestamp());
	CPPUNIT_ASSERT_EQUAL((string) "XXX", asset->getDeviceUuid());
	CPPUNIT_ASSERT_EQUAL((string) "Data", asset->getContent());
	CPPUNIT_ASSERT_EQUAL(false, asset->isRemoved());

	document = "<Workpiece assetId=\"XXX123\" timestamp=\"2014-04-14T01:22:33.123\" "
		"serialNumber=\"A1234\" deviceUuid=\"XXX\" removed=\"true\">Data</Workpiece>";
	asset = m_xmlParser->parseAsset("XXX", "Workpiece", document);

	CPPUNIT_ASSERT(asset.getObject());
	CPPUNIT_ASSERT_EQUAL(true, asset->isRemoved());
}


void XmlParserTest::testParseRemovedAsset()
{
	auto document = getFile("asset3.xml");
	AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
	CuttingToolPtr tool = (CuttingTool *) asset.getObject();

	CPPUNIT_ASSERT_EQUAL(true, tool->isRemoved());
}


void XmlParserTest::testUpdateAsset()
{
	auto document = getFile("asset1.xml");
	AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
	CuttingToolPtr tool = (CuttingTool *) asset.getObject();

	string replacement =
	"<CuttingDiameterMax code=\"DC\" nominal=\"76.2\" maximum=\"76.213\" minimum=\"76.187\">10.123</CuttingDiameterMax>";
	m_xmlParser->updateAsset(asset, "CuttingTool", replacement);

	CuttingItemPtr item = tool->m_items[0];
	CPPUNIT_ASSERT_EQUAL((string) "10.123", tool->m_measurements.at("CuttingDiameterMax")->m_value);

	// Test cutting item replacement
	CPPUNIT_ASSERT_EQUAL((string) "12.7", item->m_measurements.at("CuttingEdgeLength")->m_value);

	replacement =
	"<CuttingItem indices=\"1-4\" itemId=\"SDET43PDER8GB\" manufacturers=\"KMT\" grade=\"KC725M\">"
	"<Locus>FLANGE: 1-4, ROW: 1</Locus>"
	"<Measurements>"
	"<CuttingEdgeLength code=\"L\" nominal=\"12.7\" minimum=\"12.675\" maximum=\"12.725\">14.7</CuttingEdgeLength>"
	"<WiperEdgeLength code=\"BS\" nominal=\"2.56\">2.56</WiperEdgeLength>"
	"<IncribedCircleDiameter code=\"IC\" nominal=\"12.7\">12.7</IncribedCircleDiameter>"
	"<CornerRadius code=\"RE\" nominal=\"0.8\">0.8</CornerRadius>"
	"</Measurements>"
	"</CuttingItem>";

	m_xmlParser->updateAsset(asset, "CuttingTool", replacement);

	item = tool->m_items[0];
	CPPUNIT_ASSERT_EQUAL((string) "14.7", item->m_measurements.at("CuttingEdgeLength")->m_value);
}


void XmlParserTest::testBadAsset()
{
	auto xml = getFile("asset4.xml");

	auto asset = m_xmlParser->parseAsset("XXX", "CuttingTool", xml);
	CPPUNIT_ASSERT(!asset);
}


void XmlParserTest::testNoNamespace()
{
	if(m_xmlParser)
	{
		delete m_xmlParser;
		m_xmlParser = nullptr;
	}

	m_xmlParser = new XmlParser();
	CPPUNIT_ASSERT_NO_THROW(m_xmlParser->parseFile("../samples/NoNamespace.xml"));
}

void XmlParserTest::testFilteredDataItem13()
{
	delete m_xmlParser; m_xmlParser = nullptr;
	try
	{
		m_xmlParser = new XmlParser();
		m_devices = m_xmlParser->parseFile("../samples/filter_example_1.3.xml");
	}
	catch (exception & e)
	{
		CPPUNIT_FAIL("Could not locate test xml: ../samples/filter_example_1.3.xml");
	}

	Device *dev = m_devices[0];
	DataItem *di = dev->getDeviceDataItem("c1");

	CPPUNIT_ASSERT_EQUAL(di->getFilterValue(), 5.0);
	CPPUNIT_ASSERT(di->hasMinimumDelta());
}

void XmlParserTest::testFilteredDataItem()
{
	if(m_xmlParser)
	{
		delete m_xmlParser;
		m_xmlParser = nullptr;
	}

	try
	{
		m_xmlParser = new XmlParser();
		m_devices = m_xmlParser->parseFile("../samples/filter_example.xml");
	}
	catch (exception &)
	{
		CPPUNIT_FAIL("Could not locate test xml: ../samples/filter_example.xml");
	}

	auto di = m_devices[0]->getDeviceDataItem("c1");

	CPPUNIT_ASSERT_EQUAL(di->getFilterValue(), 5.0);
	CPPUNIT_ASSERT(di->hasMinimumDelta());
	di = m_devices[0]->getDeviceDataItem("c2");

	CPPUNIT_ASSERT_EQUAL(di->getFilterPeriod(), 10.0);
	CPPUNIT_ASSERT(di->hasMinimumPeriod());
}


void XmlParserTest::testReferences()
{
	if(m_xmlParser)
	{
		delete m_xmlParser;
		m_xmlParser = nullptr;
	}

	try
	{
		m_xmlParser = new XmlParser();
		m_devices = m_xmlParser->parseFile("../samples/reference_example.xml");
	}
	catch (exception &)
	{
		CPPUNIT_FAIL("Could not locate test xml: ../samples/reference_example.xml");
	}

	string id = "mf";
	const auto item = m_devices[0]->getDeviceDataItem(id);
	const auto comp = item->getComponent();

	comp->resolveReferences();

	const auto refs = comp->getReferences();
	const auto &ref = refs.front();

	CPPUNIT_ASSERT_EQUAL((string) "c4", ref.m_id);
	CPPUNIT_ASSERT_EQUAL((string) "chuck", ref.m_name);

	CPPUNIT_ASSERT_MESSAGE("DataItem was not resolved", ref.m_dataItem);

	const auto &ref2 = refs.back();
	CPPUNIT_ASSERT_EQUAL((string) "d2", ref2.m_id);
	CPPUNIT_ASSERT_EQUAL((string) "door", ref2.m_name);

	CPPUNIT_ASSERT_MESSAGE("DataItem was not resolved", ref2.m_dataItem);

	std::set<string> filter;
	m_xmlParser->getDataItems(filter, "//BarFeederInterface");

	CPPUNIT_ASSERT_EQUAL((size_t) 3, filter.size());
	CPPUNIT_ASSERT_EQUAL((size_t) 1, filter.count("mf"));
	CPPUNIT_ASSERT_EQUAL((size_t) 1, filter.count("c4"));
	CPPUNIT_ASSERT_EQUAL((size_t) 1, filter.count("d2"));
}


void XmlParserTest::testExtendedAsset()
{
	auto document = getFile("ext_asset.xml");
	AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
	CuttingToolPtr tool = (CuttingTool *) asset.getObject();


	CPPUNIT_ASSERT_EQUAL(((size_t) 1), tool->m_values.count("x:Color"));
}


void XmlParserTest::testExtendedAssetFragment()
{
	auto document = getFile("ext_asset_2.xml");
	AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
	CuttingToolPtr tool = (CuttingTool *) asset.getObject();


	CPPUNIT_ASSERT_EQUAL(((size_t) 1), tool->m_values.count("x:Color"));
}
