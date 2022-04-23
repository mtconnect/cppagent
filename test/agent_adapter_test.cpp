//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "device_model/reference.hpp"
#include "pipeline/mtconnect_xml_transform.hpp"
#include "pipeline/response_document.hpp"
#include "source/adapter/adapter.hpp"
#include "source/adapter/agent_adapter/agent_adapter.hpp"
#include "source/adapter/agent_adapter/url_parser.hpp"
#include "test_utilities.hpp"
#include "xml_printer.hpp"

// Registers the fixture into the 'registry'
using namespace std;
using namespace std::chrono;
using namespace mtconnect;
using namespace mtconnect::rest_sink;
using namespace mtconnect::source::adapter;
using namespace mtconnect::observation;
using namespace mtconnect::source::adapter::agent_adapter;
using namespace mtconnect::pipeline;
using namespace mtconnect::asset;
using namespace mtconnect::observation;

using status = boost::beast::http::status;
namespace asio = boost::asio;

struct MockPipelineContract : public PipelineContract
{
  MockPipelineContract(DevicePtr &device) : m_device(device) {}
  DevicePtr findDevice(const std::string &device) override { return m_device; }
  DataItemPtr findDataItem(const std::string &device, const std::string &name) override
  {
    return m_device->getDeviceDataItem(name);
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override
  {
    m_observations.push_back(obs);
  }
  void deliverAsset(AssetPtr) override {}
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &dev, bool flag) override {}
  void sourceFailed(const std::string &id) override { m_failed = true; }

  bool m_failed = false;
  std::string m_result;

  DevicePtr m_device;
  std::vector<ObservationPtr> m_observations;
};

class AgentAdapterTest : public testing::Test
{
public:
  typedef std::map<std::string, std::string> map_type;
  using queue_type = list<string>;

protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25, true);
    m_agentTestHelper->getAgent()->start();
    m_agentId = to_string(getCurrentTimeInSec());

    auto printer = make_unique<XmlPrinter>();
    auto parser = make_unique<XmlParser>();

    m_device =
        parser->parseFile(PROJECT_ROOT_DIR "/samples/test_config.xml", printer.get()).front();

    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_device);
  }

  void TearDown() override
  {
    if (m_adapter)
    {
      m_adapter->stop();
      m_adapter.reset();
    }

    m_agentTestHelper->getAgent()->stop();
    m_agentTestHelper.reset();
  }

  auto createAdapter(int port, mtconnect::ConfigOptions options = {}, const std::string path = "",
                     int hb = 500)
  {
    using namespace mtconnect;
    using namespace mtconnect::source::adapter;

    string url = "http://127.0.0.1:"s + boost::lexical_cast<string>(port) + "/"s + path;
    options.emplace(configuration::Url, url);
    options.emplace(configuration::Device, "LinuxCNC"s);
    options.emplace(configuration::SourceDevice, "LinuxCNC"s);
    options.emplace(configuration::Port, port);
    options.emplace(configuration::Count, 100);
    options.emplace(configuration::Heartbeat, hb);
    options.emplace(configuration::ReconnectInterval, 1);

    boost::property_tree::ptree tree;
    m_adapter = std::make_shared<agent_adapter::AgentAdapter>(m_agentTestHelper->m_ioContext,
                                                              m_context, options, tree);

    return m_adapter;
  }

  void addAdapter(ConfigOptions options = ConfigOptions {})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 7878,
                                  m_agentTestHelper->m_agent->defaultDevice()->getName());
  }

public:
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::shared_ptr<AgentAdapter> m_adapter;
  std::chrono::milliseconds m_delay {};
  shared_ptr<PipelineContext> m_context;
  DevicePtr m_device;
};

TEST_F(AgentAdapterTest, should_connect_to_agent)
{
  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port);

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  bool connecting = false;
  bool connected = false;
  ResponseDocument data;
  handler->m_processData = [&](const string &d, const string &s) {};
  handler->m_connecting = [&](const string id) { connecting = true; };
  handler->m_connected = [&](const string id) { connected = true; };

  adapter->setHandler(handler);
  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 500ms);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      throw runtime_error("test timed out");
    }
  });

  while (!connecting)
  {
    m_agentTestHelper->m_ioContext.run_one_for(100ms);
  }
  ASSERT_TRUE(connecting);

  while (!connected)
  {
    m_agentTestHelper->m_ioContext.run_one_for(100ms);
  }

  ASSERT_TRUE(connected);

  timeout.cancel();
}

TEST_F(AgentAdapterTest, should_get_current_from_agent)
{
  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port);

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  bool current = false;
  handler->m_processData = [&](const string &d, const string &s) {
    if (d.find("MTConnectStreams") != string::npos)
      current = true;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  adapter->setHandler(handler);
  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 500ms);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      throw runtime_error("test timed out");
    }
  });

  while (!current)
  {
    m_agentTestHelper->m_ioContext.run_one_for(100ms);
  }
  ASSERT_TRUE(current);

  timeout.cancel();
}

TEST_F(AgentAdapterTest, should_get_assets_from_agent)
{
  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port);

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  bool assets = false;
  handler->m_processData = [&](const string &d, const string &s) {
    if (d.find("MTConnectAssets") != string::npos)
      assets = true;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  adapter->setHandler(handler);
  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 500ms);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      throw runtime_error("test timed out");
    }
  });

  while (!assets)
  {
    m_agentTestHelper->m_ioContext.run_one_for(100ms);
  }
  ASSERT_TRUE(assets);

  timeout.cancel();
}

TEST_F(AgentAdapterTest, should_receive_sample)
{
  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port);

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    auto seq = m_context->getSharedState<XmlTransformFeedback>("XmlTransformFeedback");
    seq->m_next = rd.m_next;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  adapter->setHandler(handler);
  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 500ms);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      throw runtime_error("test timed out");
    }
  });

  while (rc < 2)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_EQ(2, rc);

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|execution|READY");

  ASSERT_EQ(32, rd.m_entities.size());
  rd.m_entities.clear();
  while (rc < 3)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_EQ(3, rc);
  ASSERT_EQ(1, rd.m_entities.size());

  auto obs = rd.m_entities.front();
  ASSERT_EQ("p5", get<string>(obs->getProperty("dataItemId")));
  ASSERT_EQ("READY", obs->getValue<string>());

  timeout.cancel();
}

TEST_F(AgentAdapterTest, should_reconnect)
{
  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {}, "", 5000);

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    auto seq = m_context->getSharedState<XmlTransformFeedback>("XmlTransformFeedback");
    seq->m_next = rd.m_next;
    seq->m_instanceId = rd.m_instanceId;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  bool disconnected = false;
  handler->m_disconnected = [&](const string id) {
    disconnected = true;
  };

  adapter->setHandler(handler);
  adapter->start();

  sink::rest_sink::SessionPtr session;
  m_agentTestHelper->m_restService->getServer()->m_lastSession =
      [&](sink::rest_sink::SessionPtr ptr) {
        session = ptr;
      };

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 2s);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      //throw runtime_error("test timed out");
    }
  });

  while (rc < 2)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_EQ(2, rc);
  ASSERT_TRUE(session);

  ASSERT_EQ(32, rd.m_entities.size());
  rd.m_entities.clear();

  session->close();
  while (!disconnected)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }

  session.reset();

  while (!session)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_TRUE(session);

  timeout.cancel();
}

TEST_F(AgentAdapterTest, should_connect_with_http_10_agent)
{
  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {{"!CloseConnectionAfterResponse!", true}});

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    auto seq = m_context->getSharedState<XmlTransformFeedback>("XmlTransformFeedback");
    seq->m_next = rd.m_next;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  adapter->setHandler(handler);
  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 500ms);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      throw runtime_error("test timed out");
    }
  });

  while (rc < 2)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_EQ(2, rc);

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|execution|READY");

  ASSERT_EQ(32, rd.m_entities.size());
  rd.m_entities.clear();
  while (rc < 3)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_EQ(3, rc);
  ASSERT_EQ(1, rd.m_entities.size());

  auto obs = rd.m_entities.front();
  ASSERT_EQ("p5", get<string>(obs->getProperty("dataItemId")));
  ASSERT_EQ("READY", obs->getValue<string>());

  timeout.cancel();
}

TEST_F(AgentAdapterTest, should_map_device_name_and_uuid) { GTEST_SKIP(); }

TEST_F(AgentAdapterTest, should_fallback_to_polling_samples_if_chunked_times_out) { GTEST_SKIP(); }

TEST_F(AgentAdapterTest, should_connect_to_tls_agent) { GTEST_SKIP(); }

TEST_F(AgentAdapterTest, should_first_try_to_recover_from_previous_position) { GTEST_SKIP(); }

TEST_F(AgentAdapterTest, should_check_instance_id_on_recovery)
{
  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {}, "", 5000);

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  bool disconnected = false;
  bool recovering = false;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    auto seq = m_context->getSharedState<XmlTransformFeedback>("XmlTransformFeedback");
    seq->m_next = rd.m_next;
    if (recovering)
    {
      recovering = false;
      throw std::system_error(make_error_code(mtconnect::source::ErrorCode::RESTART_STREAM));
    }
    seq->m_instanceId = rd.m_instanceId;
    disconnected = false;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {
  };

  handler->m_disconnected = [&](const string id) {
    disconnected = true;
  };

  adapter->setHandler(handler);
  adapter->start();

  sink::rest_sink::SessionPtr session;
  m_agentTestHelper->m_restService->getServer()->m_lastSession =
      [&](sink::rest_sink::SessionPtr ptr) {
        session = ptr;
      };

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 2s);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      //throw runtime_error("test timed out");
    }
  });

  while (rc < 2)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_EQ(2, rc);
  ASSERT_TRUE(session);

  ASSERT_EQ(32, rd.m_entities.size());
  rd.m_entities.clear();

  session->close();
  while (!disconnected)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }

  recovering = true;
  session.reset();
  while (!session)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_TRUE(session);
  
  while (disconnected)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }

  timeout.cancel();
}
