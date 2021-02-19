// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"

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

class SolidModelTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/solid_model.xml",
                                   8, 4, "1.7", 25);
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

TEST_F(SolidModelTest, ParseDeviceSolidModel)
{
  ASSERT_NE(nullptr, m_device);

  auto &configurations_list = m_device->getConfiguration();

  ASSERT_TRUE(!configurations_list.empty());

  auto configurations_entity = configurations_list.front()->getEntity();

  auto &configuration = configurations_entity->get<entity::EntityList>("LIST");

  ASSERT_EQ(2, configuration.size());

  auto config = configuration.begin();
  config++;

  EXPECT_EQ("SolidModel", (*config)->getName());
  
  ASSERT_EQ("dm", get<string>((*config)->getProperty("id")));
  ASSERT_EQ("STL", get<string>((*config)->getProperty("mediaType")));
  ASSERT_EQ("/models/foo.stl", get<string>((*config)->getProperty("href")));
  ASSERT_EQ("machine", get<string>((*config)->getProperty("coordinateSystemIdRef")));
  
  auto scale = (*config)->getProperty("Scale");

  ASSERT_EQ(2.0, get<std::vector<double>>(scale).at(0));
  ASSERT_EQ(3.0, get<std::vector<double>>(scale).at(1));
  ASSERT_EQ(4.0, get<std::vector<double>>(scale).at(2));
  
}


/*
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
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    
    ASSERT_XML_PATH_COUNT(doc, DEVICE_SOLID_MODEL_PATH , 1);
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@id" , "dm");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@mediaType" , "STL");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@href" , "/models/foo.stl");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@coordinateSystemIdRef" , "machine");

    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "/m:Origin" , "10 20 30");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "/m:Scale" , "2 3 4");

  }
}

#define ROTARY_CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define ROTARY_SOLID_MODEL_PATH ROTARY_CONFIGURATION_PATH "/m:SolidModel"


TEST_F(SolidModelTest, RotaryXmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, ROTARY_SOLID_MODEL_PATH , 1);
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@id" , "cm");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@mediaType" , "STL");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@solidModelIdRef" , "dm");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@itemRef" , "spindle");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@coordinateSystemIdRef" , "machine");
    
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "/m:Transformation/m:Translation" , "10 20 30");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "/m:Transformation/m:Rotation" , "90 -90 180");
  }
}


TEST_F(SolidModelTest, DeviceJsonPrinting)
{
  {
    m_agentTestHelper->m_request.m_accepts = "Application/json";
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto model = device.at("/Configuration/SolidModel"_json_pointer);
    ASSERT_TRUE(model.is_object());
    
    ASSERT_EQ(6, model.size());
    EXPECT_EQ("dm", model["id"]);
    EXPECT_EQ("STL", model["mediaType"]);
    EXPECT_EQ("/models/foo.stl", model["href"]);
    EXPECT_EQ("machine", model["coordinateSystemIdRef"]);

    auto origin = model["Origin"];
    ASSERT_TRUE(origin.is_array());
    ASSERT_EQ(3, origin.size());
    ASSERT_EQ(10.0, origin[0].get<double>());
    ASSERT_EQ(20.0, origin[1].get<double>());
    ASSERT_EQ(30.0, origin[2].get<double>());
    
    auto scale = model["Scale"];
    ASSERT_TRUE(scale.is_array());
    ASSERT_EQ(3, scale.size());
    ASSERT_EQ(2.0, scale[0].get<double>());
    ASSERT_EQ(3.0, scale[1].get<double>());
    ASSERT_EQ(4.0, scale[2].get<double>());
  }
}

TEST_F(SolidModelTest, RotaryJsonPrinting)
{
  {
    m_agentTestHelper->m_request.m_accepts = "Application/json";
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);
    auto rotary = device.at("/Components/0/Axes/Components/0/Rotary"_json_pointer);

    auto model = rotary.at("/Configuration/SolidModel"_json_pointer);
    ASSERT_TRUE(model.is_object());
    
    ASSERT_EQ(6, model.size());
    EXPECT_EQ("cm", model["id"]);
    EXPECT_EQ("STL", model["mediaType"]);
    EXPECT_EQ("machine", model["coordinateSystemIdRef"]);
    EXPECT_EQ("dm", model["solidModelIdRef"]);
    EXPECT_EQ("spindle", model["itemRef"]);

    auto trans = model.at("/Transformation/Translation"_json_pointer);
    ASSERT_TRUE(trans.is_array());
    ASSERT_EQ(3, trans.size());
    ASSERT_EQ(10.0, trans[0].get<double>());
    ASSERT_EQ(20.0, trans[1].get<double>());
    ASSERT_EQ(30.0, trans[2].get<double>());
    
    auto rot = model.at("/Transformation/Rotation"_json_pointer);
    ASSERT_TRUE(rot.is_array());
    ASSERT_EQ(3, rot.size());
    ASSERT_EQ(90.0, rot[0].get<double>());
    ASSERT_EQ(-90.0, rot[1].get<double>());
    ASSERT_EQ(180.0, rot[2].get<double>());
  }
}
*/