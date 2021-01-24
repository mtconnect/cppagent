// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "device_model/coordinate_systems.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::adapter;

class CoordinateSystemTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/configuration.xml",
                                   8, 4, "1.6", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
  }

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
  EXPECT_EQ("world", world->m_attributes["id"]);
  EXPECT_EQ("WORLD", world->m_attributes["type"]);
  EXPECT_EQ("worldy", world->m_attributes["name"]);
  EXPECT_EQ(world->m_attributes.end(), world->m_attributes.find("nativeName"));
  EXPECT_EQ(world->m_attributes.end(), world->m_attributes.find("parentIdRef"));

  EXPECT_TRUE(world->m_geometry);
  ASSERT_TRUE(holds_alternative<Origin>(world->m_geometry->m_location));
  const Origin &wt = get<Origin>(world->m_geometry->m_location);
  EXPECT_EQ(101.0, wt.m_x);
  EXPECT_EQ(102.0, wt.m_y);
  EXPECT_EQ(103.0, wt.m_z);

  systems++;
  const auto &machine = *systems;
  EXPECT_EQ("machine", machine->m_attributes["id"]);
  EXPECT_EQ("MACHINE", machine->m_attributes["type"]);
  EXPECT_EQ("machiney", machine->m_attributes["name"]);
  EXPECT_EQ("xxx", machine->m_attributes["nativeName"]);
  EXPECT_EQ("world", machine->m_attributes["parentIdRef"]);
  
  EXPECT_TRUE(machine->m_geometry);
  ASSERT_TRUE(holds_alternative<Transformation>(machine->m_geometry->m_location));
  const Transformation &mt = get<Transformation>(machine->m_geometry->m_location);
  EXPECT_TRUE(mt.m_translation);
  EXPECT_EQ(10.0, mt.m_translation->m_x);
  EXPECT_EQ(10.0, mt.m_translation->m_y);
  EXPECT_EQ(10.0, mt.m_translation->m_z);

  EXPECT_TRUE(mt.m_rotation);
  EXPECT_EQ(90.0, mt.m_rotation->m_roll);
  EXPECT_EQ(0, mt.m_rotation->m_pitch);
  EXPECT_EQ(90.0, mt.m_rotation->m_yaw);
}

#define CONFIGURATION_PATH "//m:Device/m:Configuration"
#define COORDINATE_SYSTEMS_PATH CONFIGURATION_PATH "/m:CoordinateSystems"

TEST_F(CoordinateSystemTest, XmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/probe");
    
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
  auto agent = m_agentTestHelper->getAgent();
  m_adapter = new Adapter("LinuxCNC", "server", 7878);
  agent->addAdapter(m_adapter);
  ASSERT_TRUE(m_adapter);
  
  {
    m_agentTestHelper->m_request.m_accepts = "Application/json";
    PARSE_JSON_RESPONSE("/probe");
        
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
    json origin = wfields["Origin"];
    EXPECT_TRUE(origin.is_array());
    EXPECT_EQ(101.0, origin[0]);
    EXPECT_EQ(102.0, origin[1]);
    EXPECT_EQ(103.0, origin[2]);
    
    auto machine = systems.at(1);
    auto mfields = machine.at("/CoordinateSystem"_json_pointer);
    ASSERT_EQ(6, mfields.size());
    EXPECT_EQ("MACHINE", mfields["type"]);
    EXPECT_EQ("machiney", mfields["name"]);
    EXPECT_EQ("machine", mfields["id"]);
    EXPECT_EQ("xxx", mfields["nativeName"]);
    EXPECT_EQ("world", mfields["parentIdRef"]);
    
    json trans = mfields["Transformation"]["Translation"];
    ASSERT_TRUE(trans.is_array());
    EXPECT_EQ(10.0, trans[0]);
    EXPECT_EQ(10.0, trans[1]);
    EXPECT_EQ(10.0, trans[2]);
    
    json rot = mfields["Transformation"]["Rotation"];
    ASSERT_TRUE(rot.is_array());
    EXPECT_EQ(90.0, rot[0]);
    EXPECT_EQ(0.0, rot[1]);
    EXPECT_EQ(90.0, rot[2]);
  }
}
