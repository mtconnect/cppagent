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
#include <set>
#include <sstream>
#include <string>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/device_model/composition.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace device_model;
using namespace entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class CompositionTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/configuration.xml", 8, 4, "1.5", 25);

    auto device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
    m_component = device->getComponentById("power");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  ComponentPtr m_component;

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(CompositionTest, ParseDeviceAndComponentRelationships)
{
  using namespace mtconnect::entity;
  ASSERT_NE(nullptr, m_component);

  const auto &compositions = m_component->getList("Compositions");
  ASSERT_TRUE(compositions);

  ASSERT_EQ(1, compositions->size());
  auto composition = compositions->begin();

  EXPECT_EQ("Composition", (*composition)->getName());

  EXPECT_EQ("zmotor", get<string>((*composition)->getProperty("id")));
  EXPECT_EQ("MOTOR", get<string>((*composition)->getProperty("type")));
  EXPECT_EQ("12345", get<string>((*composition)->getProperty("uuid")));
  EXPECT_EQ("motor_name", get<string>((*composition)->getProperty("name")));

  auto description = (*composition)->get<entity::EntityPtr>("Description");

  EXPECT_EQ("open", get<string>(description->getProperty("manufacturer")));
  EXPECT_EQ("vroom", get<string>(description->getProperty("model")));
  EXPECT_EQ("12356", get<string>(description->getProperty("serialNumber")));
  EXPECT_EQ("A", get<string>(description->getProperty("station")));
  EXPECT_EQ("Hello There", get<string>(description->getValue()));

  const auto &configuration = (*composition)->get<EntityPtr>("Configuration");

  auto specs = configuration->getList("Specifications");
  ASSERT_TRUE(specs);
  ASSERT_EQ(1, specs->size());

  auto it = specs->begin();

  EXPECT_EQ("Specification", (*it)->getName());
  EXPECT_EQ("spec2", get<string>((*it)->getProperty("id")));
  EXPECT_EQ("VOLTAGE_AC", get<string>((*it)->getProperty("type")));
  EXPECT_EQ("VOLT", get<string>((*it)->getProperty("units")));
  EXPECT_EQ("voltage", get<string>((*it)->getProperty("name")));

  EXPECT_EQ(10000.0, get<double>((*it)->getProperty("Maximum")));
  EXPECT_EQ(100.0, get<double>((*it)->getProperty("Minimum")));
  EXPECT_EQ(1000.0, get<double>((*it)->getProperty("Nominal")));
}

#define COMPOSITION_PATH "//m:Power[@id='power']/m:Compositions/m:Composition[@id='zmotor']"
#define CONFIGURATION_PATH COMPOSITION_PATH "/m:Configuration"
#define SPECIFICATIONS_PATH CONFIGURATION_PATH "/m:Specifications"

TEST_F(CompositionTest, XmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/probe");

    ASSERT_XML_PATH_COUNT(doc, COMPOSITION_PATH, 1);

    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH, 1);
    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH "/*", 1);

    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@type", "VOLTAGE_AC");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@units", "VOLT");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@name", "voltage");

    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH "/m:Specification/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Maximum", "10000");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Minimum", "100");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Nominal", "1000");
  }
}

TEST_F(CompositionTest, JsonPrinting)
{
  {
    PARSE_JSON_RESPONSE("/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto composition = device.at("/Components/2/Power/Compositions/0/Composition"_json_pointer);
    auto specifications = composition.at("/Configuration/Specifications"_json_pointer);
    ASSERT_TRUE(specifications.is_array());
    ASSERT_EQ(1_S, specifications.size());

    auto crel = specifications.at(0);
    auto cfields = crel.at("/Specification"_json_pointer);
    EXPECT_EQ("VOLTAGE_AC", cfields["type"]);
    EXPECT_EQ("VOLT", cfields["units"]);
    EXPECT_EQ("voltage", cfields["name"]);

    EXPECT_EQ(10000.0, cfields["Maximum"]);
    EXPECT_EQ(100.0, cfields["Minimum"]);
    EXPECT_EQ(1000.0, cfields["Nominal"]);
  }
}

TEST_F(CompositionTest, should_create_topic)
{
  using namespace mtconnect::entity;
  using namespace mtconnect::device_model;
  ASSERT_NE(nullptr, m_component);

  const auto &compositions = m_component->getList("Compositions");
  ASSERT_TRUE(compositions);

  auto composition = dynamic_pointer_cast<Composition>(compositions->front());
  ASSERT_TRUE(composition);
  ASSERT_EQ("Motor[motor_name]", composition->getTopicName());
}
