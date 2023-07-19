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
using namespace mtconnect::source::adapter;
using namespace entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class SolidModelTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/solid_model.xml", 8, 4, "1.7", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  Adapter *m_adapter {nullptr};
  std::string m_agentId;
  DevicePtr m_device;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(SolidModelTest, ParseDeviceSolidModel)
{
  ASSERT_NE(nullptr, m_device);

  auto &clc = m_device->get<EntityPtr>("Configuration");
  ASSERT_TRUE(clc);
  auto model = clc->get<EntityPtr>("SolidModel");

  EXPECT_EQ("SolidModel", model->getName());

  ASSERT_EQ("dm", model->get<string>("id"));
  ASSERT_EQ("STL", model->get<string>("mediaType"));
  ASSERT_EQ("/models/foo.stl", model->get<string>("href"));
  ASSERT_EQ("machine", model->get<string>("coordinateSystemIdRef"));

  auto scale = model->get<Vector>("Scale");

  ASSERT_EQ(2.0, scale[0]);
  ASSERT_EQ(3.0, scale[1]);
  ASSERT_EQ(4.0, scale[2]);
}

TEST_F(SolidModelTest, ParseRotarySolidModel)
{
  ASSERT_NE(nullptr, m_device);

  auto rot = m_device->getComponentById("c");
  auto &clc = rot->get<EntityPtr>("Configuration");
  ASSERT_TRUE(clc);
  auto model = clc->get<EntityPtr>("SolidModel");

  ASSERT_EQ("cm", model->get<string>("id"));
  ASSERT_EQ("dm", model->get<string>("solidModelIdRef"));
  ASSERT_EQ("spindle", model->get<string>("itemRef"));
  ASSERT_EQ("STL", model->get<string>("mediaType"));
  ASSERT_EQ("machine", model->get<string>("coordinateSystemIdRef"));
  ASSERT_EQ("MILLIMETER", model->get<string>("units"));
  ASSERT_EQ("METER", model->get<string>("nativeUnits"));

  auto tf = model->maybeGet<EntityPtr>("Transformation");
  ASSERT_TRUE(tf);

  auto tv = (*tf)->get<Vector>("Translation");
  ASSERT_EQ(10.0, tv[0]);
  ASSERT_EQ(20.0, tv[1]);
  ASSERT_EQ(30.0, tv[2]);

  auto rv = (*tf)->get<Vector>("Rotation");
  ASSERT_EQ(90.0, rv[0]);
  ASSERT_EQ(-90.0, rv[1]);
  ASSERT_EQ(180.0, rv[2]);

  ASSERT_FALSE(model->hasProperty("Scale"));
}

#define DEVICE_CONFIGURATION_PATH "//m:Device/m:Configuration"
#define DEVICE_SOLID_MODEL_PATH DEVICE_CONFIGURATION_PATH "/m:SolidModel"

TEST_F(SolidModelTest, DeviceXmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, DEVICE_SOLID_MODEL_PATH, 1);
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@id", "dm");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@mediaType", "STL");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@href", "/models/foo.stl");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@coordinateSystemIdRef", "machine");

    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "/m:Scale", "2 3 4");
  }
}

#define ROTARY_CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define ROTARY_SOLID_MODEL_PATH ROTARY_CONFIGURATION_PATH "/m:SolidModel"

TEST_F(SolidModelTest, RotaryXmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, ROTARY_SOLID_MODEL_PATH, 1);
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@id", "cm");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@mediaType", "STL");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@solidModelIdRef", "dm");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@itemRef", "spindle");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "@coordinateSystemIdRef", "machine");

    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "/m:Transformation/m:Translation",
                          "10 20 30");
    ASSERT_XML_PATH_EQUAL(doc, ROTARY_SOLID_MODEL_PATH "/m:Transformation/m:Rotation",
                          "90 -90 180");
  }
}

TEST_F(SolidModelTest, DeviceJsonPrinting)
{
  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto model = device.at("/Configuration/SolidModel"_json_pointer);
    ASSERT_TRUE(model.is_object());

    ASSERT_EQ(5, model.size());
    EXPECT_EQ("dm", model["id"]);
    EXPECT_EQ("STL", model["mediaType"]);
    EXPECT_EQ("/models/foo.stl", model["href"]);
    EXPECT_EQ("machine", model["coordinateSystemIdRef"]);

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
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);
    auto rotary = device.at("/Components/0/Axes/Components/0/Rotary"_json_pointer);

    auto model = rotary.at("/Configuration/SolidModel"_json_pointer);
    ASSERT_TRUE(model.is_object());

    ASSERT_EQ(8, model.size());
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
