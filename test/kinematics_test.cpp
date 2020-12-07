// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "motion.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class KinematicsTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/kinematics.xml", 4, 4, "1.7");
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

TEST_F(KinematicsTest, ParseZAxisKinematics)
{
  ASSERT_NE(nullptr, m_device);
  
  auto linear = m_device->getComponentById("z");
  
  ASSERT_EQ(1, linear->getConfiguration().size());
  auto ci = linear->getConfiguration().begin();
  
  auto model = dynamic_cast<const Motion*>(ci->get());
  ASSERT_NE(nullptr, model);
  
  ASSERT_EQ("zax", model->m_attributes.find("id")->second);
  ASSERT_EQ("PRISMATIC", model->m_attributes.find("type")->second);
  ASSERT_EQ("DIRECT", model->m_attributes.find("actuation")->second);
  ASSERT_EQ("machine", model->m_attributes.find("coordinateSystemIdRef")->second);
  ASSERT_EQ("The linears Z kinematics", model->m_description);
  
  ASSERT_TRUE(model->m_geometry);
  ASSERT_NE(0, model->m_geometry->m_location.index());
  ASSERT_TRUE(holds_alternative<Origin>(model->m_geometry->m_location));
  
  const Origin &o = get<Origin>(model->m_geometry->m_location);
  ASSERT_EQ(100.0, o.m_x);
  ASSERT_EQ(101.0, o.m_y);
  ASSERT_EQ(102.0, o.m_z);
  
  ASSERT_TRUE(model->m_geometry->m_axis);
  ASSERT_EQ(0.0, model->m_geometry->m_axis->m_x);
  ASSERT_EQ(0.1, model->m_geometry->m_axis->m_y);
  ASSERT_EQ(1.0, model->m_geometry->m_axis->m_z);
}

TEST_F(KinematicsTest, ParseCAxisKinematics)
{
  ASSERT_NE(nullptr, m_device);
  
  auto rot = m_device->getComponentById("c");
  
  ASSERT_EQ(1, rot->getConfiguration().size());
  auto ci = rot->getConfiguration().begin();
  
  auto model = dynamic_cast<const Motion*>(ci->get());
  ASSERT_NE(nullptr, model);
  
  ASSERT_EQ("spin", model->m_attributes.find("id")->second);
  ASSERT_EQ("CONTINUOUS", model->m_attributes.find("type")->second);
  ASSERT_EQ("DIRECT", model->m_attributes.find("actuation")->second);
  ASSERT_EQ("machine", model->m_attributes.find("coordinateSystemIdRef")->second);
  ASSERT_EQ("zax", model->m_attributes.find("parentIdRef")->second);
  ASSERT_EQ("The spindle kinematics", model->m_description);

  ASSERT_TRUE(model->m_geometry);
  ASSERT_NE(0, model->m_geometry->m_location.index());
  ASSERT_TRUE(holds_alternative<Transformation>(model->m_geometry->m_location));
  
  const Transformation &o = get<Transformation>(model->m_geometry->m_location);
  ASSERT_TRUE(o.m_translation);
  ASSERT_EQ(10.0, o.m_translation->m_x);
  ASSERT_EQ(20.0, o.m_translation->m_y);
  ASSERT_EQ(30.0, o.m_translation->m_z);
  
  ASSERT_TRUE(o.m_rotation);
  ASSERT_EQ(90.0, o.m_rotation->m_roll);
  ASSERT_EQ(0.0, o.m_rotation->m_pitch);
  ASSERT_EQ(180.0, o.m_rotation->m_yaw);
  
  ASSERT_TRUE(model->m_geometry->m_axis);
  ASSERT_EQ(0.0, model->m_geometry->m_axis->m_x);
  ASSERT_EQ(0.5, model->m_geometry->m_axis->m_y);
  ASSERT_EQ(1.0, model->m_geometry->m_axis->m_z);
}

#define ZAXIS_CONFIGURATION_PATH "//m:Linear[@id='z']/m:Configuration"
#define ZAXIS_MOTION_PATH ZAXIS_CONFIGURATION_PATH "/m:Motion"

TEST_F(KinematicsTest, ZAxisXmlPrinting)
{
  m_agentTestHelper->m_path = "/LinuxCNC/probe";
  {
    PARSE_XML_RESPONSE;
    
    ASSERT_XML_PATH_COUNT(doc, ZAXIS_MOTION_PATH , 1);
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "@id" , "zax");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "@type" , "PRISMATIC");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "@actuation" , "DIRECT");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "@coordinateSystemIdRef" , "machine");

    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "/m:Origin" , "100 101 102");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "/m:Axis" , "0 0.1 1");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "/m:Description" , "The linears Z kinematics");

  }
}


#define ROTARY_CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define ROTARY_MOTION_PATH ROTARY_CONFIGURATION_PATH "/m:Motion"


TEST_F(KinematicsTest, RotaryXmlPrinting)
{
  m_agentTestHelper->m_path = "/LinuxCNC/probe";
  {
    PARSE_XML_RESPONSE;
    
    ASSERT_XML_PATH_COUNT(doc, ROTARY_MOTION_PATH , 1);
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@id" , "spin");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@type" , "CONTINUOUS");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@parentIdRef" , "zax");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@actuation" , "DIRECT");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@coordinateSystemIdRef" , "machine");

    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "/m:Transformation/m:Translation" , "10 20 30");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "/m:Transformation/m:Rotation" , "90 0 180");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "/m:Axis" , "0 0.5 1");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "/m:Description" , "The spindle kinematics");
  }
}


TEST_F(KinematicsTest, ZAxisJsonPrinting)
{
  m_agentTestHelper->m_path = "/LinuxCNC/probe";
  m_agentTestHelper->m_incomingHeaders["Accept"] = "Application/json";
  
  {
    PARSE_JSON_RESPONSE;
    
    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);
    auto linear = device.at("/Components/0/Axes/Components/0/Linear"_json_pointer);
    
    auto motion = linear.at("/Configuration/Motion"_json_pointer);
    ASSERT_TRUE(motion.is_object());
    
    ASSERT_EQ(7, motion.size());
    EXPECT_EQ("zax", motion["id"]);
    EXPECT_EQ("PRISMATIC", motion["type"]);
    EXPECT_EQ("DIRECT", motion["actuation"]);
    EXPECT_EQ("machine", motion["coordinateSystemIdRef"]);
    
    auto origin = motion.at("/Origin"_json_pointer);
    ASSERT_TRUE(origin.is_array());
    ASSERT_EQ(3, origin.size());
    ASSERT_EQ(100.0, origin[0].get<double>());
    ASSERT_EQ(101.0, origin[1].get<double>());
    ASSERT_EQ(102.0, origin[2].get<double>());
    
    auto axis = motion.at("/Axis"_json_pointer);
    ASSERT_TRUE(axis.is_array());
    ASSERT_EQ(3, axis.size());
    ASSERT_EQ(0.0, axis[0].get<double>());
    ASSERT_EQ(0.1, axis[1].get<double>());
    ASSERT_EQ(1.0, axis[2].get<double>());
    
    ASSERT_EQ("The linears Z kinematics", motion["Description"].get<string>());
  }
}

TEST_F(KinematicsTest, RotaryJsonPrinting)
{
  m_agentTestHelper->m_path = "/LinuxCNC/probe";
  m_agentTestHelper->m_incomingHeaders["Accept"] = "Application/json";
  
  {
    PARSE_JSON_RESPONSE;
    
    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);
    auto rotary = device.at("/Components/0/Axes/Components/1/Rotary"_json_pointer);
    
    auto motion = rotary.at("/Configuration/Motion"_json_pointer);
    ASSERT_TRUE(motion.is_object());
    
    ASSERT_EQ(8, motion.size());
    EXPECT_EQ("spin", motion["id"]);
    EXPECT_EQ("CONTINUOUS", motion["type"]);
    EXPECT_EQ("DIRECT", motion["actuation"]);
    EXPECT_EQ("zax", motion["parentIdRef"]);
    EXPECT_EQ("machine", motion["coordinateSystemIdRef"]);
    
    auto trans = motion.at("/Transformation/Translation"_json_pointer);
    ASSERT_TRUE(trans.is_array());
    ASSERT_EQ(3, trans.size());
    ASSERT_EQ(10.0, trans[0].get<double>());
    ASSERT_EQ(20.0, trans[1].get<double>());
    ASSERT_EQ(30.0, trans[2].get<double>());
    
    auto rot = motion.at("/Transformation/Rotation"_json_pointer);
    ASSERT_TRUE(rot.is_array());
    ASSERT_EQ(3, rot.size());
    ASSERT_EQ(90.0, rot[0].get<double>());
    ASSERT_EQ(0.0, rot[1].get<double>());
    ASSERT_EQ(180.0, rot[2].get<double>());

    auto axis = motion.at("/Axis"_json_pointer);
    ASSERT_TRUE(axis.is_array());
    ASSERT_EQ(3, axis.size());
    ASSERT_EQ(0.0, axis[0].get<double>());
    ASSERT_EQ(0.5, axis[1].get<double>());
    ASSERT_EQ(1.0, axis[2].get<double>());
    
    ASSERT_EQ("The spindle kinematics", motion["Description"].get<string>());

  }
}

