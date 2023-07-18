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

class CoordinateSystemTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/configuration.xml", 8, 4, "1.6", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  Adapter *m_adapter {nullptr};
  std::string m_agentId;
  DevicePtr m_device {nullptr};
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(CoordinateSystemTest, ParseDeviceAndComponentRelationships)
{
  ASSERT_NE(nullptr, m_device);

  auto &clc = m_device->get<EntityPtr>("Configuration");
  ASSERT_TRUE(clc);

  const auto &cds = clc->getList("CoordinateSystems");
  ASSERT_TRUE(cds);
  ASSERT_EQ(2, cds->size());

  auto it = cds->begin();

  EXPECT_EQ("world", (*it)->get<string>("id"));
  EXPECT_EQ("WORLD", (*it)->get<string>("type"));
  EXPECT_EQ("worldy", (*it)->get<string>("name"));

  const auto origin = (*it)->getProperty("Origin");

  ASSERT_EQ(101, get<std::vector<double>>(origin).at(0));
  ASSERT_EQ(102, get<std::vector<double>>(origin).at(1));
  ASSERT_EQ(103, get<std::vector<double>>(origin).at(2));

  it++;

  EXPECT_EQ("machine", (*it)->get<string>("id"));
  EXPECT_EQ("MACHINE", (*it)->get<string>("type"));
  EXPECT_EQ("machiney", (*it)->get<string>("name"));
  EXPECT_EQ("xxx", (*it)->get<string>("nativeName"));
  EXPECT_EQ("world", (*it)->get<string>("parentIdRef"));

  const auto transformation = (*it)->get<entity::EntityPtr>("Transformation");

  auto translation = transformation->getProperty("Translation");
  auto rotation = transformation->getProperty("Rotation");

  ASSERT_EQ(10, get<std::vector<double>>(translation).at(0));
  ASSERT_EQ(10, get<std::vector<double>>(translation).at(1));
  ASSERT_EQ(10, get<std::vector<double>>(translation).at(2));

  ASSERT_EQ(90, get<std::vector<double>>(rotation).at(0));
  ASSERT_EQ(0, get<std::vector<double>>(rotation).at(1));
  ASSERT_EQ(90, get<std::vector<double>>(rotation).at(2));
}

#define CONFIGURATION_PATH "//m:Device/m:Configuration"
#define COORDINATE_SYSTEMS_PATH CONFIGURATION_PATH "/m:CoordinateSystems"

TEST_F(CoordinateSystemTest, XmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/probe");

    ASSERT_XML_PATH_COUNT(doc, COORDINATE_SYSTEMS_PATH, 1);
    ASSERT_XML_PATH_COUNT(doc, COORDINATE_SYSTEMS_PATH "/*", 2);

    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']@type",
                          "WORLD");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']@name",
                          "worldy");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']/m:Origin",
                          "101 102 103");
    ASSERT_XML_PATH_EQUAL(
        doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']@nativeName", nullptr);
    ASSERT_XML_PATH_EQUAL(
        doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='world']@parentIdRef", nullptr);

    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']@type",
                          "MACHINE");
    ASSERT_XML_PATH_EQUAL(doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']@name",
                          "machiney");
    ASSERT_XML_PATH_EQUAL(
        doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']@nativeName", "xxx");
    ASSERT_XML_PATH_EQUAL(
        doc, COORDINATE_SYSTEMS_PATH "/m:CoordinateSystem[@id='machine']@parentIdRef", "world");
    ASSERT_XML_PATH_EQUAL(doc,
                          COORDINATE_SYSTEMS_PATH
                          "/m:CoordinateSystem[@id='machine']/m:Transformation/m:Translation",
                          "10 10 10");
    ASSERT_XML_PATH_EQUAL(doc,
                          COORDINATE_SYSTEMS_PATH
                          "/m:CoordinateSystem[@id='machine']/m:Transformation/m:Rotation",
                          "90 0 90");
  }
}

TEST_F(CoordinateSystemTest, JsonPrinting)
{
  {
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
