//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <fstream>
#include <iostream>
#include <stdexcept>

#include "mtconnect/device_model/reference.hpp"
#include "mtconnect/parser/xml_parser.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "test_utilities.hpp"

using namespace std;
using namespace std::literals;
using namespace mtconnect;
using namespace entity;
using namespace device_model;
using namespace data_item;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class XmlParserTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_xmlParser = nullptr;

    try
    {
      std::unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
      m_xmlParser = new parser::XmlParser();
      m_devices =
          m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/test_config.xml", printer.get());
    }
    catch (exception &)
    {
      FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR
             << " /test/resources/samples/test_config.xml";
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

  parser::XmlParser *m_xmlParser {nullptr};
  std::list<DevicePtr> m_devices;
};

TEST_F(XmlParserTest, Constructor)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  std::unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
  m_xmlParser = new parser::XmlParser();
  ASSERT_THROW(m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/badPath.xml", printer.get()),
               std::runtime_error);
  delete m_xmlParser;
  m_xmlParser = nullptr;
  m_xmlParser = new parser::XmlParser();
  ASSERT_NO_THROW(
      m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/test_config.xml", printer.get()));
}

TEST_F(XmlParserTest, GetDevices)
{
  ASSERT_EQ((size_t)1, m_devices.size());

  const auto device = m_devices.front();

  // Check for Description
  auto &description = device->get<EntityPtr>("Description");
  ASSERT_TRUE(description);
  ASSERT_EQ((string) "Linux CNC Device", description->getValue<string>());

  list<DataItemPtr> dataItems;
  const auto &dataItemsMap = device->getDeviceDataItems();

  for (auto const &mapItem : dataItemsMap)
    dataItems.emplace_back(mapItem.lock());

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
  const auto &dataItemsMap = device->getDeviceDataItems();
  const auto item = dataItemsMap.find("clc")->lock();
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
    std::unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
    m_xmlParser = new parser::XmlParser();
    m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/extension.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR
           << "/test/resources/samples/extension.xml";
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
    std::unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
    m_xmlParser = new parser::XmlParser();
    m_devices = m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/extension.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR
           << "/test/resources/samples/extension.xml";
  }

  ASSERT_EQ((size_t)1, m_devices.size());

  const auto device = m_devices.front();

  // Check for Description
  auto &description = device->get<EntityPtr>("Description");
  ASSERT_TRUE(description);
  ASSERT_EQ((string) "Extended Schema.", description->getValue<string>());

  ComponentPtr pump = dynamic_pointer_cast<Component>(device->getList("Components")->front());
  ASSERT_EQ((string) "pump", pump->get<string>("name"));
  ASSERT_EQ((string) "Pump", string(pump->getName().getName()));
  ASSERT_EQ((string) "x", pump->getName().getNs());

  auto item = dynamic_pointer_cast<DataItem>(pump->getList("DataItems")->front());
  ASSERT_EQ((string) "x:FLOW", item->getType());
  ASSERT_EQ((string) "Flow", string(item->getObservationName().getName()));
  ASSERT_EQ((string) "x", item->getObservationName().getNs());
}

TEST_F(XmlParserTest, TimeSeries)
{
  const auto dev = m_devices.front();
  ASSERT_TRUE(dev);

  auto item = dev->getDeviceDataItem("Xact");
  ASSERT_TRUE(item);

  ASSERT_EQ((string) "AVERAGE", item->get<string>("statistic"));

  item = dev->getDeviceDataItem("Xts");
  ASSERT_TRUE(item);
  ASSERT_TRUE(item->isTimeSeries());
  ASSERT_EQ(DataItem::TIME_SERIES, item->getRepresentation());
  ASSERT_EQ((string) "TIME_SERIES", item->get<string>("representation"));
}

TEST_F(XmlParserTest, Configuration)
{
  const auto dev = m_devices.front();
  ASSERT_TRUE(dev);

  EntityPtr power;
  const auto &children = dev->getChildren();

  for (auto const &iter : *children)
  {
    if (iter->get<string>("name") == "power")
      power = iter;
  }

  ASSERT_TRUE(power);
  ASSERT_TRUE(power->hasProperty("Configuration"));
}

TEST_F(XmlParserTest, NoNamespace)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
  m_xmlParser = new parser::XmlParser();
  ASSERT_NO_THROW(
      m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/NoNamespace.xml", printer.get()));
}

TEST_F(XmlParserTest, FilteredDataItem13)
{
  delete m_xmlParser;
  m_xmlParser = nullptr;
  try
  {
    unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
    m_xmlParser = new parser::XmlParser();
    m_devices =
        m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/filter_example_1.3.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR
           << "/test/resources/samples/filter_example_1.3.xml";
  }

  DevicePtr dev = m_devices.front();
  DataItemPtr di = dev->getDeviceDataItem("c1");

  ASSERT_TRUE(di->getMinimumDelta());
  ASSERT_EQ(5.0, *di->getMinimumDelta());
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
    unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
    m_xmlParser = new parser::XmlParser();
    m_devices =
        m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/filter_example.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR
           << "/test/resources/samples/filter_example.xml";
  }

  auto di = m_devices.front()->getDeviceDataItem("c1");

  ASSERT_TRUE(di->getMinimumDelta());
  ASSERT_EQ(5.0, *di->getMinimumDelta());
  di = m_devices.front()->getDeviceDataItem("c2");

  ASSERT_TRUE(di->getMinimumPeriod());
  ASSERT_EQ(10.0, di->getMinimumPeriod());
}

TEST_F(XmlParserTest, References)
{
  using namespace device_model;

  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  try
  {
    unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
    m_xmlParser = new parser::XmlParser();
    m_devices =
        m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/reference_example.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR
           << "/test/resources/samples/reference_example.xml";
  }

  string id = "mf";
  const auto item = m_devices.front()->getDeviceDataItem(id);
  const auto comp = item->getComponent();

  auto refs = comp->getList("References");
  ASSERT_TRUE(refs);
  auto ref = refs->begin();

  ASSERT_EQ((string) "c4", (*ref)->get<string>("idRef"));
  ASSERT_EQ((string) "chuck", (*ref)->get<string>("name"));

  auto r = dynamic_pointer_cast<Reference>(*ref);
  ASSERT_TRUE(r);
  ASSERT_TRUE(r->getDataItem().lock()) << "DataItem was not resolved.";

  ref++;
  ASSERT_EQ((string) "d2", (*ref)->get<string>("idRef"));
  ASSERT_EQ((string) "door", (*ref)->get<string>("name"));

  r = dynamic_pointer_cast<Reference>(*ref);
  ASSERT_TRUE(r);
  ASSERT_TRUE(r->getDataItem().lock()) << "DataItem was not resolved.";

  ref++;
  ASSERT_EQ((string) "ele", (*ref)->get<string>("idRef"));
  ASSERT_EQ((string) "electric", (*ref)->get<string>("name"));

  r = dynamic_pointer_cast<Reference>(*ref);
  ASSERT_TRUE(r);
  ASSERT_TRUE(r->getComponent().lock()) << "Component was not resolved.";

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
    unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
    m_xmlParser = new parser::XmlParser();
    m_devices =
        m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/reference_example.xml", printer.get());
  }
  catch (exception &)
  {
    FAIL() << "Could not locate test xml: " << PROJECT_ROOT_DIR
           << "/test/resources/samples/reference_example.xml";
  }

  const auto item = m_devices.front()->getDeviceDataItem("bfc");
  ASSERT_TRUE(item != nullptr);

  auto source = item->maybeGet<EntityPtr>("Source");
  ASSERT_TRUE(source);
  ASSERT_FALSE((*source)->maybeGetValue<string>());
  ASSERT_EQ("mf", (*source)->get<string>("dataItemId"));
  ASSERT_EQ("ele", (*source)->get<string>("componentId"));
  ASSERT_EQ("xxx", (*source)->get<string>("compositionId"));
}

TEST_F(XmlParserTest, DataItemRelationships)
{
  if (m_xmlParser)
  {
    delete m_xmlParser;
    m_xmlParser = nullptr;
  }

  unique_ptr<printer::XmlPrinter> printer(new printer::XmlPrinter());
  m_xmlParser = new parser::XmlParser();
  m_devices =
      m_xmlParser->parseFile(TEST_RESOURCE_DIR "/samples/relationship_test.xml", printer.get());

  const auto &device = m_devices.front();
  const auto &dataItemsMap = device->getDeviceDataItems();

  const auto item1 = dataItemsMap.find("xlc")->lock();
  ASSERT_TRUE(item1 != nullptr);

  const auto &relations = item1->getList("Relationships");
  ASSERT_TRUE(relations);

  ASSERT_EQ((size_t)2, relations->size());

  auto rel = relations->begin();
  ASSERT_EQ(string("DataItemRelationship"), (*rel)->getName());
  ASSERT_EQ(string("LIMIT"), (*rel)->get<string>("type"));
  ASSERT_EQ(string("archie"), (*rel)->get<string>("name"));
  ASSERT_EQ(string("xlcpl"), (*rel)->get<string>("idRef"));

  rel++;
  ASSERT_EQ(string("SpecificationRelationship"), (*rel)->getName());
  ASSERT_EQ(string("LIMIT"), (*rel)->get<string>("type"));
  ASSERT_FALSE((*rel)->maybeGet<string>("name"));
  ASSERT_EQ(string("spec1"), (*rel)->get<string>("idRef"));

  const auto item2 = dataItemsMap.find("xlcpl")->lock();
  ASSERT_TRUE(item2 != nullptr);

  const auto &relations2 = item2->getList("Relationships");

  ASSERT_EQ((size_t)1, relations2->size());

  auto rel2 = relations2->begin();
  ASSERT_EQ(string("DataItemRelationship"), (*rel2)->getName());
  ASSERT_EQ(string("OBSERVATION"), (*rel2)->get<string>("type"));
  ASSERT_EQ(string("bob"), (*rel2)->get<string>("name"));
  ASSERT_EQ(string("xlc"), (*rel2)->get<string>("idRef"));
}

TEST_F(XmlParserTest, ParseDeviceMTConnectVersion)
{
  const auto dev = m_devices.front();
  ASSERT_TRUE(dev);

  ASSERT_EQ(string("1.7"), dev->get<string>("mtconnectVersion"));
}
