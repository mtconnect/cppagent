// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "relationships.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class RelationshipTest : public testing::Test
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

    std::map<string, string> attributes;
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

TEST_F(RelationshipTest, ParseDeviceAndComponentRelationships)
{
  ASSERT_NE(nullptr, m_component);
  
  ASSERT_EQ(1, m_component->getConfiguration().size());
  
  const auto conf = m_component->getConfiguration().front().get();
  ASSERT_EQ(typeid(Relationships), typeid(*conf));
  
  const Relationships *rels = dynamic_cast<const Relationships*>(m_component->getConfiguration().front().get());
  ASSERT_NE(nullptr, rels);
  ASSERT_EQ(2, rels->getRelationships().size());
  
  auto reli = rels->getRelationships().begin();
  const auto rel1 = reli->get();
  
  ASSERT_EQ(typeid(ComponentRelationship), typeid(*rel1));
  const auto crel = dynamic_cast<ComponentRelationship*>(rel1);
  EXPECT_EQ("ref1", crel->m_id);
  EXPECT_EQ("Power", crel->m_name);
  EXPECT_EQ("PEER", crel->m_type);
  EXPECT_EQ("CRITICAL", crel->m_criticality);
  EXPECT_EQ("power", crel->m_idRef);
  
  reli++;
  
  const auto rel2 = reli->get();
  
  ASSERT_EQ(typeid(DeviceRelationship), typeid(*rel2));
  const auto drel = dynamic_cast<DeviceRelationship*>(rel2);
  EXPECT_EQ("ref2", drel->m_id);
  EXPECT_EQ("coffee", drel->m_name);
  EXPECT_EQ("PARENT", drel->m_type);
  EXPECT_EQ("NON_CRITICAL", drel->m_criticality);
  EXPECT_EQ("AUXILIARY", drel->m_role);
  EXPECT_EQ("http://127.0.0.1:2000/coffee", drel->m_href);
  EXPECT_EQ("bfccbfb0-5111-0138-6cd5-0c85909298d9", drel->m_deviceUuidRef);
}

#define ASSERT_DATA_SET_ENTRY(doc, var, key, expected) \
ASSERT_XML_PATH_EQUAL(doc, "//m:" var "/m:Entry[@key='" key "']", expected)


#define CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define RELATIONSHIPS_PATH CONFIGURATION_PATH "/m:Relationships"

TEST_F(RelationshipTest, XmlPrinting)
{
  m_agentTestHelper->m_path = "/probe";
  {
    PARSE_XML_RESPONSE;
    
    ASSERT_XML_PATH_COUNT(doc, RELATIONSHIPS_PATH , 1);
    ASSERT_XML_PATH_COUNT(doc, RELATIONSHIPS_PATH "/*" , 2);

    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@id" , "ref1");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@name" , "Power");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@type" , "PEER");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@criticality" , "CRITICAL");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@idRef" , "power");

    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@id" , "ref2");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@name" , "coffee");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@type" , "PARENT");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@criticality" , "NON_CRITICAL");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@role" , "AUXILIARY");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@href" , "http://127.0.0.1:2000/coffee");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@deviceUuidRef" , "bfccbfb0-5111-0138-6cd5-0c85909298d9");
  }
}

TEST_F(RelationshipTest, JsonPrinting)
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
    auto relationships = rotary.at("/Configuration/Relationships"_json_pointer);
    ASSERT_TRUE(relationships.is_array());
    ASSERT_EQ(2_S, relationships.size());

    auto crel = relationships.at(0);
    auto cfields = crel.at("/ComponentRelationship"_json_pointer);
    EXPECT_EQ("ref1", cfields["id"]);
    EXPECT_EQ("Power", cfields["name"]);
    EXPECT_EQ("PEER", cfields["type"]);
    EXPECT_EQ("CRITICAL", cfields["criticality"]);
    EXPECT_EQ("power", cfields["idRef"]);

    auto drel = relationships.at(1);
    auto dfields = drel.at("/DeviceRelationship"_json_pointer);
    EXPECT_EQ("ref2", dfields["id"]);
    EXPECT_EQ("coffee", dfields["name"]);
    EXPECT_EQ("PARENT", dfields["type"]);
    EXPECT_EQ("NON_CRITICAL", dfields["criticality"]);
    EXPECT_EQ("AUXILIARY", dfields["role"]);
    EXPECT_EQ("http://127.0.0.1:2000/coffee", dfields["href"]);
    EXPECT_EQ("bfccbfb0-5111-0138-6cd5-0c85909298d9", dfields["deviceUuidRef"]);

  }
}
