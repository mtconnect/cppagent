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
#include "device_model/data_item/data_item.hpp"
#include "mqtt/mqtt_client_impl.hpp"
#include "mqtt/mqtt_server_impl.hpp"
#include "printer/json_printer.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"
#include "sink/rest_sink/checkpoint.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::device_model::data_item;
using namespace mtconnect::sink::mqtt_sink;
using namespace mtconnect::sink::rest_sink;

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

  void createAgent(ConfigOptions options = {})
  {
    ConfigOptions opts(options);
    MergeOptions(opts, {{"MqttSink", true},
      {configuration::MqttPort, m_port},
      {configuration::MqttHost, "127.0.0.1"s}
    });
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25, false, true,
                                   opts);

    m_agentTestHelper->getAgent()->start();
  }

  void createServer(const ConfigOptions& options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts(options);
    MergeOptions(opts, {{ServerIp, "127.0.0.1"s},
                           {MqttPort, 0},
                           {MqttTls, false},
                           {AutoAvailable, false},
                           {RealTime, false}});
    
    m_server = make_shared<mtconnect::mqtt_server::MqttTcpServer>(m_agentTestHelper->m_ioContext, opts);
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
  
  void createClient(const ConfigOptions& options, unique_ptr<ClientHandler> &&handler)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts(options);
    MergeOptions(opts, {{Host, "127.0.0.1"s},
                           {MqttPort, m_port},
                           {MqttTls, false},
                           {AutoAvailable, false},
                           {RealTime, false}});
    m_client = make_shared<mtconnect::mqtt_client::MqttTcpClient>(m_agentTestHelper->m_ioContext, opts, move(handler));
  }

  bool startClient()
  {
    bool started = m_client && m_client->start();
    if (started)
    {
      m_agentTestHelper->m_ioContext.run_for(1s);
    }
    return started;
  }

  std::unique_ptr<printer::JsonPrinter> m_jsonPrinter;
  std::shared_ptr<mtconnect::mqtt_server::MqttServer> m_server;
  std::shared_ptr<MqttClient> m_client;
  std::shared_ptr<MqttService> m_service;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  uint16_t m_port { 0 };
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

  boost::asio::steady_timer timer(m_agentTestHelper->m_ioContext);
  timer.expires_from_now(5s);
  timer.async_wait([](boost::system::error_code ec) {
    if (!ec)
    {
      FAIL();
    }
  });
  
  while (!service->isConnected())
  {
    m_agentTestHelper->m_ioContext.run_for(100ms);
  }
  timer.cancel();
  
  ASSERT_TRUE(service->isConnected());
}

#if 0
TEST_F(MqttSinkTest, mqtt_sink_probe_to_subscribe)
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

  std::shared_ptr<MqttClient> client = mqttService->getClient();
  ASSERT_TRUE(client);
  ASSERT_TRUE(client->isConnected());

  std::list<DevicePtr> devices = m_agentTestHelper->m_agent->getDevices();
  
  for (auto device : devices)
  {
    const auto& dataItems = device->getDeviceDataItems();
    for (const auto& dataItemAssoc : dataItems)
    {
      auto dataItem = dataItemAssoc.second.lock();
      string dataTopic = dataItem->getTopic();
      bool sucess = client->subscribe(dataTopic);
      if (!sucess)
      {
        continue;
      }
    }
  }
}

TEST_F(MqttSinkTest, mqtt_sink_probe_to_publish)
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

  std::shared_ptr<MqttClient> client = mqttService->getClient();
  ASSERT_TRUE(client);
  ASSERT_TRUE(client->isConnected());

  std::list<DevicePtr> devices = m_agentTestHelper->m_agent->getDevices();
  ErrorList errors;
  auto time = chrono::system_clock::now();
  ObservationList observationList;

  for (auto device : devices)
  {
    const auto& dataItems = device->getDeviceDataItems();
    for (const auto& dataItemAssoc : dataItems)
    {
      auto dataItem = dataItemAssoc.second.lock();
      string dataTopic = dataItem->getTopic();
      auto dataEvent =
          Observation::make(dataItem, dataItem->getObservationProperties(), time, errors);
      observationList.push_back(dataEvent);
    }
  }
   auto jsonDoc =
      m_jsonPrinter->printSample(123, 131072, 10974584, 10843512, 10123800, observationList);

  stringstream buffer;
  buffer << jsonDoc;

  m_client->publish("Topics", buffer.str());
}

TEST_F(MqttSinkTest, mqtt_sink_publish_observations)
{
  GTEST_SKIP();

  ErrorList errors;
  auto time = chrono::system_clock::now();
  ObservationList observationList;
  auto prop = entity::Properties {{"VALUE", "123"s}};

  auto add = DataItem::make(
      {{"type", "DEVICE_ADDED"s}, {"id", "device_added"s}, {"category", "EVENT"s}}, errors);

  auto event1 = Observation::make(add, prop, time, errors);
  observationList.push_back(event1);

  auto removed = DataItem::make(
      {{"type", "DEVICE_REMOVED"s}, {"id", "device_removed"s}, {"category", "EVENT"s}}, errors);
  auto event2 = Observation::make(removed, prop, time, errors);
  observationList.push_back(event2);

  auto changed = DataItem::make(
      {{"type", "DEVICE_CHANGED"s}, {"id", "device_changed"s}, {"category", "EVENT"s}}, errors);
  auto event3 = Observation::make(changed, prop, time, errors);
  observationList.push_back(event3);

  ConfigOptions options {{configuration::Host, "localhost"s},
                         {configuration::Port, 0},
                         {configuration::MqttTls, false},
                         {configuration::AutoAvailable, false},
                         {configuration::RealTime, false}};

  createServer(options);
  startServer();

  createClient(options);

  ASSERT_TRUE(startClient());

  ASSERT_TRUE(m_client->isConnected());

  if (m_client && m_client->isConnected())
  {
    auto jsonDoc =
        m_jsonPrinter->printSample(123, 131072, 10974584, 10843512, 10123800, observationList);

    stringstream buffer;
    buffer << jsonDoc;

    m_client->publish(jsonDoc, buffer.str());
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
}

TEST_F(MqttSinkTest, mqtt_localhost_probe_to_subscribe)
{  
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
    std::list<DevicePtr> devices = m_agentTestHelper->m_agent->getDevices();

    for (auto device : devices)
    {
      const auto& dataItems = device->getDeviceDataItems();
      for (const auto& dataItemAssoc : dataItems)
      {
        auto dataItem = dataItemAssoc.second.lock();
        string dataTopic = dataItem->getTopic();
        bool sucess = m_client->subscribe(dataTopic);
        if (!sucess)
        {
          continue;
        }
      }
    }
  }
}

TEST_F(MqttSinkTest, mqtt_localhost_publish_observations)
{
  createAgent();

  ConfigOptions options {{configuration::Host, "localhost"s},
                         {configuration::Port, 0},
                         {configuration::MqttTls, false},
                         {configuration::AutoAvailable, false},
                         {configuration::RealTime, false}};

  createServer(options);

  startServer();

  createClient(options);

  ASSERT_TRUE(startClient());

  ASSERT_TRUE(m_client->isConnected());

  if (m_client && m_client->isConnected())
  {
    ErrorList errors;
    auto time = chrono::system_clock::now();
    ObservationList observationList;

    auto agent = m_agentTestHelper->getAgent();
    std::list<DevicePtr> devices = agent->getDevices();

    for (auto device : devices)
    {
      const auto& dataItems = device->getDeviceDataItems();
      for (const auto& dataItemAssoc : dataItems)
      {
        auto dataItem = dataItemAssoc.second.lock();
        string dataTopic = dataItem->getTopic();

        auto dataEvent =
            Observation::make(dataItem, dataItem->getObservationProperties(), time, errors);
        observationList.push_back(dataEvent);
      }
    }
    auto jsonDoc =
        m_jsonPrinter->printSample(123, 131072, 10974584, 10843512, 10123800, observationList);

    stringstream buffer;
    buffer << jsonDoc;

    m_client->publish("ObservationList", buffer.str());
  }
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
      return true;
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
                            [packet_id](MQTT_NS::error_code ec) {
                              std::cout << "async_publish callback: " << ec.message() << std::endl;
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
#endif
