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
#include "mqtt/mqtt_client_impl.hpp"
#include "mqtt/mqtt_server_impl.hpp"
#include "printer/json_printer.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::mqtt_sink;
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
      m_context.run_for(100ms);
      m_client.reset();
    }
    if (m_server)
    {
      m_server->stop();
      m_context.run_for(500ms);
      m_server.reset();
    }
    m_agentTestHelper.reset();
    m_jsonPrinter.reset();
  }

  void createAgent(ConfigOptions options = {})
  {
    options.emplace("MqttSink", true);
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25, false, true,
                                   options);

    m_agentTestHelper->getAgent()->start();
  }

  void createServer(const ConfigOptions& options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts(options);
    opts[Port] = 0;
    opts[ServerIp] = "127.0.0.1"s;
    m_server = make_shared<mtconnect::mqtt_server::MqttTcpServer>(m_context, opts);
  }

  void startServer()
  {
    if (m_server)
    {
      bool start = m_server->start();
      if (start)
      {
        m_port = m_server->getPort();
        m_context.run_for(500ms);
      }
    }
  }

  void createClient(const ConfigOptions& options)
  {
    ConfigOptions opts(options);
    opts[configuration::Port] = m_port;
    m_client = make_shared<mtconnect::mqtt_client::MqttTcpClient>(m_context, opts);
  }

  bool startClient()
  {
    bool started = m_client && m_client->start();
    if (started)
    {
      m_context.run_for(1s);
    }
    return started;
  }

  std::unique_ptr<printer::JsonPrinter> m_jsonPrinter;
  std::shared_ptr<mtconnect::mqtt_server::MqttServer> m_server;
  std::shared_ptr<MqttClient> m_client;
  asio::io_context m_context;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  uint16_t m_port { 0 };
};

TEST_F(MqttSinkTest, mqtt_sink_should_be_loaded_by_agent)
{
  createAgent();

  auto agent = m_agentTestHelper->getAgent();

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService);
}

TEST_F(MqttSinkTest, mqtt_sink_to_send_Probe)
{  
  createAgent();

  ConfigOptions options {{configuration::Host, "localhost"s},
                         {configuration::Port, 0},
                         {configuration::MqttTls, false},
                         {configuration::AutoAvailable, false},
                         {configuration::RealTime, false}};

  createServer(options);

  startServer();

  ASSERT_NE(0, m_port);

  auto agent = m_agentTestHelper->getAgent();

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService);

  if (mqttService->isConnected())
  {   
    std::shared_ptr<MqttClient> client = mqttService->getClient();
    if (client)
    {
      std::list<DevicePtr> devices = m_agentTestHelper->m_agent->getDevices();

      auto doc = m_jsonPrinter->printProbe(123, 9999, 1, 1024, 10, devices);
      auto jdoc = json::parse(doc);
      auto it = jdoc.begin();
      ASSERT_NE(string("MTConnectDevices"), it.key());
      auto jsonDevices = jdoc.at("/MTConnectDevices/Devices"_json_pointer);

      auto device = jsonDevices.at(0).at("/Device"_json_pointer);
      auto device2 = jsonDevices.at(1).at("/Device"_json_pointer);

      ASSERT_NE(string("x872a3490"), device.at("/id"_json_pointer).get<string>());
      ASSERT_NE(string("SimpleCnc"), device.at("/name"_json_pointer).get<string>());
    }
  }
}

const string MqttCACert(PROJECT_ROOT_DIR "/test/resources/clientca.crt");

TEST_F(MqttSinkTest, mqtt_client_should_connect_to_broker)
{
  ConfigOptions options {
      {configuration::Host, "localhost"s}, {configuration::Port, 0},
      {configuration::MqttTls, false},     {configuration::AutoAvailable, false},
      {configuration::RealTime, false},    {configuration::MqttCaCert, MqttCACert}};

  createServer(options);

  startServer();

  ASSERT_NE(0, m_port);

  createClient(options);

  ASSERT_TRUE(startClient());
  
  ASSERT_TRUE(m_client->isConnected());    

  m_client->stop();
}

TEST_F(MqttSinkTest, mqtt_client_print_probe)
{
  GTEST_SKIP();
	
  createAgent();

  ConfigOptions options {
      {configuration::Host, "localhost"s}, {configuration::Port, 0},
      {configuration::MqttTls, false},     {configuration::AutoAvailable, false},
      {configuration::RealTime, false},    {configuration::MqttCaCert, MqttCACert}};

  createServer(options);

  startServer();

  ASSERT_NE(0, m_port);

  createClient(options);
   
  ASSERT_TRUE(startClient());

  ASSERT_TRUE(m_client->isConnected());

  if (m_client && m_client->isConnected())
  {
    StringList topicList;
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");
    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
   
    for (auto& device : devices)
    {
      auto dataItems = device.at("/DataItems"_json_pointer);

      for (int i = 0; i < dataItems.size(); i++)
      {
        auto dataItem = dataItems.at(i);
      //}
      //for (auto const& dataItem : dataItems)
      //{
        string dataItemId = dataItem.at("/DataItem/id"_json_pointer).get<string>();
        bool sucess = m_client->subscribe(dataItemId);
        if (!sucess)
        {
          continue;
        }
      }
    }

    /*auto device = devices.at(0).at("/Device"_json_pointer);
    auto dataItems = device.at("/DataItems"_json_pointer);

    auto dataitem0 = dataItems.at(0);
    string dataItemId0 = dataitem0.at("/DataItem/id"_json_pointer).get<string>();
    ASSERT_EQ(string("a"), dataItemId0);
    topicList.push_back(dataItemId0);
    m_client->subscribe(dataItemId0);

    auto dataitem1 = dataItems.at(1);
    string dataItemId1 = dataitem1.at("/DataItem/id"_json_pointer).get<string>();
    ASSERT_EQ(string("avail"), dataItemId1);
    topicList.push_back(dataItemId1);
    
    m_client->subscribe(dataItemId1);*/
  }   

  m_client->stop();
}


TEST_F(MqttSinkTest, mqtt_client_should_connect_to_local_server)
{
  ConfigOptions options {
      {configuration::Host, "localhost"s}, {configuration::Port, 0},
      {configuration::MqttTls, false},     {configuration::AutoAvailable, false},
      {configuration::RealTime, false},    {configuration::MqttCaCert, MqttCACert}};

  createServer(options);
  startServer();

  ASSERT_NE(0, m_port);
  
  std::uint16_t pid_sub1;

  auto client = mqtt::make_async_client(m_context, "localhost", m_port);

  client->set_client_id("cliendId1");
  client->set_clean_session(true);
  client->set_keep_alive_sec(30);

  client->set_connack_handler(
      [&client, &pid_sub1](bool sp, mqtt::connect_return_code connack_return_code) {
        std::cout << "Connack handler called" << std::endl;
        std::cout << "Session Present: " << std::boolalpha << sp << std::endl;
        std::cout << "Connack Return Code: " << connack_return_code << std::endl;
        if (connack_return_code == mqtt::connect_return_code::accepted)
        {
          pid_sub1 = client->acquire_unique_packet_id();

          client->async_subscribe(pid_sub1, "mqtt_client_cpp/topic1", MQTT_NS::qos::at_most_once,
                                  // [optional] checking async_subscribe completion code
                                  [](MQTT_NS::error_code ec) {
                                    std::cout << "async_subscribe callback: " << ec.message()
                                              << std::endl;
                                  });
        }
        return true;
      });
  client->set_close_handler([] { std::cout << "closed" << std::endl; });

  client->set_suback_handler([&client, &pid_sub1](std::uint16_t packet_id,
                                                  std::vector<mqtt::suback_return_code> results) {
    std::cout << "suback received. packet_id: " << packet_id << std::endl;
    for (auto const& e : results)
    {
      std::cout << "subscribe result: " << e << std::endl;
    }

    if (packet_id == pid_sub1)
    {
      client->async_publish("mqtt_client_cpp/topic1", "test1", MQTT_NS::qos::at_most_once,
                            // [optional] checking async_publish completion code
                            [](MQTT_NS::error_code ec) {
                              std::cout << "async_publish callback: " << ec.message() << std::endl;
                              ASSERT_EQ(ec.message(), "Success");
                            });
    }

    return true;
  });

  client->set_close_handler([] { std::cout << "closed" << std::endl; });

  client->set_suback_handler(
      [&client, &pid_sub1](std::uint16_t packet_id, std::vector<mqtt::suback_return_code> results) {
        std::cout << "suback received. packet_id: " << packet_id << std::endl;
        for (auto const &e : results)
        {
          std::cout << "subscribe result: " << e << std::endl;
        }

        if (packet_id == pid_sub1)
        {
          client->async_publish("mqtt_client_cpp/topic1", "test1", MQTT_NS::qos::at_most_once,
                                // [optional] checking async_publish completion code
                                [packet_id](MQTT_NS::error_code ec) {
                                  std::cout << "async_publish callback: " << ec.message()
                                            << std::endl;
                                  ASSERT_TRUE(packet_id);
                                });
        }

        return true;
      });

  client->set_publish_handler([&client](mqtt::optional<std::uint16_t> packet_id,
                                        mqtt::publish_options pubopts, mqtt::buffer topic_name,
                                        mqtt::buffer contents) {
    std::cout << "publish received."
              << " dup: " << pubopts.get_dup() << " qos: " << pubopts.get_qos()
              << " retain: " << pubopts.get_retain() << std::endl;
    if (packet_id)
      std::cout << "packet_id: " << *packet_id << std::endl;
    std::cout << "topic_name: " << topic_name << std::endl;
    std::cout << "contents: " << contents << std::endl;

    client->async_disconnect();
    return true;
  });

  client->async_connect();

  m_context.run();
}
