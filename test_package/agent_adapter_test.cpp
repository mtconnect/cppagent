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

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "agent_test_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/device_model/reference.hpp"
#include "mtconnect/pipeline/mtconnect_xml_transform.hpp"
#include "mtconnect/pipeline/response_document.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/agent_adapter/agent_adapter.hpp"
#include "mtconnect/source/adapter/agent_adapter/url_parser.hpp"
#include "test_utilities.hpp"

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

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

struct MockPipelineContract : public PipelineContract
{
  MockPipelineContract(DevicePtr &device) : m_device(device) {}
  DevicePtr findDevice(const std::string &device) override
  {
    m_deviceName = device;
    return m_device;
  }
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
  void deliverDevice(DevicePtr d) override { m_receivedDevice = d; }
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &dev, bool flag) override {}
  void sourceFailed(const std::string &id) override { m_failed = true; }
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }

  bool m_failed = false;
  std::string m_result;
  std::string m_deviceName;
  DevicePtr m_device;
  DevicePtr m_receivedDevice;
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

    auto printer = make_unique<printer::XmlPrinter>();
    auto parser = make_unique<parser::XmlParser>();

    m_device =
        parser->parseFile(TEST_RESOURCE_DIR "/samples/test_config.xml", printer.get()).front();

    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_device);
  }

  void TearDown() override
  {
    if (m_adapter)
    {
      m_adapter->stop();
      m_agentTestHelper->m_ioContext.run_for(100ms);
    }

    m_agentTestHelper->getAgent()->stop();
    m_agentTestHelper->m_ioContext.removeGuard();
    m_agentTestHelper->m_ioContext.run_for(10s);
    m_agentTestHelper.reset();

    m_adapter.reset();
  }

  void createAgent(ConfigOptions options = {}, std::string deviceFile = "/samples/test_config.xml")
  {
    m_agentTestHelper->createAgent(deviceFile, 8, 4, "2.0", 25, false, true, options);
    m_agentTestHelper->getAgent()->start();
    m_agentId = to_string(getCurrentTimeInSec());
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
    options.emplace(configuration::Heartbeat, Milliseconds(hb));
    options.emplace(configuration::ReconnectInterval, 500ms);

    boost::property_tree::ptree tree;
    m_adapter = std::make_shared<agent_adapter::AgentAdapter>(m_agentTestHelper->m_ioContext,
                                                              m_context, options, tree);

    return m_adapter;
  }

  void addAdapter(ConfigOptions options = ConfigOptions {})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 7878,
                                  m_agentTestHelper->m_agent->getDefaultDevice()->getName());
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
  createAgent();

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
  createAgent();

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

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 1s);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      // throw runtime_error("test timed out");
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
  createAgent();

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

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 1s);
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
  createAgent();

  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port);

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    adapter->getFeedback().m_next = rd.m_next;
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
  createAgent();

  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {}, "", 1000);

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  ResponseDocument rd;
  bool response = false;
  handler->m_processData = [&](const string &d, const string &s) {
    response = true;

    ResponseDocument::parse(d, rd, m_context);
    rc++;

    adapter->getFeedback().m_next = rd.m_next;
    adapter->getFeedback().m_instanceId = rd.m_instanceId;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  bool disconnected = false;
  handler->m_disconnected = [&](const string id) { disconnected = true; };

  adapter->setHandler(handler);
  adapter->start();

  sink::rest_sink::SessionPtr session;
  m_agentTestHelper->m_restService->getServer()->m_lastSession =
      [&](sink::rest_sink::SessionPtr ptr) { session = ptr; };

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 5s);
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
  ASSERT_TRUE(session);

  ASSERT_EQ(32, rd.m_entities.size());
  rd.m_entities.clear();

  session->close();
  response = false;
  while (!response)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }

  ASSERT_TRUE(session);
  ASSERT_FALSE(disconnected);

  m_agentTestHelper->m_restService->getServer()->m_lastSession = nullptr;
  timeout.cancel();
}

TEST_F(AgentAdapterTest, should_connect_with_http_10_agent)
{
  createAgent();

  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {{"!CloseConnectionAfterResponse!", true}});

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    adapter->getFeedback().m_next = rd.m_next;
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

TEST_F(AgentAdapterTest, should_check_instance_id_on_recovery)
{
  createAgent();

  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {}, "", 500);

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  bool disconnected = false;
  bool recovering = false;
  bool response = false;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    rd.m_next = 0;
    rd.m_instanceId = 0;
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    if (rd.m_next != 0)
      response = true;

    auto seq = &adapter->getFeedback();
    if (rd.m_next != 0)
      seq->m_next = rd.m_next;
    if (recovering)
    {
      recovering = false;
      throw std::system_error(make_error_code(mtconnect::source::ErrorCode::INSTANCE_ID_CHANGED));
    }
    seq->m_instanceId = rd.m_instanceId;
    disconnected = false;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  handler->m_disconnected = [&](const string id) { disconnected = true; };

  adapter->setHandler(handler);
  adapter->start();

  sink::rest_sink::SessionPtr session;
  m_agentTestHelper->m_restService->getServer()->m_lastSession =
      [&](sink::rest_sink::SessionPtr ptr) { session = ptr; };

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 4s);
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
  ASSERT_TRUE(session);

  ASSERT_EQ(32, rd.m_entities.size());
  rd.m_entities.clear();

  session->close();
  response = false;
  while (!response)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }

  recovering = true;
  session.reset();
  while (recovering)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_FALSE(recovering);
  ASSERT_TRUE(disconnected);

  response = false;
  while (!response)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_TRUE(response);

  m_agentTestHelper->m_restService->getServer()->m_lastSession = nullptr;
  timeout.cancel();
}

TEST_F(AgentAdapterTest, should_map_device_name_and_uuid)
{
  createAgent();

  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {{configuration::Device, "NewMachine"s}}, "", 5000);

  addAdapter();

  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 500ms);
  timeout.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      throw runtime_error("test timed out");
    }
  });

  auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());

  while (contract->m_observations.size() == 0)
  {
    contract->m_deviceName.clear();
    m_agentTestHelper->m_ioContext.run_one();
  }

  ASSERT_EQ("NewMachine", contract->m_deviceName);
}

TEST_F(AgentAdapterTest, should_use_polling_when_option_is_set)
{
  createAgent();

  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {{"UsePolling", true}});

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    adapter->getFeedback().m_next = rd.m_next;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  adapter->setHandler(handler);
  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 2s);
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

  auto seq = &adapter->getFeedback();
  auto next = seq->m_next;

  ASSERT_EQ(32, rd.m_entities.size());
  rd.m_entities.clear();
  while (rc < 4)
  {
    m_agentTestHelper->m_ioContext.run_one();
  }
  ASSERT_EQ(4, rc);

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|execution|READY");

  while (rd.m_entities.empty())
  {
    m_agentTestHelper->m_ioContext.run_one();
  }

  ASSERT_EQ(1, rd.m_entities.size());

  seq = &adapter->getFeedback();
  ASSERT_GT(seq->m_next, next);

  auto obs = rd.m_entities.front();
  ASSERT_EQ("p5", get<string>(obs->getProperty("dataItemId")));
  ASSERT_EQ("READY", obs->getValue<string>());

  timeout.cancel();
}

// Not sure if this is the correct behavior
// TEST_F(AgentAdapterTest, should_fallback_to_polling_samples_if_chunked_times_out) { GTEST_SKIP();
// }

const string CertFile(TEST_RESOURCE_DIR "/user.crt");
const string KeyFile {TEST_RESOURCE_DIR "/user.key"};
const string DhFile {TEST_RESOURCE_DIR "/dh2048.pem"};
const string RootCertFile(TEST_RESOURCE_DIR "/rootca.crt");

TEST_F(AgentAdapterTest, should_connect_to_tls_agent)
{
  using namespace mtconnect::configuration;

  createAgent({{{TlsCertificateChain, CertFile},
                {TlsPrivateKey, KeyFile},
                {TlsDHKey, DhFile},
                {TlsCertificatePassword, "mtconnect"s}}});

  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  string url = "https://127.0.0.1:"s + boost::lexical_cast<string>(port) + "/";
  auto adapter = createAdapter(port, {{configuration::Url, url}});

  addAdapter();

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

TEST_F(AgentAdapterTest, should_create_device_when_option_supplied)
{
  createAgent({}, "/samples/solid_model.xml");

  auto port = m_agentTestHelper->m_restService->getServer()->getPort();
  auto adapter = createAdapter(port, {{configuration::EnableSourceDeviceModels, true}});

  addAdapter();

  unique_ptr<source::adapter::Handler> handler = make_unique<Handler>();

  int rc = 0;
  ResponseDocument rd;
  handler->m_processData = [&](const string &d, const string &s) {
    ResponseDocument::parse(d, rd, m_context);
    rc++;

    adapter->getFeedback().m_next = rd.m_next;
  };
  handler->m_connecting = [&](const string id) {};
  handler->m_connected = [&](const string id) {};

  adapter->setHandler(handler);
  adapter->start();

  boost::asio::steady_timer timeout(m_agentTestHelper->m_ioContext, 2s);
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
  ASSERT_GE(2, rc);
}
