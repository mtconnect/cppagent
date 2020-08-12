// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "coordinate_systems.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class CoordinateSystemTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_checkpoint = nullptr;
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/configuration.xml", 4, 4, "1.5");
    m_agentId = int64ToString(getCurrentTimeInSec());
    m_adapter = nullptr;

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();

    m_device = m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override
  {
    m_agent.reset();
    m_checkpoint.reset();
    m_agentTestHelper.reset();
  }

  std::unique_ptr<Checkpoint> m_checkpoint;
  std::unique_ptr<Agent> m_agent;
  Adapter *m_adapter{nullptr};
  std::string m_agentId;
  Device *m_device{nullptr};

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(CoordinateSystemTest, ParseDeviceAndComponentRelationships)
{
  ASSERT_NE(nullptr, m_device);
  
  ASSERT_EQ(1, m_device->getConfiguration().size());
    
  auto ci = m_device->getConfiguration().begin();
  const auto conf = ci->get();
  ASSERT_EQ(typeid(CoordinateSystems), typeid(*conf));
  
  const auto cds = dynamic_cast<const CoordinateSystems*>(conf);
  ASSERT_NE(nullptr, cds);
  ASSERT_EQ(2, cds->getCoordinateSystems().size());

  auto systems = cds->getCoordinateSystems().begin();
  
  const auto &world = *systems;
  EXPECT_EQ("world", world->m_id);
  EXPECT_EQ("WORLD", world->m_type);
  EXPECT_EQ("worldy", world->m_name);
  EXPECT_EQ("101 102 103", world->m_origin);
  EXPECT_TRUE(world->m_nativeName.empty());
  EXPECT_TRUE(world->m_parentIdRef.empty());
  EXPECT_TRUE(world->m_rotation.empty());
  EXPECT_TRUE(world->m_translation.empty());

  systems++;
  const auto &machine = *systems;
  EXPECT_EQ("machine", machine->m_id);
  EXPECT_EQ("MACHINE", machine->m_type);
  EXPECT_EQ("machiney", machine->m_name);
  EXPECT_EQ("xxx", machine->m_nativeName);
  EXPECT_EQ("world", machine->m_parentIdRef);
  EXPECT_EQ("10 10 10", machine->m_translation);
  EXPECT_EQ("90 0 90", machine->m_rotation);
  EXPECT_TRUE(machine->m_origin.empty());
}

#define CONFIGURATION_PATH "//m:Device/m:Configuration"
#define COORDINATE_SYSTEMS_PATH CONFIGURATION_PATH "/m:CoordinateSystems"

TEST_F(CoordinateSystemTest, XmlPrinting)
{
  m_agentTestHelper->m_path = "/probe";
  {
    PARSE_XML_RESPONSE;
    
    ASSERT_XML_PATH_COUNT(doc, COORDINATE_SYSTEMS_PATH , 1);
    ASSERT_XML_PATH_COUNT(doc, COORDINATE_SYSTEMS_PATH "/*" , 2);

    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']@type" , "WORLD");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']@name" , "worldy");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']/m:Origin" , "101 102 103");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']@nativeName" , nullptr);
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']@parentIdRef" , nullptr);

    
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']@type" , "MACHINE");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']@name" , "machiney");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']@nativeName" , "xxx");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']@parentIdRef" , "world");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']/m:Transformation/m:Translation" , "10 10 10");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']/m:Transformation/m:Rotation" , "90 0 90");
  }
}

TEST_F(CoordinateSystemTest, JsonPrinting)
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  ASSERT_TRUE(m_adapter);
  
  m_agentTestHelper->m_path = "/probe";
  m_agentTestHelper->m_incomingHeaders["Accept"] = "Application/json";
  
  {
    PARSE_JSON_RESPONSE;
        
    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto systems = device.at("/Configuration/CoordinateSystems"_json_pointer);
    ASSERT_TRUE(systems.is_array());
    ASSERT_EQ(2_S, systems.size());
    

    auto world = systems.at(0);    
    auto wfields = world.at("/CoordinateSystem"_json_pointer);
    ASSERT_EQ(4, wfields.size());
    EXPECT_EQ("WORLD", wfields["type"]);
    EXPECT_EQ("worldy", wfields["name"]);
    EXPECT_EQ("world", wfields["id"]);

    EXPECT_EQ("101 102 103", wfields["Transformation"]["Origin"].get<string>());
    
    auto machine = systems.at(1);
    auto mfields = machine.at("/CoordinateSystem"_json_pointer);
    ASSERT_EQ(6, mfields.size());
    EXPECT_EQ("MACHINE", mfields["type"]);
    EXPECT_EQ("machiney", mfields["name"]);
    EXPECT_EQ("machine", mfields["id"]);
    EXPECT_EQ("xxx", mfields["nativeName"]);
    EXPECT_EQ("world", mfields["parentIdRef"]);

    EXPECT_EQ("10 10 10", mfields["Transformation"]["Translation"]);
    EXPECT_EQ("90 0 90", mfields["Transformation"]["Rotation"]);
  }
}
