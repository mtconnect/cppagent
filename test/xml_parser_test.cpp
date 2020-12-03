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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "test_globals.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace std;
using namespace mtconnect;

class XmlParserTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    m_xmlParser = nullptr;

    try
    {
      std::unique_ptr<XmlPrinter> printer(new XmlPrinter());
      m_xmlParser = new XmlParser();
      m_devices =
          m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/test_config.xml", printer.get());
    }
    catch (exception &)
    {
      FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR << " /samples/test_config.xml";
    }
  }

  void TearDown() override
  {
    if (m_xmlParser)
    {
      delete m_xmlParser;
      m_xmlParser = nullptr;
    }
  }

  XmlParser *m_xmlParser{nullptr};
  std::vector<Device *> m_devices;
};

TEST_F(XmlParserTest, Constructor)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  std::unique_ptr<XmlPrinter> printer(new XmlPrinter());
  m_xmlParser = new XmlParser();
  ASSERT_THROW(m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/badPath.xml", printer.get()),
               std::runtime_error);
  delete m_xmlParser;
  m_xmlParser = nullptr;
  m_xmlParser = new XmlParser();
  ASSERT_NO_THROW(
      m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/test_config.xml", printer.get()));
}

TEST_F(XmlParserTest, GetDevices)
{
  ASSERT_EQ((size_t)1, m_devices.size());

  const auto device = m_devices.front();

  // Check for Description
  ASSERT_EQ((string) "Linux CNC Device", device->getDescriptionBody());

  vector<DataItem *> dataItems;
  const auto &dataItemsMap = device->getDeviceDataItems();

  for (auto const &mapItem : dataItemsMap)
    dataItems.emplace_back(mapItem.second);

  bool hasExec = false, hasZcom = false;

  for (auto const &dataItem : dataItems)
  {
    if (dataItem->getId() == "p5" && dataItem->getName() == "execution")
      hasExec = true;

    if (dataItem->getId() == "z2" && dataItem->getName() == "Zcom")
      hasZcom = true;
  }

  ASSERT_TRUE(hasExec);
  ASSERT_TRUE(hasZcom);
}

TEST_F(XmlParserTest, Condition)
{
  ASSERT_EQ((size_t)1, m_devices.size());

  const auto device = m_devices.front();
  auto dataItemsMap = device->getDeviceDataItems();

  const auto item = dataItemsMap.at("clc");
  ASSERT_TRUE(item);

  ASSERT_EQ((string) "clc", item->getId());
  ASSERT_TRUE(item->isCondition());
}

TEST_F(XmlParserTest, GetDataItems)
{
  std::set<string> filter;

  m_xmlParser->getDataItems(filter, "//Linear");
  ASSERT_EQ(13, (int)filter.size());

  filter.clear();
  m_xmlParser->getDataItems(filter, "//Linear//DataItem[@category='CONDITION']");
  ASSERT_EQ(3, (int)filter.size());

  filter.clear();
  m_xmlParser->getDataItems(filter, "//Controller/electric/*");
  ASSERT_EQ(0, (int)filter.size());

  filter.clear();
  m_xmlParser->getDataItems(filter, "//Device/DataItems");
  ASSERT_EQ(2, (int)filter.size());

  filter.clear();
  m_xmlParser->getDataItems(filter, "//Device/DataItems/");
  ASSERT_EQ(0, (int)filter.size());

  filter.clear();
  m_xmlParser->getDataItems(filter, R"(//Rotary[@name="C"]//DataItem[@type="LOAD"])");
  ASSERT_EQ(2, (int)filter.size());

  filter.clear();
  m_xmlParser->getDataItems(
      filter, R"(//Rotary[@name="C"]//DataItem[@category="CONDITION" or @category="SAMPLE"])");
  ASSERT_EQ(5, (int)filter.size());
}

TEST_F(XmlParserTest, GetDataItemsExt)
{
  std::set<string> filter;

  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  // For the rest we will check with the extended schema
  try
  {
    std::unique_ptr<XmlPrinter> printer(new XmlPrinter());
    m_xmlParser = new XmlParser();
    m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/extension.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR << "/samples/extension.xml";
  }

  filter.clear();
  m_xmlParser->getDataItems(filter, "//Device//Pump");
  ASSERT_EQ(0, (int)filter.size());

  filter.clear();
  m_xmlParser->getDataItems(filter, "//Device//x:Pump");
  ASSERT_EQ(1, (int)filter.size());
}

TEST_F(XmlParserTest, ExtendedSchema)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  try
  {
    std::unique_ptr<XmlPrinter> printer(new XmlPrinter());
    m_xmlParser = new XmlParser();
    m_devices = m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/extension.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR << "/samples/extension.xml";
  }

  ASSERT_EQ((size_t)1, m_devices.size());

  const auto device = m_devices.front();

  // Check for Description
  ASSERT_EQ((string) "Extended Schema.", device->getDescriptionBody());

  mtconnect::Component *pump = device->getChildren().front();
  ASSERT_EQ((string) "pump", pump->getName());
  ASSERT_EQ((string) "Pump", pump->getClass());
  ASSERT_EQ((string) "x", pump->getPrefix());

  DataItem *item = pump->getDataItems().front();
  ASSERT_EQ((string) "x:FLOW", item->getType());
  ASSERT_EQ((string) "Flow", item->getElementName());
  ASSERT_EQ((string) "x", item->getPrefix());
}

TEST_F(XmlParserTest, TimeSeries)
{
  const auto dev = m_devices[0];
  ASSERT_TRUE(dev);

  auto item = dev->getDeviceDataItem("Xact");
  ASSERT_TRUE(item);

  item->getAttributes();
  ASSERT_EQ((string) "AVERAGE", item->getStatistic());

  const auto &attrs1 = item->getAttributes();
  ASSERT_EQ(string("AVERAGE"), attrs1.at("statistic"));

  item = dev->getDeviceDataItem("Xts");
  ASSERT_TRUE(item);
  item->getAttributes();
  ASSERT_TRUE(item->isTimeSeries());
  ASSERT_EQ(DataItem::TIME_SERIES, item->getRepresentation());

  const auto &attrs2 = item->getAttributes();
  ASSERT_EQ(string("TIME_SERIES"), attrs2.at("representation"));
}

TEST_F(XmlParserTest, Configuration)
{
  const auto dev = m_devices[0];
  ASSERT_TRUE(dev);

  mtconnect::Component *power = nullptr;
  const auto &children = dev->getChildren();

  for (auto const &iter : children)
  {
    if (iter->getName() == "power")
      power = iter;
  }

  ASSERT_TRUE(power);
  ASSERT_FALSE(power->getConfiguration().empty());
}

TEST_F(XmlParserTest, ParseAsset)
{
  auto document = getFile("asset1.xml");
  AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
  CuttingToolPtr tool = (CuttingTool *)asset.getObject();

  ASSERT_EQ((string) "KSSP300R4SD43L240", tool->getIdentity().at("toolId"));
  ASSERT_EQ((string) "KSSP300R4SD43L240.1", tool->getAssetId());
  ASSERT_EQ((string) "1", tool->getIdentity().at("serialNumber"));
  ASSERT_EQ((string) "KMT,Parlec", tool->getIdentity().at("manufacturers"));
  ASSERT_EQ((string) "2011-05-11T13:55:22", tool->getTimestamp());
  ASSERT_EQ(false, tool->isRemoved());

  // Top Level
  ASSERT_EQ((string) "ISO 13399...", tool->m_values.at("CuttingToolDefinition")->m_value);
  ASSERT_EQ((string) "EXPRESS",
            tool->m_values.at("CuttingToolDefinition")->m_properties.at("format"));
  ASSERT_EQ((string) "Cutting tool ...", tool->getDescription());

  // Status
  ASSERT_EQ((string) "NEW", tool->m_status[0]);

  // Values
  ASSERT_EQ((string) "10000", tool->m_values.at("ProcessSpindleSpeed")->m_value);
  ASSERT_EQ((string) "222", tool->m_values.at("ProcessFeedRate")->m_value);
  ASSERT_EQ((unsigned int)1, tool->m_values.at("ProcessFeedRate")->refCount());

  // Measurements
  ASSERT_EQ((string) "73.25", tool->m_measurements.at("BodyDiameterMax")->m_value);
  ASSERT_EQ((string) "76.2", tool->m_measurements.at("CuttingDiameterMax")->m_value);
  ASSERT_EQ((unsigned int)1, tool->m_measurements.at("BodyDiameterMax")->refCount());

  // Items
  ASSERT_EQ((string) "24", tool->m_itemCount);

  // Item
  ASSERT_EQ((size_t)6, tool->m_items.size());
  CuttingItemPtr item = tool->m_items[0];
  ASSERT_EQ((unsigned int)2, item->refCount());

  ASSERT_EQ((string) "SDET43PDER8GB", item->m_identity.at("itemId"));
  ASSERT_EQ((string) "FLANGE: 1-4, ROW: 1", item->m_values.at("Locus")->m_value);
  ASSERT_EQ((string) "12.7", item->m_measurements.at("CuttingEdgeLength")->m_value);
  ASSERT_EQ((unsigned int)1, item->m_measurements.at("CuttingEdgeLength")->refCount());
}

TEST_F(XmlParserTest, ParseOtherAsset)
{
  string document =
      "<Workpiece assetId=\"XXX123\" timestamp=\"2014-04-14T01:22:33.123\" "
      "serialNumber=\"A1234\" deviceUuid=\"XXX\" >Data</Workpiece>";
  std::unique_ptr<XmlPrinter> printer(new XmlPrinter());
  AssetPtr asset = m_xmlParser->parseAsset("XXX", "Workpiece", document);

  ASSERT_TRUE(asset.getObject());
  ASSERT_EQ((string) "XXX123", asset->getAssetId());
  ASSERT_EQ((string) "2014-04-14T01:22:33.123", asset->getTimestamp());
  ASSERT_EQ((string) "XXX", asset->getDeviceUuid());
  ASSERT_EQ((string) "Data", asset->getContent(printer.get()));
  ASSERT_EQ(false, asset->isRemoved());

  document =
      "<Workpiece assetId=\"XXX123\" timestamp=\"2014-04-14T01:22:33.123\" "
      "serialNumber=\"A1234\" deviceUuid=\"XXX\" removed=\"true\">Data</Workpiece>";
  asset = m_xmlParser->parseAsset("XXX", "Workpiece", document);

  ASSERT_TRUE(asset.getObject());
  ASSERT_EQ(true, asset->isRemoved());
}

TEST_F(XmlParserTest, ParseRemovedAsset)
{
  auto document = getFile("asset3.xml");
  AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
  CuttingToolPtr tool = (CuttingTool *)asset.getObject();

  ASSERT_EQ(true, tool->isRemoved());
}

TEST_F(XmlParserTest, UpdateAsset)
{
  auto document = getFile("asset1.xml");
  AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
  CuttingToolPtr tool = (CuttingTool *)asset.getObject();

  string replacement =
      "<CuttingDiameterMax code=\"DC\" nominal=\"76.2\" maximum=\"76.213\" "
      "minimum=\"76.187\">10.123</CuttingDiameterMax>";
  m_xmlParser->updateAsset(asset, "CuttingTool", replacement);

  CuttingItemPtr item = tool->m_items[0];
  ASSERT_EQ((string) "10.123", tool->m_measurements.at("CuttingDiameterMax")->m_value);

  // Test cutting item replacement
  ASSERT_EQ((string) "12.7", item->m_measurements.at("CuttingEdgeLength")->m_value);

  replacement =
      "<CuttingItem indices=\"1-4\" itemId=\"SDET43PDER8GB\" manufacturers=\"KMT\" "
      "grade=\"KC725M\">"
      "<Locus>FLANGE: 1-4, ROW: 1</Locus>"
      "<Measurements>"
      "<CuttingEdgeLength code=\"L\" nominal=\"12.7\" minimum=\"12.675\" "
      "maximum=\"12.725\">14.7</CuttingEdgeLength>"
      "<WiperEdgeLength code=\"BS\" nominal=\"2.56\">2.56</WiperEdgeLength>"
      "<IncribedCircleDiameter code=\"IC\" nominal=\"12.7\">12.7</IncribedCircleDiameter>"
      "<CornerRadius code=\"RE\" nominal=\"0.8\">0.8</CornerRadius>"
      "</Measurements>"
      "</CuttingItem>";

  m_xmlParser->updateAsset(asset, "CuttingTool", replacement);

  item = tool->m_items[0];
  ASSERT_EQ((string) "14.7", item->m_measurements.at("CuttingEdgeLength")->m_value);
}

TEST_F(XmlParserTest, BadAsset)
{
  auto xml = getFile("asset4.xml");

  auto asset = m_xmlParser->parseAsset("XXX", "CuttingTool", xml);
  ASSERT_TRUE(!asset);
}

TEST_F(XmlParserTest, NoNamespace)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  unique_ptr<XmlPrinter> printer(new XmlPrinter());
  m_xmlParser = new XmlParser();
  ASSERT_NO_THROW(
      m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/NoNamespace.xml", printer.get()));
}

TEST_F(XmlParserTest, FilteredDataItem13)
{
  delete m_xmlParser;
  m_xmlParser = nullptr;
  try
  {
    unique_ptr<XmlPrinter> printer(new XmlPrinter());
    m_xmlParser = new XmlParser();
    m_devices =
        m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/filter_example_1.3.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR
           << "/samples/filter_example_1.3.xml";
  }

  Device *dev = m_devices[0];
  DataItem *di = dev->getDeviceDataItem("c1");

  ASSERT_EQ(di->getFilterValue(), 5.0);
  ASSERT_TRUE(di->hasMinimumDelta());
}

TEST_F(XmlParserTest, FilteredDataItem)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  try
  {
    unique_ptr<XmlPrinter> printer(new XmlPrinter());
    m_xmlParser = new XmlParser();
    m_devices =
        m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/filter_example.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR << "/samples/filter_example.xml";
  }

  auto di = m_devices[0]->getDeviceDataItem("c1");

  ASSERT_EQ(di->getFilterValue(), 5.0);
  ASSERT_TRUE(di->hasMinimumDelta());
  di = m_devices[0]->getDeviceDataItem("c2");

  ASSERT_EQ(di->getFilterPeriod(), 10.0);
  ASSERT_TRUE(di->hasMinimumPeriod());
}

TEST_F(XmlParserTest, References)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  try
  {
    unique_ptr<XmlPrinter> printer(new XmlPrinter());
    m_xmlParser = new XmlParser();
    m_devices =
        m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/reference_example.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR << "/samples/reference_example.xml";
  }

  string id = "mf";
  const auto item = m_devices[0]->getDeviceDataItem(id);
  const auto comp = item->getComponent();

  comp->resolveReferences();

  const auto refs = comp->getReferences();
  const auto &ref = refs[0];

  ASSERT_EQ((string) "c4", ref.m_id);
  ASSERT_EQ((string) "chuck", ref.m_name);

  ASSERT_TRUE(ref.m_dataItem) << "DataItem was not resolved.";

  const auto &ref2 = refs[1];
  ASSERT_EQ((string) "d2", ref2.m_id);
  ASSERT_EQ((string) "door", ref2.m_name);

  ASSERT_TRUE(ref2.m_dataItem) << "DataItem was not resolved.";

  const auto &ref3 = refs[2];
  ASSERT_EQ((string) "ele", ref3.m_id);
  ASSERT_EQ((string) "electric", ref3.m_name);

  ASSERT_TRUE(ref3.m_component) << "DataItem was not resolved.";

  std::set<string> filter;
  m_xmlParser->getDataItems(filter, "//BarFeederInterface");

  ASSERT_EQ((size_t)5, filter.size());
  ASSERT_EQ((size_t)1, filter.count("mf"));
  ASSERT_EQ((size_t)1, filter.count("c4"));
  ASSERT_EQ((size_t)1, filter.count("bfc"));
  ASSERT_EQ((size_t)1, filter.count("d2"));
  ASSERT_EQ((size_t)1, filter.count("eps"));
}

TEST_F(XmlParserTest, SourceReferences)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  try
  {
    unique_ptr<XmlPrinter> printer(new XmlPrinter());
    m_xmlParser = new XmlParser();
    m_devices =
        m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/reference_example.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR << "/samples/reference_example.xml";
  }

  const auto item = m_devices[0]->getDeviceDataItem("bfc");
  ASSERT_TRUE(item != nullptr);

  ASSERT_EQ(string(""), item->getSource());
  ASSERT_EQ(string("mf"), item->getSourceDataItemId());
  ASSERT_EQ(string("ele"), item->getSourceComponentId());
  ASSERT_EQ(string("xxx"), item->getSourceCompositionId());
}

TEST_F(XmlParserTest, ExtendedAsset)
{
  auto document = getFile("ext_asset.xml");
  AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
  CuttingToolPtr tool = (CuttingTool *)asset.getObject();

  ASSERT_EQ(((size_t)1), tool->m_values.count("x:Color"));
}

TEST_F(XmlParserTest, ExtendedAssetFragment)
{
  auto document = getFile("ext_asset_2.xml");
  AssetPtr asset = m_xmlParser->parseAsset("XXX", "CuttingTool", document);
  CuttingToolPtr tool = (CuttingTool *)asset.getObject();

  ASSERT_EQ(((size_t)1), tool->m_values.count("x:Color"));
}

TEST_F(XmlParserTest, DataItemRelationships)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  unique_ptr<XmlPrinter> printer(new XmlPrinter());
  m_xmlParser = new XmlParser();
  m_devices = m_xmlParser->parseFile(PROJECT_ROOT_DIR "/samples/relationship_test.xml", printer.get());
  
  const auto &device = m_devices.front();
  auto &dataItemsMap = device->getDeviceDataItems();
  
  const auto item1 = dataItemsMap.at("xlc");
  ASSERT_TRUE(item1 != nullptr);
  
  const auto &relations = item1->getRelationships();
  
  ASSERT_EQ((size_t) 2, relations.size());

  auto rel = relations.begin();
  ASSERT_EQ(string("DataItemRelationship"),
	    rel->m_relation);
  ASSERT_EQ(string("LIMIT"),
	    rel->m_type);
  ASSERT_EQ(string("archie"),
	    rel->m_name);
  ASSERT_EQ(string("xlcpl"),
	    rel->m_idRef);
  
  rel++;
  ASSERT_EQ(string("SpecificationRelationship"),
	    rel->m_relation);
  ASSERT_EQ(string("LIMIT"),
	    rel->m_type);
  ASSERT_TRUE(rel->m_name.empty());
  ASSERT_EQ(string("spec1"),
	    rel->m_idRef);
  
  const auto item2 = dataItemsMap.at("xlcpl");
  ASSERT_TRUE(item2 != nullptr);
  
  const auto &relations2 = item2->getRelationships();
  
  ASSERT_EQ((size_t) 1, relations2.size());
  
  auto rel2 = relations2.begin();
  ASSERT_EQ(string("DataItemRelationship"), rel2->m_relation);
  ASSERT_EQ(string("OBSERVATION"), rel2->m_type);
  ASSERT_EQ(string("bob"), rel2->m_name);
  ASSERT_EQ(string("xlc"), rel2->m_idRef);
 
}

TEST_F(XmlParserTest, ParseDeviceMTConnectVersion)
{
  const auto dev = m_devices[0];
  ASSERT_TRUE(dev);

  ASSERT_EQ(string("1.7"), dev->getMTConnectVersion());
}
