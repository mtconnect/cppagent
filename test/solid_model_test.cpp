// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "solid_model.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class SolidModelTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/solid_model.xml", 4, 4, "1.7");
    m_agentId = int64ToString(getCurrentTimeInSec());

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();

    m_device = m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override
  {
    m_agent.reset();
    m_agentTestHelper.reset();
  }

  std::unique_ptr<Agent> m_agent;
  std::string m_agentId;
  Device *m_device{nullptr};

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(SolidModelTest, ParseDeviceSolidModel)
{
  ASSERT_NE(nullptr, m_device);
  
  ASSERT_EQ(2, m_device->getConfiguration().size());
    
  auto ci = m_device->getConfiguration().begin();
  ci++;
    
  auto model = dynamic_cast<const SolidModel*>(ci->get());
  ASSERT_NE(nullptr, model);
  
  ASSERT_EQ("dm", model->m_attributes.find("id")->second);
  ASSERT_EQ("/models/foo.stl", model->m_attributes.find("href")->second);
  ASSERT_EQ("STL", model->m_attributes.find("mediaType")->second);
  ASSERT_EQ("machine", model->m_attributes.find("coordinateSystemIdRef")->second);
  ASSERT_EQ(model->m_attributes.end(), model->m_attributes.find("dummy"));
  
  ASSERT_TRUE(model->m_geometry);
  ASSERT_NE(0, model->m_geometry->m_location.index());
  ASSERT_TRUE(holds_alternative<Origin>(model->m_geometry->m_location));
  
  const Origin &o = get<Origin>(model->m_geometry->m_location);
  ASSERT_EQ(10.0, o.m_x);
  ASSERT_EQ(20.0, o.m_y);
  ASSERT_EQ(30.0, o.m_z);
  
  ASSERT_TRUE(model->m_geometry->m_scale);
  ASSERT_EQ(2.0, model->m_geometry->m_scale->m_scaleX);
  ASSERT_EQ(3.0, model->m_geometry->m_scale->m_scaleY);
  ASSERT_EQ(4.0, model->m_geometry->m_scale->m_scaleZ);
}

TEST_F(SolidModelTest, ParseRotarySolidModel)
{
  ASSERT_NE(nullptr, m_device);
  auto rotary = m_device->getComponentById("c");
  
  ASSERT_EQ(1, rotary->getConfiguration().size());
  
  auto ci = rotary->getConfiguration().begin();
  
  auto model = dynamic_cast<const SolidModel*>(ci->get());
  ASSERT_NE(nullptr, model);
  
  ASSERT_EQ("cm", model->m_attributes.find("id")->second);
  ASSERT_EQ("dm", model->m_attributes.find("solidModelIdRef")->second);
  ASSERT_EQ("spindle", model->m_attributes.find("itemRef")->second);
  ASSERT_EQ("STL", model->m_attributes.find("mediaType")->second);
  ASSERT_EQ("machine", model->m_attributes.find("coordinateSystemIdRef")->second);
  
  ASSERT_TRUE(model->m_geometry);
  ASSERT_NE(0, model->m_geometry->m_location.index());
  ASSERT_TRUE(holds_alternative<Transformation>(model->m_geometry->m_location));
  
  const Transformation &t = get<Transformation>(model->m_geometry->m_location);
  ASSERT_TRUE(t.m_translation);
  ASSERT_EQ(10.0, t.m_translation->m_x);
  ASSERT_EQ(20.0, t.m_translation->m_y);
  ASSERT_EQ(30.0, t.m_translation->m_z);
  
  ASSERT_TRUE(t.m_rotation);
  ASSERT_EQ(90.0, t.m_rotation->m_roll);
  ASSERT_EQ(-90.0, t.m_rotation->m_pitch);
  ASSERT_EQ(180.0, t.m_rotation->m_yaw);
  
  ASSERT_FALSE(model->m_geometry->m_scale);
}


#define DEVICE_CONFIGURATION_PATH "//m:Device/m:Configuration"
#define DEVICE_SOLID_MODEL_PATH DEVICE_CONFIGURATION_PATH "/m:SolidModel"

TEST_F(SolidModelTest, DeviceXmlPrinting)
{
  m_agentTestHelper->m_path = "/probe";
  {
    PARSE_XML_RESPONSE;
    
    ASSERT_XML_PATH_COUNT(doc, DEVICE_SOLID_MODEL_PATH , 1);
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@id" , "dm");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@mediaType" , "STL");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@href" , "/models/foo.stl");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@coordinateSystemIdRef" , "machine");

    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "m:Origin" , "10 20 30");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "m:Scale" , "2 3 4");

  }
}

#define ROTARY_CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define ROTARY_SOLID_MODEL_PATH ROTARY_CONFIGURATION_PATH "/m:SolidModel"


TEST_F(SolidModelTest, RotaryXmlPrinting)
{
  m_agentTestHelper->m_path = "/probe";
  {
    PARSE_XML_RESPONSE;
    
    ASSERT_XML_PATH_COUNT(doc, ROTARY_SOLID_MODEL_PATH , 1);
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@id" , "cm");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@mediaType" , "STL");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@solidModelIdRef" , "dm");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@itemRef" , "spindle");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@coordinateSystemIdRef" , "machine");
    
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "m:Transformation/m:Translation" , "10 20 30");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "m:Transformation/m:Rotation" , "90 -90 180");    
  }
}

/*
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
*/
