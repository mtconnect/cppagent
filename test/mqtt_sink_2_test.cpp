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

#include <string>

#include <nlohmann/json.hpp>

#include "agent_test_helper.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/json_parser.hpp"
#include "mtconnect/mqtt/mqtt_client_impl.hpp"
#include "mtconnect/mqtt/mqtt_server_impl.hpp"
#include "mtconnect/printer//json_printer.hpp"
#include "mtconnect/sink/mqtt_sink/mqtt2_service.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::device_model::data_item;
using namespace mtconnect::sink::mqtt_sink;
using namespace mtconnect::asset;
using namespace mtconnect::configuration;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

using json = nlohmann::json;

class MqttSinkTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_jsonPrinter = std::make_unique<printer::JsonPrinter>(2, true);
  }

  void TearDown() override
  {
    const auto agent = m_agentTestHelper->getAgent();
    if (agent)
    {
      m_agentTestHelper->getAgent()->stop();
      m_agentTestHelper->m_ioContext.run_for(100ms);
    }
    if (m_client)
    {
      m_client->stop();
      m_agentTestHelper->m_ioContext.run_for(500ms);
      m_client.reset();
    }
    if (m_server)
    {
      m_server->stop();
      m_agentTestHelper->m_ioContext.run_for(500ms);
      m_server.reset();
    }
    m_agentTestHelper.reset();
    m_jsonPrinter.reset();
  }

  void createAgent(std::string testFile = {}, ConfigOptions options = {})
  {
    if (testFile == "")
      testFile = "/samples/test_config.xml";

    ConfigOptions opts(options);
    MergeOptions(opts, {{"Mqtt2Sink", true},
                        {configuration::MqttPort, m_port},
                        {configuration::MqttHost, "127.0.0.1"s}});
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25, false, true, opts);
    addAdapter();

    m_agentTestHelper->getAgent()->start();
  }

  void createServer(const ConfigOptions &options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts(options);
    MergeOptions(opts, {{ServerIp, "127.0.0.1"s},
                        {MqttPort, 0},
                        {MqttTls, false},
                        {AutoAvailable, false},
                        {RealTime, false}});

    m_server =
        make_shared<mtconnect::mqtt_server::MqttTcpServer>(m_agentTestHelper->m_ioContext, opts);
  }

  template <typename Rep, typename Period>
  bool waitFor(const chrono::duration<Rep, Period> &time, function<bool()> pred)
  {
    boost::asio::steady_timer timer(m_agentTestHelper->m_ioContext);
    timer.expires_from_now(time);
    bool timeout = false;
    timer.async_wait([&timeout](boost::system::error_code ec) {
      if (!ec)
      {
        timeout = true;
      }
    });

    while (!timeout && !pred())
    {
      m_agentTestHelper->m_ioContext.run_for(100ms);
    }
    timer.cancel();

    return pred();
  }

  void startServer()
  {
    if (m_server)
    {
      bool start = m_server->start();
      if (start)
      {
        m_port = m_server->getPort();
        m_agentTestHelper->m_ioContext.run_for(500ms);
      }
    }
  }

  void createClient(const ConfigOptions &options, unique_ptr<ClientHandler> &&handler)
  {
    ConfigOptions opts(options);
    MergeOptions(opts, {{MqttHost, "127.0.0.1"s},
                        {MqttPort, m_port},
                        {MqttTls, false},
                        {AutoAvailable, false},
                        {RealTime, false}});
    m_client = make_shared<mtconnect::mqtt_client::MqttTcpClient>(m_agentTestHelper->m_ioContext,
                                                                  opts, move(handler));
  }

  bool startClient()
  {
    bool started = m_client && m_client->start();
    if (started)
    {
      return waitFor(1s, [this]() { return m_client->isConnected(); });
    }
    return started;
  }

  void addAdapter(ConfigOptions options = ConfigOptions {})
  {
    m_agentTestHelper->addAdapter(options, "localhost", 7878,
                                  m_agentTestHelper->m_agent->getDefaultDevice()->getName());
  }

  std::unique_ptr<printer::JsonPrinter> m_jsonPrinter;
  std::shared_ptr<mtconnect::mqtt_server::MqttServer> m_server;
  std::shared_ptr<MqttClient> m_client;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  uint16_t m_port {0};
};

TEST_F(MqttSinkTest, mqtt_sink_should_publish_Probe)
{
  ConfigOptions options;
  createServer(options);
  startServer();
  ASSERT_NE(0, m_port);

  entity::JsonParser parser;

  auto handler = make_unique<ClientHandler>();
  bool gotDevice = false;
  handler->m_receive = [&gotDevice, &parser](std::shared_ptr<MqttClient> client,
                                             const std::string &topic, const std::string &payload) {
    EXPECT_EQ("MTConnect/Device/000", topic);

    ErrorList list;
    auto ptr = parser.parse(device_model::Device::getRoot(), payload, "2.0", list);
    EXPECT_EQ(0, list.size());
    auto dev = dynamic_pointer_cast<device_model::Device>(ptr);
    EXPECT_TRUE(dev);
    EXPECT_EQ("LinuxCNC", dev->getComponentName());
    EXPECT_EQ("000", *dev->getUuid());

    gotDevice = true;
  };

  createClient(options, move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Device/000");

  createAgent();

  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));

  waitFor(1s, [&gotDevice]() { return gotDevice; });
}

TEST_F(MqttSinkTest, mqtt_sink_should_publish_Sample)
{
  ConfigOptions options;
  createServer(options);
  startServer();
  ASSERT_NE(0, m_port);

  entity::JsonParser parser;

  auto handler = make_unique<ClientHandler>();
  bool gotSample = false;
  handler->m_receive = [&gotSample, &parser](std::shared_ptr<MqttClient> client,
                                             const std::string &topic, const std::string &payload) {
    EXPECT_EQ("MTConnect/000/Sample", topic);

    ErrorList list;
    auto ptr = parser.parse(device_model::Device::getRoot(), payload, "2.0", list);
    EXPECT_EQ(0, list.size());
    auto dev = dynamic_pointer_cast<device_model::Device>(ptr);
    EXPECT_TRUE(dev);
    EXPECT_EQ("LinuxCNC", dev->getComponentName());
    EXPECT_EQ("000", *dev->getUuid());

    gotSample = true;
  };

  createClient(options, move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/000/Sample");

  createAgent();

  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));

  waitFor(1s, [&gotSample]() { return gotSample; });
}

TEST_F(MqttSinkTest, mqtt_sink_should_publish_Current)
{
  ConfigOptions options;
  createServer(options);
  startServer();
  ASSERT_NE(0, m_port);

  entity::JsonParser parser;

  auto handler = make_unique<ClientHandler>();
  bool gotCurrent = false;
  handler->m_receive = [&gotCurrent, &parser](std::shared_ptr<MqttClient> client,
                                             const std::string &topic, const std::string &payload) {
    EXPECT_EQ("MTConnect/e481314c-07c4-525f-966f-71dd53b8d717/Current", topic);

    ErrorList list;
    auto ptr = parser.parse(device_model::Device::getRoot(), payload, "2.0", list);
    EXPECT_EQ(0, list.size());
    auto dev = dynamic_pointer_cast<device_model::Device>(ptr);
    EXPECT_TRUE(dev);
    EXPECT_EQ("LinuxCNC", dev->getComponentName());
    EXPECT_EQ("e481314c-07c4-525f-966f-71dd53b8d717", *dev->getUuid());

    gotCurrent = true;
  };

  createClient(options, move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/e481314c-07c4-525f-966f-71dd53b8d717/Current");

  createAgent();

  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));

  waitFor(1s, [&gotCurrent]() { return gotCurrent; });
}
