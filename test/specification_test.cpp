// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "specifications.hpp"

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

class SpecificationTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_checkpoint = nullptr;
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/configuration.xml", 4, 4, "1.5");
    m_agentId = int64ToString(getCurrentTimeInSec());
    m_adapter = nullptr;

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();

    auto device = m_agent->getDeviceByName("LinuxCNC");
    m_component = device->getComponentById("c");
  }

  void TearDown() override
  {
    m_agent.reset();
    m_checkpoint.reset();
    m_agentTestHelper.reset();
  }

  std::unique_ptr<Checkpoint> m_checkpoint;
  std::unique_ptr<Agent> m_agent;
  Adapter *m_adapter{nullptr};
  std::string m_agentId;
  Component *m_component{nullptr};

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(SpecificationTest, ParseDeviceAndComponentRelationships)
{
  ASSERT_NE(nullptr, m_component);
  
  ASSERT_EQ(2, m_component->getConfiguration().size());
    
  auto ci = m_component->getConfiguration().begin();
  // Get the second configuration.
  ci++;
  const auto conf = ci->get();
  ASSERT_EQ(typeid(Specifications), typeid(*conf));
  
  const auto specs = dynamic_cast<const Specifications*>(conf);
  ASSERT_NE(nullptr, specs);
  ASSERT_EQ(3, specs->getSpecifications().size());

  const auto &spec = specs->getSpecifications().front();
  EXPECT_EQ("ROTARY_VELOCITY", spec->m_type);
  EXPECT_EQ("ACTUAL", spec->m_subType);
  EXPECT_EQ("REVOLUTION/MINUTE", spec->m_units);
  EXPECT_EQ("speed_limit", spec->m_name);
  EXPECT_EQ("cmotor", spec->m_compositionIdRef);
  EXPECT_EQ("machine", spec->m_coordinateSystemIdRef);
  EXPECT_EQ("c1", spec->m_dataItemIdRef);
  EXPECT_EQ("Specification", spec->getClass());
  EXPECT_FALSE(spec->hasGroups());

  EXPECT_EQ(10000.0, spec->getLimit("Limits", "Maximum"));
  EXPECT_EQ(100.0, spec->getLimit("Limits", "Minimum"));
  EXPECT_EQ(1000.0, spec->getLimit("Limits", "Nominal"));
}


#define CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define SPECIFICATIONS_PATH CONFIGURATION_PATH "/m:Specifications"

TEST_F(SpecificationTest, XmlPrinting)
{
  m_agentTestHelper->m_path = "/probe";
  {
    PARSE_XML_RESPONSE;
    
    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH , 1);
    ASSERT_XML_PATH_COUNT(doc, SPECIFICATIONS_PATH "/*" , 3);

    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@type" , "ROTARY_VELOCITY");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@subType" , "ACTUAL");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@units" , "REVOLUTION/MINUTE");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@name" , "speed_limit");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@compositionIdRef" , "cmotor");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@coordinateSystemIdRef" , "machine");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification@dataItemIdRef" , "c1");
    
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Maximum" , "10000");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Minimum" , "100");
    ASSERT_XML_PATH_EQUAL(doc, SPECIFICATIONS_PATH "/m:Specification/m:Nominal" , "1000");
  }
}

TEST_F(SpecificationTest, JsonPrinting)
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  ASSERT_TRUE(m_adapter);
  
  m_agentTestHelper->m_path = "/probe";
  m_agentTestHelper->m_incomingHeaders["Accept"] = "Application/json";
  
  {
    PARSE_JSON_RESPONSE;
    
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

TEST_F(SpecificationTest, Parse17SpecificationValues)
{
  ASSERT_NE(nullptr, m_component);
  
  ASSERT_EQ(2, m_component->getConfiguration().size());
  
  auto ci = m_component->getConfiguration().begin();
  // Get the second configuration.
  ci++;
  const auto conf = ci->get();
  ASSERT_EQ(typeid(Specifications), typeid(*conf));
  
  const auto specs = dynamic_cast<const Specifications*>(conf);
  ASSERT_NE(nullptr, specs);
  ASSERT_EQ(3, specs->getSpecifications().size());
  
  auto si = specs->getSpecifications().begin();
  
  // Advance to second specificaiton
  si++;

  EXPECT_EQ("Specification", (*si)->getClass());

  EXPECT_EQ("spec1", (*si)->m_id);
  EXPECT_EQ("LOAD", (*si)->m_type);
  EXPECT_EQ("PERCENT", (*si)->m_units);
  EXPECT_EQ("loadspec", (*si)->m_name);
  EXPECT_EQ("MANUFACTURER", (*si)->m_originator);
  
  EXPECT_FALSE((*si)->hasGroups());

  EXPECT_EQ(1000.0, (*si)->getLimit("Limits", "Maximum"));
  EXPECT_EQ(-1000.0, (*si)->getLimit("Limits", "Minimum"));
  EXPECT_EQ(100.0, (*si)->getLimit("Limits", "Nominal"));
  EXPECT_EQ(500.0, (*si)->getLimit("Limits", "UpperLimit"));
  EXPECT_EQ(-500.0, (*si)->getLimit("Limits", "LowerLimit"));
  EXPECT_EQ(200.0, (*si)->getLimit("Limits", "UpperWarning"));
  EXPECT_EQ(-200.0, (*si)->getLimit("Limits", "LowerWarning"));
}

TEST_F(SpecificationTest, ParseProcessSpecificationValues)
{
  ASSERT_NE(nullptr, m_component);
  
  ASSERT_EQ(2, m_component->getConfiguration().size());
  
  auto ci = m_component->getConfiguration().begin();
  // Get the second configuration.
  ci++;
  const auto conf = ci->get();
  ASSERT_EQ(typeid(Specifications), typeid(*conf));
  
  const auto specs = dynamic_cast<const Specifications*>(conf);
  ASSERT_NE(nullptr, specs);
  ASSERT_EQ(3, specs->getSpecifications().size());
  
  auto si = specs->getSpecifications().begin();
  
  // Advance to third specificaiton
  si++; si++;
  EXPECT_EQ("ProcessSpecification", (*si)->getClass());
  
  EXPECT_EQ("pspec1", (*si)->m_id);
  EXPECT_EQ("LOAD", (*si)->m_type);
  EXPECT_EQ("PERCENT", (*si)->m_units);
  EXPECT_EQ("procspec", (*si)->m_name);
  EXPECT_EQ("USER", (*si)->m_originator);
  
  EXPECT_TRUE((*si)->hasGroups());
  auto groups = (*si)->getGroupKeys();
  std::set<string> expected = { "SpecificationLimits", "AlarmLimits", "ControlLimits" };
  ASSERT_EQ(expected, groups);
  
  const auto spec = (*si)->getGroup("SpecificationLimits");
  EXPECT_TRUE(spec);
  
  EXPECT_EQ(500.0, spec->find("UpperLimit")->second);
  EXPECT_EQ(50.0, spec->find("Nominal")->second);
  EXPECT_EQ(-500.0, spec->find("LowerLimit")->second);

  const auto control = (*si)->getGroup("ControlLimits");
  EXPECT_TRUE(control);
  
  EXPECT_EQ(500.0, control->find("UpperLimit")->second);
  EXPECT_EQ(200.0, control->find("UpperWarning")->second);
  EXPECT_EQ(10, control->find("Nominal")->second);
  EXPECT_EQ(-200.0, control->find("LowerWarning")->second);
  EXPECT_EQ(-500.0, control->find("LowerLimit")->second);

  const auto alarm = (*si)->getGroup("ControlLimits");
  EXPECT_TRUE(alarm);
  
  EXPECT_EQ(500.0, alarm->find("UpperLimit")->second);
  EXPECT_EQ(200.0, alarm->find("UpperWarning")->second);
  EXPECT_EQ(-200.0, alarm->find("LowerWarning")->second);
  EXPECT_EQ(-500.0, alarm->find("LowerLimit")->second);
}
