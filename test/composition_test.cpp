// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "device_model/specifications.hpp"
#include "device_model/composition.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <set>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class CompositionTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/configuration.xml",
                                   8, 4, "1.5", 25);

    auto device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
    m_component = device->getComponentById("power");
    m_composition = m_component->getCompositions().front().get();
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
  }

  Component *m_component{nullptr};
  Composition *m_composition{nullptr};

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(CompositionTest, ParseDeviceAndComponentRelationships)
{
  ASSERT_NE(nullptr, m_composition);
  
  ASSERT_EQ(1, m_composition->getConfiguration().size());
  auto ci = m_composition->getConfiguration().begin();
  
  // Get the second configuration.
  const auto conf = ci->get();
  ASSERT_EQ(typeid(Specifications), typeid(*conf));
  
  const auto specs = dynamic_cast<const Specifications*>(conf);
  ASSERT_NE(nullptr, specs);
  ASSERT_EQ(1, specs->getSpecifications().size());

  const auto &spec = specs->getSpecifications().front();
  EXPECT_EQ("VOLTAGE_AC", spec->m_type);
  EXPECT_EQ("VOLT", spec->m_units);
  EXPECT_EQ("voltage", spec->m_name);
  EXPECT_EQ("Specification", spec->getClass());
  EXPECT_FALSE(spec->hasGroups());

  EXPECT_EQ(10000.0, spec->getLimit("Maximum"));
  EXPECT_EQ(100.0, spec->getLimit("Minimum"));
  EXPECT_EQ(1000.0, spec->getLimit("Nominal"));
}

#define COMPOSITION_PATH  "//m:Power[@id='power']/m:Compositions/m:Composition[@id='zmotor']"
#define CONFIGURATION_PATH COMPOSITION_PATH "/m:Configuration"
#define SPECIFICATIONS_PATH CONFIGURATION_PATH "/m:Specifications"

TEST_F(CompositionTest, XmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/probe");

    ASSERT_XML_PATH_COUNT(doc, COMPOSITION_PATH , 1);

    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH , 1);
    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH "/*" , 1);

    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@type" , "VOLTAGE_AC");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@units" , "VOLT");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@name" , "voltage");

    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH "/m:Specification/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Maximum" , "10000");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Minimum" , "100");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Nominal" , "1000");
  }
}

TEST_F(CompositionTest, JsonPrinting)
{
  {
    m_agentTestHelper->m_request.m_accepts = "Application/json";
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

