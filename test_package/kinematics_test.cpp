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

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class KinematicsTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/kinematics.xml", 8, 4, "1.7", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  std::string m_agentId;
  DevicePtr m_device {nullptr};
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(KinematicsTest, ParseZAxisKinematics)
{
  using namespace mtconnect::entity;
  ASSERT_NE(nullptr, m_device);

  auto linear = m_device->getComponentById("z");
  ASSERT_TRUE(linear);

  auto &ent = linear->get<EntityPtr>("Configuration");
  ASSERT_TRUE(ent);

  auto motion = ent->get<EntityPtr>("Motion");

  EXPECT_EQ("Motion", motion->getName());

  ASSERT_EQ("zax", motion->get<string>("id"));
  ASSERT_EQ("PRISMATIC", motion->get<string>("type"));
  ASSERT_EQ("DIRECT", motion->get<string>("actuation"));
  ASSERT_EQ("machine", motion->get<string>("coordinateSystemIdRef"));
  ASSERT_EQ("The linears Z kinematics", motion->get<string>("Description"));

  auto origin = motion->get<Vector>("Origin");

  ASSERT_EQ(100.0, origin[0]);
  ASSERT_EQ(101.0, origin[1]);
  ASSERT_EQ(102.0, origin[2]);

  auto axis = motion->get<Vector>("Axis");

  ASSERT_EQ(0.0, axis[0]);
  ASSERT_EQ(0.1, axis[1]);
  ASSERT_EQ(1.0, axis[2]);
}

TEST_F(KinematicsTest, ParseCAxisKinematics)
{
  ASSERT_NE(nullptr, m_device);

  auto rot = m_device->getComponentById("c");
  auto &ent = rot->get<EntityPtr>("Configuration");
  ASSERT_TRUE(ent);
  auto motion = ent->get<EntityPtr>("Motion");

  ASSERT_EQ("spin", motion->get<string>("id"));
  ASSERT_EQ("CONTINUOUS", motion->get<string>("type"));
  ASSERT_EQ("DIRECT", motion->get<string>("actuation"));
  ASSERT_EQ("machine", motion->get<string>("coordinateSystemIdRef"));
  ASSERT_EQ("zax", motion->get<string>("parentIdRef"));
  ASSERT_EQ("The spindle kinematics", motion->get<string>("Description"));

  auto tf = motion->maybeGet<EntityPtr>("Transformation");
  ASSERT_TRUE(tf);

  auto tv = (*tf)->get<Vector>("Translation");
  ASSERT_EQ(10.0, tv[0]);
  ASSERT_EQ(20.0, tv[1]);
  ASSERT_EQ(30.0, tv[2]);

  auto rv = (*tf)->get<Vector>("Rotation");
  ASSERT_EQ(90.0, rv[0]);
  ASSERT_EQ(0.0, rv[1]);
  ASSERT_EQ(180.0, rv[2]);

  auto ax = motion->get<Vector>("Axis");
  ASSERT_EQ(0.0, ax[0]);
  ASSERT_EQ(0.5, ax[1]);
  ASSERT_EQ(1.0, ax[2]);
}

#define ZAXIS_CONFIGURATION_PATH "//m:Linear[@id='z']/m:Configuration"
#define ZAXIS_MOTION_PATH ZAXIS_CONFIGURATION_PATH "/m:Motion"

TEST_F(KinematicsTest, ZAxisXmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, ZAXIS_MOTION_PATH, 1);
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "@id", "zax");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "@type", "PRISMATIC");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "@actuation", "DIRECT");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "@coordinateSystemIdRef", "machine");

    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "/m:Origin", "100 101 102");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "/m:Axis", "0 0.1 1");
    ASSERT_XML_PATH_EQUAL(doc, ZAXIS_MOTION_PATH "/m:Description", "The linears Z kinematics");
  }
}

#define ROTARY_CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define ROTARY_MOTION_PATH ROTARY_CONFIGURATION_PATH "/m:Motion"

TEST_F(KinematicsTest, RotaryXmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, ROTARY_MOTION_PATH, 1);
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@id", "spin");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@type", "CONTINUOUS");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@parentIdRef", "zax");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@actuation", "DIRECT");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "@coordinateSystemIdRef", "machine");

    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "/m:Transformation/m:Translation", "10 20 30");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "/m:Transformation/m:Rotation", "90 0 180");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "/m:Axis", "0 0.5 1");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_MOTION_PATH "/m:Description", "The spindle kinematics");
  }
}

TEST_F(KinematicsTest, ZAxisJsonPrinting)
{
  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

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
  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

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
