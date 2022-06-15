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

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "agent_test_helper.hpp"
#include "client/mqtt/mqtt_client.cpp"
#include "sink/mqtt_sink/mqtt_service.hpp"
#include "sink/rest_sink/server.hpp"

using json = nlohmann::json;

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;

class MqttSinkTest : public testing::Test
{
protected:
  void SetUp() override
  {    
    // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();

    using namespace mtconnect::configuration;
    m_server = make_unique<Server>(m_context, ConfigOptions {{Port, 0}, {ServerIp, "127.0.0.1"s}});
  }

  void TearDown() override
  {
    const auto agent = m_agentTestHelper->getAgent();
    if (agent)
    {
      m_agentTestHelper->getAgent()->stop();
      m_agentTestHelper->m_ioContext.run_for(100ms);
    }
    m_agentTestHelper.reset();

    m_server.reset();
  }

  void createAgent(ConfigOptions options = {})
  {   
    m_agentTestHelper->createAgent("/samples/configuration.xml", 8, 4, "2.0", 25, false, true,
                                   options);   

    m_agentTestHelper->getAgent()->start();
  } 

  void createServer(const ConfigOptions &options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts {{Port, 1883}, {ServerIp, "localhost"s}};
    opts.merge(ConfigOptions(options));
    m_server = make_unique<Server>(m_context, opts);
  }

  void startServer()
  {
    m_server->start();
    while (!m_server->isListening())
      m_context.run_one();
  }

  void createClient(const ConfigOptions &options)
  { 
      m_client = make_shared<mtconnect::mqtt_client::MqttClient>(m_context, options);
  }

  unique_ptr<Server> m_server;
  std::shared_ptr<MqttClientImpl> m_client;
  asio::io_context m_context;

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(MqttSinkTest, Load_Mqtt_sink)
{
  createAgent();

  auto agent = m_agentTestHelper->getAgent();

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService);
}

TEST_F(MqttSinkTest, Mqtt_Subscribe_Publish)
{
  std::uint16_t pid_sub1;

  boost::asio::io_context ioc;
  auto c = mqtt::make_client(ioc, "test.mosquitto.org", 1883);

  c->set_client_id("cliendId1");
  c->set_clean_session(true);
  c->set_keep_alive_sec(10);

  c->set_connack_handler([&c, &pid_sub1](bool sp, mqtt::connect_return_code connack_return_code) {
    std::cout << "Connack handler called" << std::endl;
    std::cout << "Session Present: " << std::boolalpha << sp << std::endl;
    std::cout << "Connack Return Code: " << connack_return_code << std::endl;
    if (connack_return_code == mqtt::connect_return_code::accepted)
    {
      pid_sub1 = c->acquire_unique_packet_id();

      c->async_subscribe(pid_sub1, "mqtt_client_cpp/topic1", MQTT_NS::qos::at_most_once,
                         // [optional] checking async_subscribe completion code
                         [](MQTT_NS::error_code ec) {
                           std::cout << "async_subscribe callback: " << ec.message() << std::endl;
                         });
    }
    return true;
  });
  c->set_close_handler([] { std::cout << "closed" << std::endl; });

  c->set_suback_handler(
      [&c, &pid_sub1](std::uint16_t packet_id, std::vector<mqtt::suback_return_code> results) {
        std::cout << "suback received. packet_id: " << packet_id << std::endl;
        for (auto const &e : results)
        {
          std::cout << "subscribe result: " << e << std::endl;
        }

        if (packet_id == pid_sub1)
        {
          c->async_publish("mqtt_client_cpp/topic1", "test1", MQTT_NS::qos::at_most_once,
                           // [optional] checking async_publish completion code
                           [](MQTT_NS::error_code ec) {
                             std::cout << "async_publish callback: " << ec.message() << std::endl;
                             ASSERT_EQ(ec.message(), "The operation completed successfully");
                           });
        }

        return true;
      });
  c->set_publish_handler([&c](mqtt::optional<std::uint16_t> packet_id,
                              mqtt::publish_options pubopts, mqtt::buffer topic_name,
                              mqtt::buffer contents) {
    std::cout << "publish received."
              << " dup: " << pubopts.get_dup() << " qos: " << pubopts.get_qos()
              << " retain: " << pubopts.get_retain() << std::endl;
    if (packet_id)
    {
      std::cout << "packet_id: " << *packet_id << std::endl;
      std::cout << "topic_name: " << topic_name << std::endl;
      std::cout << "contents: " << contents << std::endl;
    }

    c->disconnect();
    return true;
  });

  c->connect();

  ioc.run();
}

TEST_F(MqttSinkTest, Mqtt_Sink_publish)
{
    //mqtt://homeassistant:1883
  ConfigOptions options {{configuration::Host, "localhost"s}, {configuration::Port, 1883}};

  createAgent(options);

  auto agent = m_agentTestHelper->getAgent();

  const auto mqttService =
      dynamic_pointer_cast<sink::mqtt_sink::MqttService>(agent->findSink("MqttService"));

  ASSERT_TRUE(mqttService);
}