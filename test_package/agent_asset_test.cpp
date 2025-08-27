//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

/// @file
/// Integration tests for the agent. Covers many behaviours of the agent across many modules.

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "agent_test_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/asset/file_asset.hpp"
#include "mtconnect/device_model/reference.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "test_utilities.hpp"

#if defined(WIN32) && _MSC_VER < 1500
typedef __int64 int64_t;
#endif

// Registers the fixture into the 'registry'
using namespace std;
using namespace std::chrono;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;
using namespace mtconnect::source::adapter;
using namespace mtconnect::observation;

using status = boost::beast::http::status;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class AgentAssetTest : public testing::Test
{
public:
  typedef std::map<std::string, std::string> map_type;
  using queue_type = list<string>;

protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 25, true);
    m_agentId = to_string(getCurrentTimeInSec());
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  void addAdapter(ConfigOptions options = ConfigOptions {})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 7878,
                                  m_agentTestHelper->m_agent->getDefaultDevice()->getName());
  }

public:
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;

  std::chrono::milliseconds m_delay {};
};

// ------------------------- Asset Tests ---------------------------------

TEST_F(AgentAssetTest, should_store_assets_in_buffer)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);

  auto rest = m_agentTestHelper->getRestService();
  ASSERT_TRUE(rest->getServer()->arePutsAllowed());
  string body = "<Part assetId='P1' deviceUuid='LinuxCNC'>TEST</Part>";
  QueryMap queries;

  queries["type"] = "Part";
  queries["device"] = "LinuxCNC";

  ASSERT_EQ((unsigned int)4, agent->getAssetStorage()->getMaxAssets());
  ASSERT_EQ((unsigned int)0, agent->getAssetStorage()->getCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset/123", body, queries);
    ASSERT_EQ((unsigned int)1, agent->getAssetStorage()->getCount());
  }

  {
    PARSE_XML_RESPONSE("/asset/123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetBufferSize", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST");
  }

  // The device should generate an asset changed event as well.
  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged", "123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@assetType", "Part");
  }
}

TEST_F(AgentAssetTest, should_store_assets_in_buffer_and_generate_asset_added)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, true);

  auto rest = m_agentTestHelper->getRestService();
  ASSERT_TRUE(rest->getServer()->arePutsAllowed());
  string body = "<Part assetId='P1' deviceUuid='LinuxCNC'>TEST</Part>";
  QueryMap queries;

  queries["type"] = "Part";
  queries["device"] = "LinuxCNC";

  ASSERT_EQ((unsigned int)4, agent->getAssetStorage()->getMaxAssets());
  ASSERT_EQ((unsigned int)0, agent->getAssetStorage()->getCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset/123", body, queries);
    ASSERT_EQ((unsigned int)1, agent->getAssetStorage()->getCount());
  }

  {
    PARSE_XML_RESPONSE("/asset/123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetBufferSize", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST");
  }

  // The device should generate an asset added event as well.
  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetAdded", "123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetAdded@assetType", "Part");
  }
}

TEST_F(AgentAssetTest, should_handle_asset_buffer_and_buffer_limits)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);
  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;

  queries["device"] = "000";
  queries["type"] = "Part";

  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)0, storage->getCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
    ASSERT_EQ(1, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
    ASSERT_EQ(1, storage->getCountForType("Part"));
  }

  body = "<Part assetId='P2'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)2, storage->getCount());
    ASSERT_EQ(2, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  body = "<Part assetId='P3'>TEST 3</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)3, storage->getCount());
    ASSERT_EQ(3, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  body = "<Part assetId='P4'>TEST 4</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/asset/P4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 4");
    ASSERT_EQ(4, storage->getCountForType("Part"));
  }

  // Test multiple asset get
  {
    PARSE_XML_RESPONSE("/assets");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 4");
  }

  // Test multiple asset get with filter
  {
    PARSE_XML_RESPONSE_QUERY("/asset", queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 1");
  }

  queries["count"] = "2";
  {
    PARSE_XML_RESPONSE_QUERY("/assets", queries);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 2");
  }

  queries.erase("count");

  body = "<Part assetId='P5'>TEST 5</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, storage->getCount());
    ASSERT_EQ(4, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P5");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 5");
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Cannot find asset: P1");
  }

  body = "<Part assetId='P3'>TEST 6</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, storage->getCount());
    ASSERT_EQ(4, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 6");
  }

  {
    PARSE_XML_RESPONSE("/asset/P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  body = "<Part assetId='P2'>TEST 7</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, storage->getCount());
    ASSERT_EQ(4, storage->getCountForType("Part"));
  }

  body = "<Part assetId='P6'>TEST 8</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)4, storage->getCount());
    ASSERT_EQ(4, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P6");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 8");
  }

  // Now since two and three have been modified, asset 4 should be removed.
  {
    PARSE_XML_RESPONSE("/asset/P4");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Cannot find asset: P4");
  }
}

TEST_F(AgentAssetTest, should_report_asset_not_found_error)
{
  {
    PARSE_XML_RESPONSE("/asset/123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", "Cannot find asset: 123");
  }
}

TEST_F(AgentAssetTest, should_report_asset_not_found_2_6_error)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false, true,
                                 {{configuration::Validation, false}});

  {
    PARSE_XML_RESPONSE("/asset/123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound/m:ErrorMessage", "Cannot find asset: 123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound/m:AssetId", "123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound/m:URI", "/asset/123");
  }
}

TEST_F(AgentAssetTest, should_report_asset_not_found_2_6_error_with_multiple_assets)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, false, true,
                                 {{configuration::Validation, false}});

  {
    PARSE_XML_RESPONSE("/asset/123;456");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Errors/m:AssetNotFound[1]@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound[1]/m:ErrorMessage", "Cannot find asset: 123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound[1]/m:AssetId", "123");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound[1]/m:URI", "/asset/123;456");

    ASSERT_XML_PATH_EQUAL(doc, "//m:Errors/m:AssetNotFound[2]@errorCode", "ASSET_NOT_FOUND");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound[2]/m:ErrorMessage", "Cannot find asset: 456");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound[2]/m:AssetId", "456");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetNotFound[2]/m:URI", "/asset/123;456");
  }
}

TEST_F(AgentAssetTest, should_handle_asset_from_adapter_on_one_line)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }
}

TEST_F(AgentAssetTest, should_handle_multiline_asset)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  m_agentTestHelper->m_adapter->parseBuffer(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|--multiline--AAAA\n");
  m_agentTestHelper->m_adapter->parseBuffer(
      "<Part assetId='P1'>\n"
      "  <PartXXX>TEST 1</PartXXX>\n"
      "  Some Text\n"
      "  <Extra>XXX</Extra>\n");
  m_agentTestHelper->m_adapter->parseBuffer(
      "</Part>\n"
      "--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:PartXXX", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:Extra", "XXX");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@deviceUuid", "000");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "2021-02-01T12:00:00Z");
  }

  // Make sure we can still add a line and we are out of multiline mode...
  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", "204");
  }
}

TEST_F(AgentAssetTest, should_handle_bad_asset_from_adapter)
{
  addAdapter();
  const auto &storage = m_agentTestHelper->m_agent->getAssetStorage();

  m_agentTestHelper->m_adapter->parseBuffer(
      "2021-02-01T12:00:00Z|@ASSET@|111|CuttingTool|--multiline--AAAA\n");
  m_agentTestHelper->m_adapter->parseBuffer((getFile("asset4.xml") + "\n").c_str());
  m_agentTestHelper->m_adapter->parseBuffer("--multiline--AAAA\n");
  ASSERT_EQ((unsigned int)0, storage->getCount());
}

TEST_F(AgentAssetTest, should_handle_asset_removal_from_REST_api)
{
  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap query;

  query["device"] = "LinuxCNC";
  query["type"] = "Part";

  const auto &storage = m_agentTestHelper->m_agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)0, storage->getCount());

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)1, storage->getCount());
    ASSERT_EQ(1, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
  }

  // Make sure replace works properly
  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)1, storage->getCount());
    ASSERT_EQ(1, storage->getCountForType("Part"));
  }

  body = "<Part assetId='P2'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)2, storage->getCount());
    ASSERT_EQ(2, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
  }

  body = "<Part assetId='P3'>TEST 3</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)3, storage->getCount());
    ASSERT_EQ(3, storage->getCountForType("Part"));
  }

  {
    PARSE_XML_RESPONSE("/asset/P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
  }

  body = "<Part assetId='P2' removed='true'>TEST 2</Part>";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, query);
    ASSERT_EQ((unsigned int)3, storage->getCount(false));
    ASSERT_EQ(3, storage->getCountForType("Part", false));
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }

  {
    PARSE_XML_RESPONSE("/asset");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }

  query["removed"] = "true";
  {
    PARSE_XML_RESPONSE_QUERY("/asset", query);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]@removed", "true");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 3");
  }
}

TEST_F(AgentAssetTest, should_handle_asset_removal_from_adapter)
{
  addAdapter();
  QueryMap query;
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P2|Part|<Part assetId='P2'>TEST 2</Part>");
  ASSERT_EQ((unsigned int)2, storage->getCount());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P3|Part|<Part assetId='P3'>TEST 3</Part>");
  ASSERT_EQ((unsigned int)3, storage->getCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P2\r");
  ASSERT_EQ((unsigned int)3, storage->getCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }

  {
    PARSE_XML_RESPONSE("/asset");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }

  // TODO: When asset is removed and the content is literal, it will
  // not regenerate the attributes for the asset.
  query["removed"] = "true";
  {
    PARSE_XML_RESPONSE_QUERY("/asset", query);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }
}

TEST_F(AgentAssetTest, should_add_asset_changed_without_discrete_in_1_3)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "1.2", 25);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", nullptr);
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 0);
  }
}

TEST_F(AgentAssetTest, should_add_asset_removed_in_1_3)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "1.3", 25);

  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", nullptr);
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
  }
}

TEST_F(AgentAssetTest, should_add_asset_changed_with_discrete_in_1_5)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "1.5", 25);
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", "true");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
  }
}

TEST_F(AgentAssetTest, should_add_asset_changed_and_asset_added_with_discrete_in_2_6)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "2.6", 25);
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_ADDED']", 1);
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_CHANGED']@discrete", "true");
    ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@type='ASSET_ADDED']@discrete", "true");
    ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
  }
}

TEST_F(AgentAssetTest, AssetPrependId)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|@1|Part|<Part assetId='1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/asset/0001");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "0001");
  }
}

TEST_F(AgentAssetTest, should_remove_changed_asset)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P1");
  ASSERT_EQ((unsigned int)1, storage->getCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }
}

TEST_F(AgentAssetTest, should_remove_changed_observation_asset_in_2_6)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "2.6", 25);

  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 2</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded@assetType", "Part");
  }
}

TEST_F(AgentAssetTest, should_remove_added_asset_observation_in_2_6)
{
  m_agentTestHelper->createAgent("/samples/min_config.xml", 8, 4, "2.6", 25);

  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P1");
  ASSERT_EQ((unsigned int)1, storage->getCount(false));

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded@assetType", "Part");
  }
}

TEST_F(AgentAssetTest, should_remove_asset_using_http_delete)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);
  addAdapter();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  {
    PARSE_XML_RESPONSE_DELETE("/asset/P1");
  }

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
  }
}

TEST_F(AgentAssetTest, asset_changed_and_removed_should_be_defaulted_to_unavailable)
{
  addAdapter();

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "UNAVAILABLE");
  }
}

TEST_F(AgentAssetTest, in_2_6_asset_changed_removed_and_added_should_be_defaulted_to_unavailable)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.6", 4, true);
  addAdapter();

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetAdded", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "UNAVAILABLE");
  }
}

TEST_F(AgentAssetTest, should_remove_all_assets)
{
  addAdapter();
  auto agent = m_agentTestHelper->getAgent();
  const auto &storage = agent->getAssetStorage();

  ASSERT_EQ((unsigned int)4, storage->getMaxAssets());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");
  ASSERT_EQ((unsigned int)1, storage->getCount());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P2|Part|<Part assetId='P2'>TEST 2</Part>");
  ASSERT_EQ((unsigned int)2, storage->getCount());

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P3|Part|<Part assetId='P3'>TEST 3</Part>");
  ASSERT_EQ((unsigned int)3, storage->getCount());

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ALL_ASSETS@|Part");
  ASSERT_EQ((unsigned int)3, storage->getCount(false));

  {
    PARSE_XML_RESPONSE("/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "P3");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
  }

  ASSERT_EQ((unsigned int)0, storage->getCount());

  {
    PARSE_XML_RESPONSE("/assets");
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 0);
  }

  // TODO: When asset is removed and the content is literal, it will
  // not regenerate the attributes for the asset.
  {
    QueryMap q {{"removed", "true"}};
    PARSE_XML_RESPONSE_QUERY("/asset", q);
    ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "0");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
  }
}

TEST_F(AgentAssetTest, probe_should_have_the_asset_counts)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);
  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;
  const auto &storage = agent->getAssetStorage();

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE_PUT("/asset/P2", body, queries);
    ASSERT_EQ((unsigned int)2, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount@assetType", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount", "2");
  }
}

TEST_F(AgentAssetTest, should_respond_to_http_push_with_list_of_errors)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.3", 4, true);

  const string body {
      R"DOC(<CuttingTool assetId="M8010N9172N:1.0" serialNumber="1234" toolId="CAT">
  <CuttingToolLifeCycle>
    <CutterStatus>
      <Status>NEW</Status>
    </CutterStatus>
    <Measurements>
      <FunctionalLength code="LF" maximum="5.2" minimum="4.95" nominal="5" units="MILLIMETER"/>
      <CuttingDiameterMax code="DC" maximum="1.4" minimum="0.95" nominal="1.25" units="MILLIMETER"/>
    </Measurements>
  </CuttingToolLifeCycle>
</CuttingTool>
)DOC"};

  QueryMap queries;

  queries["device"] = "LinuxCNC";
  queries["type"] = "CuttingTool";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[1]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[1]",
                          "Asset parsed with errors.");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[2]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[2]",
                          "FunctionalLength(VALUE): Property VALUE is required and not provided");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[3]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[3]",
                          "Measurements: Invalid element 'FunctionalLength'");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[4]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[4]",
                          "CuttingDiameterMax(VALUE): Property VALUE is required and not provided");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[5]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[5]",
                          "Measurements: Invalid element 'CuttingDiameterMax'");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[6]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[6]",
                          "Measurements(Measurement): Entity list requirement Measurement must "
                          "have at least 1 entries, 0 found");

    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[7]@errorCode",
                          "INVALID_REQUEST");
    ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error[7]",
                          "CuttingToolLifeCycle: Invalid element 'Measurements'");
  }
}

TEST_F(AgentAssetTest, update_asset_count_data_item_v2_0)
{
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 10, "2.0", 4, true);
  addAdapter();

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P1|Part|<Part assetId='P1'>TEST 1</Part>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry@key", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|P2|Part|<Part assetId='P2'>TEST 1</Part>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry@key", "Part");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "2");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|T1|Tool|<Tool assetId='T1'>TEST 1</Tool>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "1");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|T2|Tool|<Tool assetId='T2'>TEST 1</Tool>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "2");
  }

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|T3|Tool|<Tool assetId='T3'>TEST 1</Tool>");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "2");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "3");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P1");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "3");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ASSET@|P2");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_COUNT(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", 0);
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Tool']", "3");
  }

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|@REMOVE_ALL_ASSETS@|");

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_COUNT(doc, "//m:AssetCountDataSet/*", 0);
  }
}

TEST_F(AgentAssetTest, asset_count_should_not_occur_in_header_post_20)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;
  const auto &storage = agent->getAssetStorage();

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE_PUT("/asset/P2", body, queries);
    ASSERT_EQ((unsigned int)2, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/probe");
    ASSERT_XML_PATH_COUNT(doc, "//m:Header/*", 0);
  }
}

TEST_F(AgentAssetTest, asset_count_should_track_asset_additions_by_type)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  string body1 = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;
  const auto &storage = agent->getAssetStorage();

  queries["device"] = "LinuxCNC";
  queries["type"] = "Part";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body1, queries);
    ASSERT_EQ(1u, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
  }

  string body2 = "<PartThing assetId='P2'>TEST 2</PartThing>";
  queries["type"] = "PartThing";

  {
    PARSE_XML_RESPONSE_PUT("/asset", body2, queries);
    ASSERT_EQ(2u, storage->getCount());
  }

  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='PartThing']", "1");
  }

  {
    PARSE_XML_RESPONSE_PUT("/asset/P3", body2, queries);
    ASSERT_EQ(3u, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='PartThing']", "2");
  }

  body2 = "<PartThing assetId='P3' removed='true'>TEST 2</PartThing>";

  {
    PARSE_XML_RESPONSE_PUT("/asset/P3", body2, queries);
    ASSERT_EQ(2u, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE("/LinuxCNC/current");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='Part']", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCountDataSet/m:Entry[@key='PartThing']", "1");
  }
}

TEST_F(AgentAssetTest, asset_should_also_work_using_post_with_assets)
{
  auto agent = m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 4, true);

  string body = "<Part assetId='P1'>TEST 1</Part>";
  QueryMap queries;
  const auto &storage = agent->getAssetStorage();

  {
    PARSE_XML_RESPONSE_PUT("/assets", body, queries);
    ASSERT_EQ((unsigned int)1, storage->getCount());
  }
  {
    PARSE_XML_RESPONSE_PUT("/assets/P2", body, queries);
    ASSERT_EQ((unsigned int)2, storage->getCount());
  }
}
