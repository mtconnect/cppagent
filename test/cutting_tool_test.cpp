// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "solid_model.hpp"
#include "cutting_tool.hpp"
#include "xml_printer_helper.hpp"
#include "entity.hpp"
#include "entity/xml_parser.hpp"
#include "entity/xml_printer.hpp"
#include "xml_printer.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;

class CuttingToolTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml",
                                   8, 4, "1.7", 25);
    m_agentId = int64ToString(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");

    // Asset types are registered in the agent.
    m_adapter = nullptr;
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
    
    m_writer = make_unique<XmlWriter>(true);
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
    m_writer.reset();
  }

  void addAdapter()
  {
    ASSERT_FALSE(m_adapter);
    m_adapter = new Adapter("LinuxCNC", "server", 7878);
    m_agentTestHelper->m_agent->addAdapter(m_adapter);
    ASSERT_TRUE(m_adapter);
  }
  
  std::string m_agentId;
  Device *m_device{nullptr};
  Adapter *m_adapter{nullptr};
  
  std::unique_ptr<XmlWriter> m_writer;
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
  
  auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  
  auto asset = dynamic_cast<Asset*>(entity.get());
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
  printer.print(*m_writer, entity);

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
  
  auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  
  auto asset = dynamic_cast<Asset*>(entity.get());
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
  printer.print(*m_writer, entity);

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
  
  auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  
  auto asset = dynamic_cast<Asset*>(entity.get());
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
  printer.print(*m_writer, entity);

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
  
  auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  
  auto asset = dynamic_cast<Asset*>(entity.get());
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
  printer.print(*m_writer, entity);

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
  
  auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
  ASSERT_EQ(2, errors.size());
  ASSERT_EQ("CuttingToolLifeCycle(CutterStatus): Property CutterStatus is required and not provided",
            string(errors.front()->what()));
  ASSERT_EQ("CuttingTool: Invalid element 'CuttingToolLifeCycle'",
            string(errors.back()->what()));

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
  
  auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
  ASSERT_EQ(6, errors.size());
  auto it = errors.begin();
  EXPECT_EQ("FunctionalLength(VALUE): Property VALUE is required and not provided",
            string((*it)->what()));
  it++;
  EXPECT_EQ("Measurements: Invalid element 'FunctionalLength'",
            string((*it)->what()));
  it++;
  EXPECT_EQ("CuttingDiameterMax(VALUE): Property VALUE is required and not provided",
            string((*it)->what()));
  it++;
  EXPECT_EQ("Measurements: Invalid element 'CuttingDiameterMax'",
            string((*it)->what()));
  it++;
  EXPECT_EQ("Measurements(Measurement): Entity list requirement Measurement must have at least 1 entries, 0 found",
            string((*it)->what()));
  it++;
  EXPECT_EQ("CuttingToolLifeCycle: Invalid element 'Measurements'",
            string((*it)->what()));

}

TEST_F(CuttingToolTest, AssetWithSimpleCuttingItems)
{
  auto printer = dynamic_cast<mtconnect::XmlPrinter *>(m_agentTestHelper->m_agent->getPrinter("xml"));
  ASSERT_TRUE(printer != nullptr);

  printer->clearAssetsNamespaces();
  printer->addAssetsNamespace("urn:machine.com:MachineAssets:1.3",
                              "http://www.machine.com/schemas/MachineAssets_1.3.xsd", "x");

  addAdapter();

  m_adapter->parseBuffer("TIME|@ASSET@|XXX.200|CuttingTool|--multiline--AAAA\n");
  m_adapter->parseBuffer((getFile("asset5.xml") + "\n").c_str());
  m_adapter->parseBuffer("--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)1, m_agentTestHelper->m_agent->getAssetCount());

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
