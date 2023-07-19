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
#include "mtconnect/entity/json_printer.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace entity;
using namespace device_model;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class RelationshipTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/configuration.xml", 8, 4, "1.7", 25);
    m_agentId = to_string(getCurrentTimeInSec());

    auto device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
    m_component = device->getComponentById("c");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  source::adapter::Adapter *m_adapter {nullptr};
  std::string m_agentId;
  ComponentPtr m_component {nullptr};

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(RelationshipTest, ParseDeviceAndComponentRelationships)
{
  ASSERT_NE(nullptr, m_component);

  const auto &clc = m_component->get<EntityPtr>("Configuration");
  ASSERT_TRUE(clc);

  auto rels = clc->getList("Relationships");
  ASSERT_EQ(3, rels->size());

  auto it = rels->begin();

  EXPECT_EQ("ComponentRelationship", (*it)->getName());
  EXPECT_EQ("ref1", (*it)->get<string>("id"));
  EXPECT_EQ("Power", (*it)->get<string>("name"));
  EXPECT_EQ("PEER", (*it)->get<string>("type"));
  EXPECT_EQ("CRITICAL", (*it)->get<string>("criticality"));
  EXPECT_EQ("power", (*it)->get<string>("idRef"));

  it++;

  EXPECT_EQ("DeviceRelationship", (*it)->getName());
  EXPECT_EQ("ref2", (*it)->get<string>("id"));
  EXPECT_EQ("coffee", (*it)->get<string>("name"));
  EXPECT_EQ("PARENT", (*it)->get<string>("type"));
  EXPECT_EQ("NON_CRITICAL", (*it)->get<string>("criticality"));
  EXPECT_EQ("AUXILIARY", (*it)->get<string>("role"));
  EXPECT_EQ("http://127.0.0.1:2000/coffee", (*it)->get<string>("href"));
  EXPECT_EQ("bfccbfb0-5111-0138-6cd5-0c85909298d9", (*it)->get<string>("deviceUuidRef"));

  it++;

  EXPECT_EQ("AssetRelationship", (*it)->getName());
  EXPECT_EQ("ref3", (*it)->get<string>("id"));
  EXPECT_EQ("asset", (*it)->get<string>("name"));
  EXPECT_EQ("CuttingTool", (*it)->get<string>("assetType"));
  EXPECT_EQ("PEER", (*it)->get<string>("type"));
  EXPECT_EQ("NON_CRITICAL", (*it)->get<string>("criticality"));
  EXPECT_EQ("http://127.0.0.1:2000/asset/f7de7350-6f7a-013b-ca4c-4e7f553bbb76",
            (*it)->get<string>("href"));

  EXPECT_EQ("f7de7350-6f7a-013b-ca4c-4e7f553bbb76", (*it)->get<string>("assetIdRef"));
}

#define CONFIGURATION_PATH "//m:Rotary[@id='c']/m:Configuration"
#define RELATIONSHIPS_PATH CONFIGURATION_PATH "/m:Relationships"

TEST_F(RelationshipTest, XmlPrinting)
{
  {
    PARSE_XML_RESPONSE("/probe");

    ASSERT_XML_PATH_COUNT(doc, RELATIONSHIPS_PATH, 1);
    ASSERT_XML_PATH_COUNT(doc, RELATIONSHIPS_PATH "/*", 3);

    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@id", "ref1");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@name", "Power");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@type", "PEER");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@criticality",
                          "CRITICAL");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:ComponentRelationship@idRef", "power");

    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@id", "ref2");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@name", "coffee");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@type", "PARENT");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@criticality",
                          "NON_CRITICAL");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@role", "AUXILIARY");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@href",
                          "http://127.0.0.1:2000/coffee");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:DeviceRelationship@deviceUuidRef",
                          "bfccbfb0-5111-0138-6cd5-0c85909298d9");

    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:AssetRelationship@id", "ref3");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:AssetRelationship@name", "asset");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:AssetRelationship@type", "PEER");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:AssetRelationship@assetType", "CuttingTool");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:AssetRelationship@criticality",
                          "NON_CRITICAL");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:AssetRelationship@assetIdRef",
                          "f7de7350-6f7a-013b-ca4c-4e7f553bbb76");
    ASSERT_XML_PATH_EQUAL(doc, RELATIONSHIPS_PATH "/m:AssetRelationship@href",
                          "http://127.0.0.1:2000/asset/f7de7350-6f7a-013b-ca4c-4e7f553bbb76");
  }
}

TEST_F(RelationshipTest, JsonPrinting)
{
  {
    PARSE_JSON_RESPONSE("/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(1).at("/Device"_json_pointer);

    auto rotary = device.at("/Components/0/Axes/Components/0/Rotary"_json_pointer);

    auto relationships = rotary.at("/Configuration/Relationships"_json_pointer);
    ASSERT_TRUE(relationships.is_array());
    ASSERT_EQ(3_S, relationships.size());

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

    auto arel = relationships.at(2);
    auto afields = arel.at("/AssetRelationship"_json_pointer);
    EXPECT_EQ("ref3", afields["id"]);
    EXPECT_EQ("asset", afields["name"]);
    EXPECT_EQ("PEER", afields["type"]);
    EXPECT_EQ("CuttingTool", afields["assetType"]);
    EXPECT_EQ("NON_CRITICAL", afields["criticality"]);
    EXPECT_EQ("http://127.0.0.1:2000/asset/f7de7350-6f7a-013b-ca4c-4e7f553bbb76", afields["href"]);
    EXPECT_EQ("f7de7350-6f7a-013b-ca4c-4e7f553bbb76", afields["assetIdRef"]);
  }
}
