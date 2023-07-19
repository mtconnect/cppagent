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

class AssetHashTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/solid_model.xml", 8, 4, "2.2", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  void addAdapter(ConfigOptions options = ConfigOptions {})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 7878,
                                  m_agentTestHelper->m_agent->getDefaultDevice()->getName());
  }

  Adapter *m_adapter {nullptr};
  std::string m_agentId;
  DevicePtr m_device;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(AssetHashTest, should_assign_hash_when_receiving_asset)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  m_agentTestHelper->m_adapter->parseBuffer(
      R"("2021-02-01T12:00:00Z|@ASSET@|P1|Part|--multiline--AAAA
<Part assetId='P1'>
  <PartXXX>TEST 1</PartXXX>
    Some Text
  <Extra>XXX</Extra>
</Part>
--multiline--AAAA
)");
  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)1, storage->getCount());

  auto asset = storage->getAsset("P1");
  auto hash = asset->get<string>("hash");

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:PartXXX", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:Extra", "XXX");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@deviceUuid", "000");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "2021-02-01T12:00:00Z");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@hash", hash.c_str());
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@hash", hash.c_str());
  }

  m_agentTestHelper->m_adapter->parseBuffer(
      R"("2023-02-01T12:00:00Z|@ASSET@|P1|Part|--multiline--AAAA
<Part assetId='P1'>
  <PartXXX>TEST 1</PartXXX>
    Some Text
  <Extra>XXX</Extra>
</Part>
--multiline--AAAA
)");

  auto asset2 = storage->getAsset("P1");
  auto hash2 = asset2->get<string>("hash");

  ASSERT_EQ(hash, hash2);

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "2023-02-01T12:00:00Z");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@hash", hash.c_str());
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@hash", hash.c_str());
  }
}

TEST_F(AssetHashTest, hash_should_change_when_doc_changes)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  m_agentTestHelper->m_adapter->parseBuffer(
      R"("2021-02-01T12:00:00Z|@ASSET@|P1|Part|--multiline--AAAA
<Part assetId='P1'>
  <PartXXX>TEST 1</PartXXX>
    Some Text
  <Extra>XXX</Extra>
</Part>
--multiline--AAAA
)");
  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)1, storage->getCount());

  auto asset = storage->getAsset("P1");
  auto hash = asset->get<string>("hash");

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:PartXXX", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:Extra", "XXX");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@deviceUuid", "000");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "2021-02-01T12:00:00Z");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@hash", hash.c_str());
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@hash", hash.c_str());
  }

  m_agentTestHelper->m_adapter->parseBuffer(
      R"("2023-02-01T12:00:00Z|@ASSET@|P1|Part|--multiline--AAAA
<Part assetId='P1'>
  <PartXXX>TEST 2</PartXXX>
    Some Text
  <Extra>XXX</Extra>
</Part>
--multiline--AAAA
)");

  auto asset2 = storage->getAsset("P1");
  auto hash2 = asset2->get<string>("hash");

  ASSERT_NE(hash, hash2);

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "2023-02-01T12:00:00Z");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@hash", hash2.c_str());
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@hash", hash2.c_str());
  }
}
