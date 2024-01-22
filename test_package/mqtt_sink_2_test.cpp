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
#include "json_helper.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/json_parser.hpp"
#include "mtconnect/mqtt/mqtt_client_impl.hpp"
#include "mtconnect/mqtt/mqtt_server_impl.hpp"
#include "mtconnect/printer//json_printer.hpp"
#include "mtconnect/sink/mqtt_sink/mqtt2_service.hpp"
#include "test_utilities.hpp"

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

class MqttSink2Test : public testing::Test
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
                        {MqttCurrentInterval, 200ms},
                        {MqttSampleInterval, 100ms},
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
    timer.expires_after(time);
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
                                                                  opts, std::move(handler));
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
    m_agentTestHelper->addAdapter(options, "localhost", 0,
                                  m_agentTestHelper->m_agent->getDefaultDevice()->getName());
  }

  std::unique_ptr<printer::JsonPrinter> m_jsonPrinter;
  std::shared_ptr<mtconnect::mqtt_server::MqttServer> m_server;
  std::shared_ptr<MqttClient> m_client;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  uint16_t m_port {0};
};

TEST_F(MqttSink2Test, mqtt_sink_flat_formatt_check)
{
  ConfigOptions options {{MqttMaxTopicDepth, 9}, {ProbeTopic, "Device/F/l/a/t/F/o/r/m/a/t"s}};
  createServer(options);
  startServer();

  ASSERT_NE(0, m_port);

  createAgent("", options);
  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(10s, [&service]() { return service->isConnected(); }));
}

TEST_F(MqttSink2Test, mqtt_sink_should_publish_Probe)
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
    EXPECT_EQ("MTConnect/Probe/000", topic);

    ErrorList list;
    auto ptr = parser.parse(device_model::Device::getRoot(), payload, "2.0", list);
    EXPECT_EQ(0, list.size());
    auto dev = dynamic_pointer_cast<device_model::Device>(ptr);
    EXPECT_TRUE(dev);
    EXPECT_EQ("LinuxCNC", dev->getComponentName());
    EXPECT_EQ("000", *dev->getUuid());

    gotDevice = true;
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Probe/000");

  createAgent();

  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));

  ASSERT_TRUE(waitFor(1s, [&gotDevice]() { return gotDevice; }));
}

TEST_F(MqttSink2Test, mqtt_sink_should_publish_Sample)
{
  ConfigOptions options;
  createServer(options);
  startServer();
  ASSERT_NE(0, m_port);

  entity::JsonParser parser;

  auto handler = make_unique<ClientHandler>();
  bool gotSample = false;
  handler->m_receive = [&gotSample](std::shared_ptr<MqttClient> client, const std::string &topic,
                                    const std::string &payload) {
    EXPECT_EQ("MTConnect/Sample/000", topic);

    auto jdoc = json::parse(payload);
    auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream"_json_pointer);
    EXPECT_EQ(string("LinuxCNC"), streams.at("/name"_json_pointer).get<string>());

    gotSample = true;
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Sample/000");

  createAgent();

  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));
  ASSERT_FALSE(gotSample);

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");
  ASSERT_TRUE(waitFor(10s, [&gotSample]() { return gotSample; }));
}

TEST_F(MqttSink2Test, mqtt_sink_should_publish_Current)
{
  ConfigOptions options;
  createServer(options);
  startServer();
  ASSERT_NE(0, m_port);

  entity::JsonParser parser;

  auto handler = make_unique<ClientHandler>();
  bool gotCurrent = false;
  handler->m_receive = [&gotCurrent](std::shared_ptr<MqttClient> client, const std::string &topic,
                                     const std::string &payload) {
    EXPECT_EQ("MTConnect/Current/000", topic);

    auto jdoc = json::parse(payload);
    auto streams = jdoc.at("/MTConnectStreams/Streams/0/DeviceStream"_json_pointer);
    EXPECT_EQ(string("LinuxCNC"), streams.at("/name"_json_pointer).get<string>());

    gotCurrent = true;
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Current/000");

  createAgent();

  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));
  ASSERT_TRUE(gotCurrent);

  gotCurrent = false;
  ASSERT_TRUE(waitFor(1s, [&gotCurrent]() { return gotCurrent; }));
}

TEST_F(MqttSink2Test, mqtt_sink_should_publish_Probe_with_uuid_first)
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
    EXPECT_EQ("MTConnect/000/Probe", topic);

    ErrorList list;
    auto ptr = parser.parse(device_model::Device::getRoot(), payload, "2.0", list);
    EXPECT_EQ(0, list.size());
    auto dev = dynamic_pointer_cast<device_model::Device>(ptr);
    EXPECT_TRUE(dev);
    EXPECT_EQ("LinuxCNC", dev->getComponentName());
    EXPECT_EQ("000", *dev->getUuid());

    gotDevice = true;
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/000/Probe");

  createAgent("", {{configuration::ProbeTopic, "MTConnect/[device]/Probe"s}});

  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));

  ASSERT_TRUE(waitFor(1s, [&gotDevice]() { return gotDevice; }));
}

TEST_F(MqttSink2Test, mqtt_sink_should_publish_Probe_no_device_in_format)
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
    EXPECT_EQ("MTConnect/Probe/000", topic);

    ErrorList list;
    auto ptr = parser.parse(device_model::Device::getRoot(), payload, "2.0", list);
    EXPECT_EQ(0, list.size());
    auto dev = dynamic_pointer_cast<device_model::Device>(ptr);
    EXPECT_TRUE(dev);
    EXPECT_EQ("LinuxCNC", dev->getComponentName());
    EXPECT_EQ("000", *dev->getUuid());

    gotDevice = true;
  };

  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  m_client->subscribe("MTConnect/Probe/000");

  createAgent("", {{configuration::ProbeTopic, "MTConnect/Probe"s}});

  auto service = m_agentTestHelper->getMqtt2Service();

  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));

  ASSERT_TRUE(waitFor(1s, [&gotDevice]() { return gotDevice; }));
}

TEST_F(MqttSink2Test, mqtt_sink_should_publish_agent_device)
{
  ConfigOptions options;
  createServer(options);
  startServer();
  ASSERT_NE(0, m_port);
  
  entity::JsonParser parser;
  
  DevicePtr ad;
  string agent_topic;
  
  auto handler = make_unique<ClientHandler>();
  bool gotDevice = false;
  handler->m_receive = [&gotDevice, &parser, &agent_topic, &ad ](std::shared_ptr<MqttClient> client,
                                             const std::string &topic, const std::string &payload) {
    EXPECT_EQ(agent_topic, topic);
    gotDevice = true;
  };
  
  createClient(options, std::move(handler));
  ASSERT_TRUE(startClient());
  createAgent();

  ad = m_agentTestHelper->m_agent->getAgentDevice();
  agent_topic = "MTConnect/Probe/Agent_"s + *ad->getUuid();
  m_client->subscribe(agent_topic);
    
  auto service = m_agentTestHelper->getMqtt2Service();
  
  ASSERT_TRUE(waitFor(60s, [&service]() { return service->isConnected(); }));
  
  ASSERT_TRUE(waitFor(1s, [&gotDevice]() { return gotDevice; }));
}
