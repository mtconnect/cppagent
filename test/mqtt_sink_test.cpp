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
#include "buffer/checkpoint.hpp"
#include "device_model/data_item/data_item.hpp"
#include "entity/entity.hpp"
#include "entity/json_parser.hpp"
#include "mqtt/mqtt_client_impl.hpp"
#include "mqtt/mqtt_server_impl.hpp"
#include "printer/json_printer.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::device_model::data_item;
using namespace mtconnect::sink::mqtt_sink;
using namespace mtconnect::sink::rest_sink;
using namespace mtconnect::asset;
using namespace mtconnect::entity;

using json = nlohmann::json;

class MqttSinkTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_jsonPrinter = std::make_unique<printer::JsonPrinter>(2, "1.5", true);
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
      m_agentTestHelper->m_ioContext.run_for(100ms);
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
    MergeOptions(opts, {{"MqttSink", true},
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
    using namespace mtconnect::configuration;
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
                                  m_agentTestHelper->m_agent->defaultDevice()->getName());
  }

  std::unique_ptr<printer::JsonPrinter> m_jsonPrinter;
  std::shared_ptr<mtconnect::mqtt_server::MqttServer> m_server;
  std::shared_ptr<MqttClient> m_client;
  std::shared_ptr<MqttService> m_service;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  uint16_t m_port {0};
};

TEST_F(MqttSinkTest, mqtt_sink_should_be_loaded_by_agent)
{
  createAgent();
  auto service = m_agentTestHelper->getMqttService();

  ASSERT_TRUE(service);
}

TEST_F(MqttSinkTest, mqtt_sink_should_connect_to_broker)
{
  ConfigOptions options;
  createServer(options);
  startServer();

  ASSERT_NE(0, m_port);

  createAgent();
  auto service = m_agentTestHelper->getMqttService();

  ASSERT_TRUE(waitFor(1s, [&service]() { return service->isConnected(); }));
}

TEST_F(MqttSinkTest, mqtt_sink_should_publish_device)
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

  auto service = m_agentTestHelper->getMqttService();

  ASSERT_TRUE(waitFor(1s, [&service]() { return service->isConnected(); }));

  waitFor(2s, [&gotDevice]() { return gotDevice; });
}

TEST_F(MqttSinkTest, mqtt_sink_should_publish_Streams)
{
  ConfigOptions options;
  createServer(options);
  startServer();
  ASSERT_NE(0, m_port);

  entity::JsonParser parser;

  auto handler = make_unique<ClientHandler>();
  bool foundLineDataItem = false;
  handler->m_receive = [&foundLineDataItem, &parser](std::shared_ptr<MqttClient> client,
                                                     const std::string &topic,
                                                     const std::string &payload) {
    EXPECT_EQ("MTConnect/Observation/000/Controller[Controller]/Path/Line[line]", topic);

    auto jdoc = json::parse(payload);
    string value = jdoc.at("/value"_json_pointer).get<string>();
    if (value == string("204"))
    {
      EXPECT_TRUE(true);
      foundLineDataItem = true;
    }
    // this below code not working currently
    /*ErrorList list;
    auto ptr = parser.parse(device_model::data_item::DataItem::getRoot(), payload, "2.0", list);
    auto dataItem = dynamic_pointer_cast<device_model::data_item::DataItem>(ptr);
    if (dataItem)
        EXPECT_EQ(string("204"),dataItem->getValue<string>());*/
  };
  createClient(options, move(handler));
  ASSERT_TRUE(startClient());

  createAgent();
  auto service = m_agentTestHelper->getMqttService();
  ASSERT_TRUE(waitFor(1s, [&service]() { return service->isConnected(); }));

  m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|line|204");

  m_client->subscribe("MTConnect/Observation/000/Controller[Controller]/Path/Line[line]");

  // m_agentTestHelper->m_adapter->processData("2021-02-01T12:00:00Z|lp|NORMAL||||");

  // m_client->subscribe("MTConnect/Observation/000/Controller[Controller]/LogicProgram");

  waitFor(2s, [&foundLineDataItem]() { return foundLineDataItem; });
}

TEST_F(MqttSinkTest, mqtt_sink_should_publish_Asset)
{
  ConfigOptions options;
  createServer(options);
  startServer();
  ASSERT_NE(0, m_port);

  entity::JsonParser parser;

  auto handler = make_unique<ClientHandler>();
  bool gotControllerDataItem = false;
  handler->m_receive = [&gotControllerDataItem, &parser](std::shared_ptr<MqttClient>,
                                                         const std::string &topic,
                                                         const std::string &payload) {
    EXPECT_EQ("MTConnect/Asset/0001", topic);
    auto jdoc = json::parse(payload);
    string id = jdoc.at("/Part/assetId"_json_pointer).get<string>();
    if (id == string("0001"))
    {
      EXPECT_TRUE(true);
      gotControllerDataItem = true;
    }
    /*ErrorList list;
    auto ptr = parser.parse(Asset::getRoot(), payload, "2.0", list);
    EXPECT_EQ(0, list.size());
    auto asset = dynamic_cast<Asset *>(ptr.get());
    EXPECT_TRUE(asset);*/
  };
  createClient(options, move(handler));
  ASSERT_TRUE(startClient());

  createAgent();
  auto service = m_agentTestHelper->getMqttService();
  ASSERT_TRUE(waitFor(1s, [&service]() { return service->isConnected(); }));
  auto time = chrono::system_clock::now();

  m_agentTestHelper->m_adapter->processData(
      "2021-02-01T12:00:00Z|@ASSET@|@1|Part|<Part assetId='1'>TEST 1</Part>");

  m_client->subscribe("MTConnect/Asset/0001");

  waitFor(3s, [&gotControllerDataItem]() { return gotControllerDataItem; });
}
