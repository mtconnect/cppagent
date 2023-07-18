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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/asset/cutting_tool.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/json_printer.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/entity/xml_printer.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/printer//xml_printer_helper.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;
using namespace mtconnect::source::adapter;
using namespace mtconnect::asset;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class CuttingToolTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.7", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");

    // Asset types are registered in the agent.
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");

    m_writer = make_unique<printer::XmlWriter>(true);
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
    m_writer.reset();
  }

  void addAdapter() { m_agentTestHelper->addAdapter(); }

  std::string m_agentId;
  DevicePtr m_device {nullptr};

  std::unique_ptr<printer::XmlWriter> m_writer;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(CuttingToolTest, TestMinmalArchetype)
{
  const auto doc =
      R"DOC(<CuttingToolArchetype assetId="M8010N9172N:1.0" toolId="CAT">
  <CuttingToolLifeCycle>
    <ToolLife countDirection="UP" initial="0" limit="100" type="MINUTES"/>
    <ToolLife countDirection="DOWN" initial="25" limit="1" type="PART_COUNT"/>
    <ProgramToolGroup>A</ProgramToolGroup>
    <ProgramToolNumber>10</ProgramToolNumber>
  </CuttingToolLifeCycle>
</CuttingToolArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("CAT", get<string>(entity->getProperty("toolId")));
  ASSERT_EQ("M8010N9172N:1.0", asset->getAssetId());

  ASSERT_FALSE(asset->getTimestamp());
  ASSERT_FALSE(asset->getDeviceUuid());

  auto lifeCycle = get<EntityPtr>(asset->getProperty("CuttingToolLifeCycle"));
  ASSERT_TRUE(lifeCycle);

  ASSERT_EQ("A", get<string>(lifeCycle->getProperty("ProgramToolGroup")));
  ASSERT_EQ("10", get<string>(lifeCycle->getProperty("ProgramToolNumber")));

  auto life = get<EntityList>(lifeCycle->getProperty("ToolLife"));
  ASSERT_EQ(2, life.size());

  auto it = life.begin();
  ASSERT_EQ("ToolLife", (*it)->getName());
  ASSERT_EQ("MINUTES", get<string>((*it)->getProperty("type")));
  ASSERT_EQ("UP", get<string>((*it)->getProperty("countDirection")));
  ASSERT_EQ(0.0, get<entity::DOUBLE>((*it)->getProperty("initial")));
  ASSERT_EQ(100.0, get<entity::DOUBLE>((*it)->getProperty("limit")));

  it++;
  ASSERT_EQ("ToolLife", (*it)->getName());
  ASSERT_EQ("PART_COUNT", get<string>((*it)->getProperty("type")));
  ASSERT_EQ("DOWN", get<string>((*it)->getProperty("countDirection")));
  ASSERT_EQ(25.0, get<entity::DOUBLE>((*it)->getProperty("initial")));
  ASSERT_EQ(1.0, get<entity::DOUBLE>((*it)->getProperty("limit")));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(CuttingToolTest, TestMeasurements)
{
  const auto doc =
      R"DOC(<CuttingToolArchetype assetId="M8010N9172N:1.0" toolId="CAT">
  <CuttingToolLifeCycle>
    <Measurements>
      <FunctionalLength code="LF" maximum="5.2" minimum="4.95" nominal="5" units="MILLIMETER"/>
      <CuttingDiameterMax code="DC" maximum="1.4" minimum="0.95" nominal="1.25" units="MILLIMETER"/>
    </Measurements>
  </CuttingToolLifeCycle>
</CuttingToolArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("CAT", get<string>(entity->getProperty("toolId")));
  ASSERT_EQ("M8010N9172N:1.0", asset->getAssetId());

  ASSERT_FALSE(asset->getTimestamp());
  ASSERT_FALSE(asset->getDeviceUuid());

  auto lifeCycle = get<EntityPtr>(asset->getProperty("CuttingToolLifeCycle"));
  ASSERT_TRUE(lifeCycle);

  auto meas = lifeCycle->getList("Measurements");
  ASSERT_TRUE(meas);
  ASSERT_EQ(2, meas->size());

  auto it = meas->begin();
  ASSERT_EQ("FunctionalLength", (*it)->getName());
  ASSERT_EQ("LF", get<string>((*it)->getProperty("code")));
  ASSERT_EQ("MILLIMETER", get<string>((*it)->getProperty("units")));
  ASSERT_EQ(5.0, get<entity::DOUBLE>((*it)->getProperty("nominal")));
  ASSERT_EQ(4.95, get<entity::DOUBLE>((*it)->getProperty("minimum")));
  ASSERT_EQ(5.2, get<entity::DOUBLE>((*it)->getProperty("maximum")));

  it++;
  ASSERT_EQ("CuttingDiameterMax", (*it)->getName());
  ASSERT_EQ("DC", get<string>((*it)->getProperty("code")));
  ASSERT_EQ("MILLIMETER", get<string>((*it)->getProperty("units")));
  ASSERT_EQ(1.25, get<entity::DOUBLE>((*it)->getProperty("nominal")));
  ASSERT_EQ(0.95, get<entity::DOUBLE>((*it)->getProperty("minimum")));
  ASSERT_EQ(1.4, get<entity::DOUBLE>((*it)->getProperty("maximum")));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(CuttingToolTest, TestItems)
{
  const auto doc =
      R"DOC(<CuttingToolArchetype assetId="M8010N9172N:1.0" toolId="CAT">
  <CuttingToolLifeCycle>
    <CuttingItems count="2">
      <CuttingItem grade="KC725M" indices="1-4" itemId="SDET43PDER8GB" manufacturers="KMT">
        <Locus>FLANGE: 1-4, ROW: 1</Locus>
        <Measurements>
          <CuttingEdgeLength code="L" maximum="12.725" minimum="12.675" nominal="12.7"/>
          <WiperEdgeLength code="BS" nominal="2.56"/>
          <IncribedCircleDiameter code="IC" nominal="12.7"/>
          <CornerRadius code="RE" nominal="0.8"/>
        </Measurements>
      </CuttingItem>
      <CuttingItem grade="KC725M" indices="5-8" itemId="SDET43PDER8GB" manufacturers="KMT">
        <Locus>FLANGE: 1-4, ROW: 2</Locus>
        <Measurements>
          <CuttingEdgeLength code="L" maximum="12.725" minimum="12.675" nominal="12.7"/>
          <WiperEdgeLength code="BS" nominal="2.56"/>
          <IncribedCircleDiameter code="IC" nominal="12.7"/>
          <CornerRadius code="RE" nominal="0.8"/>
        </Measurements>
      </CuttingItem>
    </CuttingItems>
  </CuttingToolLifeCycle>
</CuttingToolArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("CAT", get<string>(entity->getProperty("toolId")));
  ASSERT_EQ("M8010N9172N:1.0", asset->getAssetId());

  ASSERT_FALSE(asset->getTimestamp());
  ASSERT_FALSE(asset->getDeviceUuid());

  auto lifeCycle = get<EntityPtr>(asset->getProperty("CuttingToolLifeCycle"));
  ASSERT_TRUE(lifeCycle);

  auto items = get<EntityPtr>(lifeCycle->getProperty("CuttingItems"));
  ASSERT_EQ(2, get<int64_t>(items->getProperty("count")));

  auto itemList = lifeCycle->getList("CuttingItems");
  ASSERT_EQ(2, itemList->size());

  auto it = itemList->begin();
  ASSERT_EQ("CuttingItem", (*it)->getName());
  ASSERT_EQ("1-4", get<string>((*it)->getProperty("indices")));
  ASSERT_EQ("SDET43PDER8GB", get<string>((*it)->getProperty("itemId")));
  ASSERT_EQ("KMT", get<string>((*it)->getProperty("manufacturers")));
  ASSERT_EQ("KC725M", get<string>((*it)->getProperty("grade")));
  ASSERT_EQ("FLANGE: 1-4, ROW: 1", get<string>((*it)->getProperty("Locus")));

  {
    auto meas = (*it)->getList("Measurements");
    ASSERT_TRUE(meas);
    ASSERT_EQ(4, meas->size());

    auto im = meas->begin();
    ASSERT_EQ("CuttingEdgeLength", (*im)->getName());
    ASSERT_EQ("L", get<string>((*im)->getProperty("code")));
    ASSERT_EQ(12.7, get<entity::DOUBLE>((*im)->getProperty("nominal")));
    ASSERT_EQ(12.675, get<entity::DOUBLE>((*im)->getProperty("minimum")));
    ASSERT_EQ(12.725, get<entity::DOUBLE>((*im)->getProperty("maximum")));

    im++;
    ASSERT_EQ("WiperEdgeLength", (*im)->getName());
    ASSERT_EQ("BS", get<string>((*im)->getProperty("code")));
    ASSERT_EQ(2.56, get<entity::DOUBLE>((*im)->getProperty("nominal")));

    im++;
    ASSERT_EQ("IncribedCircleDiameter", (*im)->getName());
    ASSERT_EQ("IC", get<string>((*im)->getProperty("code")));
    ASSERT_EQ(12.7, get<entity::DOUBLE>((*im)->getProperty("nominal")));

    im++;
    ASSERT_EQ("CornerRadius", (*im)->getName());
    ASSERT_EQ("RE", get<string>((*im)->getProperty("code")));
    ASSERT_EQ(0.8, get<entity::DOUBLE>((*im)->getProperty("nominal")));
  }

  it++;
  ASSERT_EQ("CuttingItem", (*it)->getName());
  ASSERT_EQ("5-8", get<string>((*it)->getProperty("indices")));
  ASSERT_EQ("SDET43PDER8GB", get<string>((*it)->getProperty("itemId")));
  ASSERT_EQ("KMT", get<string>((*it)->getProperty("manufacturers")));
  ASSERT_EQ("KC725M", get<string>((*it)->getProperty("grade")));
  ASSERT_EQ("FLANGE: 1-4, ROW: 2", get<string>((*it)->getProperty("Locus")));

  {
    auto meas = (*it)->getList("Measurements");
    ASSERT_TRUE(meas);
    ASSERT_EQ(4, meas->size());

    auto im = meas->begin();
    ASSERT_EQ("CuttingEdgeLength", (*im)->getName());
    ASSERT_EQ("L", get<string>((*im)->getProperty("code")));
    ASSERT_EQ(12.7, get<entity::DOUBLE>((*im)->getProperty("nominal")));
    ASSERT_EQ(12.675, get<entity::DOUBLE>((*im)->getProperty("minimum")));
    ASSERT_EQ(12.725, get<entity::DOUBLE>((*im)->getProperty("maximum")));

    im++;
    ASSERT_EQ("WiperEdgeLength", (*im)->getName());
    ASSERT_EQ("BS", get<string>((*im)->getProperty("code")));
    ASSERT_EQ(2.56, get<entity::DOUBLE>((*im)->getProperty("nominal")));

    im++;
    ASSERT_EQ("IncribedCircleDiameter", (*im)->getName());
    ASSERT_EQ("IC", get<string>((*im)->getProperty("code")));
    ASSERT_EQ(12.7, get<entity::DOUBLE>((*im)->getProperty("nominal")));

    im++;
    ASSERT_EQ("CornerRadius", (*im)->getName());
    ASSERT_EQ("RE", get<string>((*im)->getProperty("code")));
    ASSERT_EQ(0.8, get<entity::DOUBLE>((*im)->getProperty("nominal")));
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(CuttingToolTest, TestMinmalTool)
{
  const auto doc =
      R"DOC(<CuttingTool assetId="M8010N9172N:1.0" serialNumber="1234" toolId="CAT">
  <CuttingToolLifeCycle>
    <CutterStatus>
      <Status>NEW</Status>
    </CutterStatus>
    <ToolLife countDirection="DOWN" initial="25" limit="1" type="PART_COUNT">10</ToolLife>
    <ProgramToolGroup>A</ProgramToolGroup>
    <ProgramToolNumber>10</ProgramToolNumber>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("CAT", get<string>(entity->getProperty("toolId")));
  ASSERT_EQ("M8010N9172N:1.0", asset->getAssetId());

  ASSERT_FALSE(asset->getTimestamp());
  ASSERT_FALSE(asset->getDeviceUuid());

  auto lifeCycle = get<EntityPtr>(asset->getProperty("CuttingToolLifeCycle"));
  ASSERT_TRUE(lifeCycle);

  ASSERT_EQ("A", get<string>(lifeCycle->getProperty("ProgramToolGroup")));
  ASSERT_EQ("10", get<string>(lifeCycle->getProperty("ProgramToolNumber")));

  auto stl = lifeCycle->getList("CutterStatus");
  ASSERT_EQ(1, stl->size());
  auto st = stl->front();
  ASSERT_EQ("NEW", get<string>(st->getValue()));

  auto life = get<EntityList>(lifeCycle->getProperty("ToolLife"));
  ASSERT_EQ(1, life.size());

  auto it = life.begin();
  ASSERT_EQ("ToolLife", (*it)->getName());
  ASSERT_EQ("PART_COUNT", get<string>((*it)->getProperty("type")));
  ASSERT_EQ("DOWN", get<string>((*it)->getProperty("countDirection")));
  ASSERT_EQ(25.0, get<entity::DOUBLE>((*it)->getProperty("initial")));
  ASSERT_EQ(1.0, get<entity::DOUBLE>((*it)->getProperty("limit")));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(CuttingToolTest, TestMinmalToolError)
{
  const auto doc =
      R"DOC(<CuttingTool assetId="M8010N9172N:1.0" serialNumber="1234" toolId="CAT">
  <CuttingToolLifeCycle>
    <ToolLife countDirection="DOWN" initial="25" limit="1" type="PART_COUNT">10</ToolLife>
    <ProgramToolGroup>A</ProgramToolGroup>
    <ProgramToolNumber>10</ProgramToolNumber>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(2, errors.size());
  ASSERT_EQ(
      "CuttingToolLifeCycle(CutterStatus): Property CutterStatus is required and not provided",
      string(errors.front()->what()));
  ASSERT_EQ("CuttingTool: Invalid element 'CuttingToolLifeCycle'", string(errors.back()->what()));
}

TEST_F(CuttingToolTest, TestMeasurementsError)
{
  const auto doc =
      R"DOC(<CuttingTool assetId="M8010N9172N:1.0" serialNumber="1234" toolId="CAT">
  <CuttingToolLifeCycle>
    <CutterStatus>
      <Status>NEW</Status>
    </CutterStatus>
    <Measurements>
      <FunctionalLength code="LF" maximum="5.2" minimum="4.95" nominal="5" units="MILLIMETER"/>
      <CuttingDiameterMax code="DC" maximum="1.4" minimum="0.95" nominal="1.25" units="MILLIMETER"/>
    </Measurements>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(6, errors.size());
  auto it = errors.begin();
  EXPECT_EQ("FunctionalLength(VALUE): Property VALUE is required and not provided",
            string((*it)->what()));
  it++;
  EXPECT_EQ("Measurements: Invalid element 'FunctionalLength'", string((*it)->what()));
  it++;
  EXPECT_EQ("CuttingDiameterMax(VALUE): Property VALUE is required and not provided",
            string((*it)->what()));
  it++;
  EXPECT_EQ("Measurements: Invalid element 'CuttingDiameterMax'", string((*it)->what()));
  it++;
  EXPECT_EQ(
      "Measurements(Measurement): Entity list requirement Measurement must have at least 1 "
      "entries, 0 found",
      string((*it)->what()));
  it++;
  EXPECT_EQ("CuttingToolLifeCycle: Invalid element 'Measurements'", string((*it)->what()));
}

TEST_F(CuttingToolTest, AssetWithSimpleCuttingItems)
{
  auto printer = dynamic_cast<printer::XmlPrinter *>(m_agentTestHelper->m_agent->getPrinter("xml"));
  ASSERT_TRUE(printer != nullptr);

  printer->clearAssetsNamespaces();
  printer->addAssetsNamespace("urn:machine.com:MachineAssets:1.3",
                              "http://www.machine.com/schemas/MachineAssets_1.3.xsd", "x");

  addAdapter();

  m_agentTestHelper->m_adapter->parseBuffer("TIME|@ASSET@|XXX.200|CuttingTool|--multiline--AAAA\n");
  m_agentTestHelper->m_adapter->parseBuffer((getFile("asset5.xml") + "\n").c_str());
  m_agentTestHelper->m_adapter->parseBuffer("--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)1, m_agentTestHelper->m_agent->getAssetStorage()->getCount());

  {
    PARSE_XML_RESPONSE("/asset/XXX.200");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@type", "PART_COUNT");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@countDirection", "UP");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@initial", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@limit", "0");

    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:CutterStatus/m:Status",
                          "AVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='2']/m:CutterStatus/m:Status", "USED");

    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@type", "PART_COUNT");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@countDirection", "UP");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@initial", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@limit", "0");
  }
}

TEST_F(CuttingToolTest, test_extended_cutting_item)
{
  const auto doc =
      R"DOC(<CuttingTool assetId="123456.10" serialNumber="10" toolId="123456">
  <CuttingToolLifeCycle>
    <CutterStatus>
      <Status>AVAILABLE</Status>
    </CutterStatus>
    <ProgramToolNumber>10</ProgramToolNumber>
    <Location negativeOverlap="0" positiveOverlap="0" type="POT">13</Location>
    <CuttingItems count="12">
      <CuttingItem indices="1">
        <ItemLife countDirection="UP" initial="0" limit="0" type="PART_COUNT">0</ItemLife>
        <ItemLife countDirection="UP" initial="0" limit="0" type="MINUTES">0</ItemLife>
        <ItemLife countDirection="UP" initial="0" limit="0" type="WEAR">0</ItemLife>
        <x:ItemCutterStatus xmlns:x="okuma.com:OkumaToolAssets">
          <Status>AVAILABLE</Status>
        </x:ItemCutterStatus>
        <x:ItemProgramToolGroup xmlns:x="okuma.com:OkumaToolAssets">0</x:ItemProgramToolGroup>
      </CuttingItem>
    </CuttingItems>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("123456", get<string>(entity->getProperty("toolId")));
  ASSERT_EQ("123456.10", asset->getAssetId());

  auto lifeCycle = get<EntityPtr>(asset->getProperty("CuttingToolLifeCycle"));
  ASSERT_TRUE(lifeCycle);

  ASSERT_EQ("10", get<string>(lifeCycle->getProperty("ProgramToolNumber")));

  auto itemList = lifeCycle->getList("CuttingItems");
  ASSERT_EQ(1, itemList->size());

  auto &item = *itemList->begin();
  ASSERT_EQ("1", get<string>(item->getProperty("indices")));

  auto life = get<EntityList>(item->getProperty("ItemLife"));
  ASSERT_EQ(3, life.size());

  auto cutterStatus = get<EntityPtr>(item->getProperty("x:ItemCutterStatus"));
  ASSERT_TRUE(cutterStatus);
  ASSERT_EQ("okuma.com:OkumaToolAssets", get<string>(cutterStatus->getProperty("xmlns:x")));

  ASSERT_EQ("AVAILABLE", cutterStatus->get<string>("Status"));

  auto toolGroup = get<EntityPtr>(item->getProperty("x:ItemProgramToolGroup"));
  ASSERT_TRUE(toolGroup);
  ASSERT_EQ("okuma.com:OkumaToolAssets", get<string>(toolGroup->getProperty("xmlns:x")));

  ASSERT_EQ("0", toolGroup->getValue<string>());

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);

  entity::JsonEntityPrinter jprinter(1, true);

  auto sdoc = jprinter.print(entity);

  EXPECT_EQ(R"({
  "CuttingTool": {
    "CuttingToolLifeCycle": {
      "CutterStatus": [
        {
          "Status": {
            "value": "AVAILABLE"
          }
        }
      ],
      "CuttingItems": {
        "list": [
          {
            "CuttingItem": {
              "ItemLife": [
                {
                  "value": 0.0,
                  "countDirection": "UP",
                  "initial": 0.0,
                  "limit": 0.0,
                  "type": "PART_COUNT"
                },
                {
                  "value": 0.0,
                  "countDirection": "UP",
                  "initial": 0.0,
                  "limit": 0.0,
                  "type": "MINUTES"
                },
                {
                  "value": 0.0,
                  "countDirection": "UP",
                  "initial": 0.0,
                  "limit": 0.0,
                  "type": "WEAR"
                }
              ],
              "indices": "1",
              "x:ItemCutterStatus": {
                "Status": "AVAILABLE",
                "xmlns:x": "okuma.com:OkumaToolAssets"
              },
              "x:ItemProgramToolGroup": {
                "value": "0",
                "xmlns:x": "okuma.com:OkumaToolAssets"
              }
            }
          }
        ],
        "count": 12
      },
      "Location": {
        "value": "13",
        "negativeOverlap": 0,
        "positiveOverlap": 0,
        "type": "POT"
      },
      "ProgramToolNumber": "10"
    },
    "assetId": "123456.10",
    "serialNumber": "10",
    "toolId": "123456"
  }
})",
            sdoc);
}

TEST_F(CuttingToolTest, test_xmlns_with_top_element_alias)
{
  const auto doc =
      R"DOC(<CuttingTool assetId="123456.10" serialNumber="10" toolId="123456">
  <CuttingToolLifeCycle>
    <CutterStatus>
      <Status>AVAILABLE</Status>
    </CutterStatus>
    <ProgramToolNumber>10</ProgramToolNumber>
    <Location negativeOverlap="0" positiveOverlap="0" type="POT">13</Location>
    <CuttingItems count="12">
      <CuttingItem indices="1">
        <ItemLife countDirection="UP" initial="0" limit="0" type="PART_COUNT">0</ItemLife>
        <ItemLife countDirection="UP" initial="0" limit="0" type="MINUTES">0</ItemLife>
        <ItemLife countDirection="UP" initial="0" limit="0" type="WEAR">0</ItemLife>
        <x:ItemCutterStatus xmlns:x="okuma.com:OkumaToolAssets">
          <Status>AVAILABLE</Status>
        </x:ItemCutterStatus>
        <x:ItemProgramToolGroup xmlns:x="okuma.com:OkumaToolAssets">0</x:ItemProgramToolGroup>
      </CuttingItem>
    </CuttingItems>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {"x"});
  string content = m_writer->getContent();

  ASSERT_EQ(content, R"DOC(<CuttingTool assetId="123456.10" serialNumber="10" toolId="123456">
  <CuttingToolLifeCycle>
    <CutterStatus>
      <Status>AVAILABLE</Status>
    </CutterStatus>
    <ProgramToolNumber>10</ProgramToolNumber>
    <Location negativeOverlap="0" positiveOverlap="0" type="POT">13</Location>
    <CuttingItems count="12">
      <CuttingItem indices="1">
        <ItemLife countDirection="UP" initial="0" limit="0" type="PART_COUNT">0</ItemLife>
        <ItemLife countDirection="UP" initial="0" limit="0" type="MINUTES">0</ItemLife>
        <ItemLife countDirection="UP" initial="0" limit="0" type="WEAR">0</ItemLife>
        <x:ItemCutterStatus>
          <Status>AVAILABLE</Status>
        </x:ItemCutterStatus>
        <x:ItemProgramToolGroup>0</x:ItemProgramToolGroup>
      </CuttingItem>
    </CuttingItems>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC");
}

TEST_F(CuttingToolTest, element_order_should_place_cutter_status_before_locus)
{
  const auto doc =
      R"DOC(<CuttingTool assetId="M8010W4194N1.2" deviceUuid="5fd88408-7811-3c6b-5400-11f4026b6890" serialNumber="0" timestamp="2022-07-12T22:38:38.2295Z" toolId="14076001">
  <CuttingToolLifeCycle>
    <CutterStatus>
      <Status>USED</Status>
    </CutterStatus>
    <Location negativeOverlap="0" positiveOverlap="0" type="POT">2</Location>
    <CuttingItems count="1">
      <CuttingItem indices="1">
        <Description>FACE MILL</Description>
        <CutterStatus>
          <Status>USED</Status>
          <Status>AVAILABLE</Status>
          <Status>ALLOCATED</Status>
        </CutterStatus>
        <Locus>12</Locus>
        <ItemLife countDirection="UP" initial="0" limit="0" type="MINUTES" warning="80">4858</ItemLife>
        <ItemLife countDirection="UP" initial="0" limit="0" type="PART_COUNT" warning="80">523</ItemLife>
        <ProgramToolGroup>14076001</ProgramToolGroup>
        <Measurements>
          <CuttingDiameter nominal="76">76.16299</CuttingDiameter>
          <FunctionalLength>259.955</FunctionalLength>
        </Measurements>
      </CuttingItem>
    </CuttingItems>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});
  string content = m_writer->getContent();

  ASSERT_EQ(content, doc);
}
