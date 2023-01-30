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
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace entity;
using namespace device_model;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class SpecificationTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/configuration.xml", 8, 4, "1.7", 25);
    auto device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
    m_component = device->getComponentById("c");
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
    m_component.reset();
  }

  mtconnect::source::adapter::Adapter *m_adapter {nullptr};
  ComponentPtr m_component;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(SpecificationTest, ParseDeviceAndComponentRelationships)
{
  ASSERT_NE(nullptr, m_component);

  auto &ent = m_component->get<EntityPtr>("Configuration");
  ASSERT_TRUE(ent);

  const auto &specs = ent->getList("Specifications");
  ASSERT_TRUE(specs);
  ASSERT_EQ(3, specs->size());

  auto it = specs->begin();

  EXPECT_EQ("spec", (*it)->get<string>("id"));
  EXPECT_EQ("ROTARY_VELOCITY", (*it)->get<string>("type"));
  EXPECT_EQ("ACTUAL", (*it)->get<string>("subType"));
  EXPECT_EQ("REVOLUTION/MINUTE", (*it)->get<string>("units"));
  EXPECT_EQ("speed_limit", (*it)->get<string>("name"));
  EXPECT_EQ("cmotor", (*it)->get<string>("compositionIdRef"));
  EXPECT_EQ("machine", (*it)->get<string>("coordinateSystemIdRef"));
  EXPECT_EQ("c1", (*it)->get<string>("dataItemIdRef"));
  EXPECT_EQ("Specification", (*it)->getName());

  EXPECT_EQ(10000.0, get<double>((*it)->getProperty("Maximum")));
  EXPECT_EQ(100.0, get<double>((*it)->getProperty("Minimum")));
  EXPECT_EQ(1000.0, get<double>((*it)->getProperty("Nominal")));
}

TEST_F(SpecificationTest, test_1_6_specification_without_id)
{
  m_agentTestHelper = make_unique<AgentTestHelper>();
  m_agentTestHelper->createAgent("/samples/configuration1.6.xml", 8, 4, "1.6", 25);
  auto device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  m_component = device->getComponentById("power");

  auto &ent = m_component->get<EntityPtr>("Configuration");
  ASSERT_TRUE(ent);

  const auto &specs = ent->getList("Specifications");
  ASSERT_TRUE(specs);
  ASSERT_EQ(1, specs->size());

  auto it = specs->begin();

  EXPECT_EQ("VOLTAGE_AC", (*it)->get<string>("type"));
  EXPECT_EQ("VOLT", (*it)->get<string>("units"));
  EXPECT_EQ("voltage", (*it)->get<string>("name"));
}

#define CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define SPECIFICATIONS_PATH CONFIGURATION_PATH "/m:Specifications"

TEST_F(SpecificationTest, XmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH, 1);
    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH "/*", 3);

    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']@type",
                          "ROTARY_VELOCITY");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']@subType",
                          "ACTUAL");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']@units",
                          "REVOLUTION/MINUTE");
    ASSERT_XML_PATH_EQUAL(
        doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']@compositionIdRef",
        "cmotor");
    ASSERT_XML_PATH_EQUAL(
        doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']@coordinateSystemIdRef",
        "machine");
    ASSERT_XML_PATH_EQUAL(
        doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']@dataItemIdRef", "c1");

    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']/*", 3);
    ASSERT_XML_PATH_EQUAL(
        doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']/m:Maximum", "10000");
    ASSERT_XML_PATH_EQUAL(
        doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']/m:Minimum", "100");
    ASSERT_XML_PATH_EQUAL(
        doc, SPECIFICATIONS_PATH "/m:Specification[@name='speed_limit']/m:Nominal", "1000");
  }
}

TEST_F(SpecificationTest, XmlPrintingForLoadSpec)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    // Load spec
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']@type", "LOAD");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']@units",
                          "PERCENT");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']@name",
                          "loadspec");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']@originator",
                          "MANUFACTURER");

    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']/*", 7);
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']/m:Maximum",
                          "1000");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']/m:Minimum",
                          "-1000");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']/m:Nominal",
                          "100");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']/m:UpperLimit",
                          "500");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']/m:LowerLimit",
                          "-500");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']/m:UpperWarning",
                          "200");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification[@id='spec1']/m:LowerWarning",
                          "-200");
  }
}

TEST_F(SpecificationTest, JsonPrinting)
{
  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto rotary = device.at("/Components/0/Axes/Components/0/Rotary"_json_pointer);
    auto specifications = rotary.at("/Configuration/Specifications"_json_pointer);
    ASSERT_TRUE(specifications.is_array());
    ASSERT_EQ(3_S, specifications.size());

    auto crel = specifications.at(0);
    auto cfields = crel.at("/Specification"_json_pointer);
    EXPECT_EQ("ROTARY_VELOCITY", cfields["type"]);
    EXPECT_EQ("ACTUAL", cfields["subType"]);
    EXPECT_EQ("REVOLUTION/MINUTE", cfields["units"]);
    EXPECT_EQ("speed_limit", cfields["name"]);
    EXPECT_EQ("cmotor", cfields["compositionIdRef"]);
    EXPECT_EQ("machine", cfields["coordinateSystemIdRef"]);
    EXPECT_EQ("c1", cfields["dataItemIdRef"]);

    EXPECT_EQ(10000.0, cfields["Maximum"]);
    EXPECT_EQ(100.0, cfields["Minimum"]);
    EXPECT_EQ(1000.0, cfields["Nominal"]);
  }
}

TEST_F(SpecificationTest, JsonPrintingForLoadSpec)
{
  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto rotary = device.at("/Components/0/Axes/Components/0/Rotary"_json_pointer);
    auto specifications = rotary.at("/Configuration/Specifications"_json_pointer);
    ASSERT_TRUE(specifications.is_array());
    ASSERT_EQ(3_S, specifications.size());

    auto crel = specifications.at(1);
    auto cfields = crel.at("/Specification"_json_pointer);
    EXPECT_EQ("spec1", cfields["id"]);
    EXPECT_EQ("LOAD", cfields["type"]);
    EXPECT_EQ("PERCENT", cfields["units"]);
    EXPECT_EQ("loadspec", cfields["name"]);
    EXPECT_EQ("MANUFACTURER", cfields["originator"]);

    EXPECT_EQ(1000.0, cfields["Maximum"]);
    EXPECT_EQ(-1000.0, cfields["Minimum"]);
    EXPECT_EQ(100.0, cfields["Nominal"]);
    EXPECT_EQ(500.0, cfields["UpperLimit"]);
    EXPECT_EQ(-500.0, cfields["LowerLimit"]);
    EXPECT_EQ(200, cfields["UpperWarning"]);
    EXPECT_EQ(-200, cfields["LowerWarning"]);
  }
}

TEST_F(SpecificationTest, Parse17SpecificationValues)
{
  ASSERT_NE(nullptr, m_component);

  auto &ent = m_component->get<EntityPtr>("Configuration");
  ASSERT_TRUE(ent);

  // Get the second configuration.
  const auto &specs = ent->getList("Specifications");
  ASSERT_TRUE(specs);
  ASSERT_EQ(3, specs->size());

  auto si = specs->begin();

  // Advance to second specificaiton
  si++;

  EXPECT_EQ("Specification", (*si)->getName());

  EXPECT_EQ("spec1", (*si)->get<string>("id"));
  EXPECT_EQ("LOAD", (*si)->get<string>("type"));
  EXPECT_EQ("PERCENT", (*si)->get<string>("units"));
  EXPECT_EQ("loadspec", (*si)->get<string>("name"));
  EXPECT_EQ("MANUFACTURER", (*si)->get<string>("originator"));

  EXPECT_EQ(1000.0, (*si)->get<double>("Maximum"));
  EXPECT_EQ(-1000.0, (*si)->get<double>("Minimum"));
  EXPECT_EQ(100.0, (*si)->get<double>("Nominal"));
  EXPECT_EQ(500.0, (*si)->get<double>("UpperLimit"));
  EXPECT_EQ(-500.0, (*si)->get<double>("LowerLimit"));
  EXPECT_EQ(200.0, (*si)->get<double>("UpperWarning"));
  EXPECT_EQ(-200.0, (*si)->get<double>("LowerWarning"));
}

TEST_F(SpecificationTest, ParseProcessSpecificationValues)
{
  ASSERT_NE(nullptr, m_component);

  auto &ent = m_component->get<EntityPtr>("Configuration");
  ASSERT_TRUE(ent);

  const auto &specs = ent->getList("Specifications");
  ASSERT_TRUE(specs);
  ASSERT_EQ(3, specs->size());
  auto si = specs->begin();

  // Advance to third specificaiton
  si++;
  si++;
  EXPECT_EQ("ProcessSpecification", (*si)->getName());

  EXPECT_EQ("pspec1", (*si)->get<string>("id"));
  EXPECT_EQ("LOAD", (*si)->get<string>("type"));
  EXPECT_EQ("PERCENT", (*si)->get<string>("units"));
  EXPECT_EQ("procspec", (*si)->get<string>("name"));
  EXPECT_EQ("USER", (*si)->get<string>("originator"));

  auto specLimits = (*si)->get<EntityPtr>("SpecificationLimits");
  ASSERT_TRUE(specLimits);
  EXPECT_EQ(500.0, specLimits->get<double>("UpperLimit"));
  EXPECT_EQ(50.0, specLimits->get<double>("Nominal"));
  EXPECT_EQ(-500.0, specLimits->get<double>("LowerLimit"));

  auto control = (*si)->get<EntityPtr>("ControlLimits");
  ASSERT_TRUE(control);

  EXPECT_EQ(500.0, control->get<double>("UpperLimit"));
  EXPECT_EQ(200.0, control->get<double>("UpperWarning"));
  EXPECT_EQ(10, control->get<double>("Nominal"));
  EXPECT_EQ(-200.0, control->get<double>("LowerWarning"));
  EXPECT_EQ(-500.0, control->get<double>("LowerLimit"));

  auto alarm = (*si)->get<EntityPtr>("AlarmLimits");
  EXPECT_TRUE(alarm);

  EXPECT_EQ(500.0, alarm->get<double>("UpperLimit"));
  EXPECT_EQ(200.0, alarm->get<double>("UpperWarning"));
  EXPECT_EQ(-200.0, alarm->get<double>("LowerWarning"));
  EXPECT_EQ(-500.0, alarm->get<double>("LowerLimit"));
}

#define CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define PROCESS_PATH CONFIGURATION_PATH "/m:Specifications/m:ProcessSpecification"

TEST_F(SpecificationTest, XmlPrintingForProcessSpecification)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, PROCESS_PATH "/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "@id", "pspec1");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "@type", "LOAD");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "@units", "PERCENT");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "@originator", "USER");

    ASSERT_XML_PATH_COUNT(doc, PROCESS_PATH "/m:SpecificationLimits/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:SpecificationLimits/m:UpperLimit", "500");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:SpecificationLimits/m:LowerLimit", "-500");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:SpecificationLimits/m:Nominal", "50");

    ASSERT_XML_PATH_COUNT(doc, PROCESS_PATH "/m:ControlLimits/*", 5);
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:ControlLimits/m:UpperLimit", "500");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:ControlLimits/m:LowerLimit", "-500");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:ControlLimits/m:UpperWarning", "200");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:ControlLimits/m:LowerWarning", "-200");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:ControlLimits/m:Nominal", "10");

    ASSERT_XML_PATH_COUNT(doc, PROCESS_PATH "/m:AlarmLimits/*", 4);
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:AlarmLimits/m:UpperLimit", "500");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:AlarmLimits/m:LowerLimit", "-500");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:AlarmLimits/m:UpperWarning", "200");
    ASSERT_XML_PATH_EQUAL(doc, PROCESS_PATH "/m:AlarmLimits/m:LowerWarning", "-200");
  }
}

TEST_F(SpecificationTest, JsonPrintingForProcessSpecification)
{
  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto rotary = device.at("/Components/0/Axes/Components/0/Rotary"_json_pointer);
    auto specifications = rotary.at("/Configuration/Specifications"_json_pointer);
    ASSERT_TRUE(specifications.is_array());
    ASSERT_EQ(3_S, specifications.size());

    auto crel = specifications.at(2);
    auto cfields = crel.at("/ProcessSpecification"_json_pointer);
    EXPECT_EQ("pspec1", cfields["id"]);
    EXPECT_EQ("LOAD", cfields["type"]);
    EXPECT_EQ("PERCENT", cfields["units"]);
    EXPECT_EQ("procspec", cfields["name"]);
    EXPECT_EQ("USER", cfields["originator"]);

    auto specs = cfields["SpecificationLimits"];
    EXPECT_EQ(500.0, specs["UpperLimit"]);
    EXPECT_EQ(50.0, specs["Nominal"]);
    EXPECT_EQ(-500.0, specs["LowerLimit"]);

    auto control = cfields["ControlLimits"];
    EXPECT_EQ(500.0, control["UpperLimit"]);
    EXPECT_EQ(10.0, control["Nominal"]);
    EXPECT_EQ(-500.0, control["LowerLimit"]);
    EXPECT_EQ(200.0, control["UpperWarning"]);
    EXPECT_EQ(-200.0, control["LowerWarning"]);

    auto alarm = cfields["AlarmLimits"];
    EXPECT_EQ(500.0, alarm["UpperLimit"]);
    EXPECT_EQ(-500.0, alarm["LowerLimit"]);
    EXPECT_EQ(200.0, alarm["UpperWarning"]);
    EXPECT_EQ(-200.0, alarm["LowerWarning"]);
  }
}
