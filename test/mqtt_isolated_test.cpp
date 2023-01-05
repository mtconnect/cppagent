
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.

//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at

//      http://www.apache.org/licenses/LICENSE-2.0

//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <string>

#include <nlohmann/json.hpp>

#include "agent_test_helper.hpp"
#include "buffer/checkpoint.hpp"
#include "device_model/data_item/data_item.hpp"
#include "entity/json_parser.hpp"
#include "mqtt/mqtt_client_impl.hpp"
#include "mqtt/mqtt_server_impl.hpp"
#include "printer/json_printer.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::configuration;
using namespace mtconnect::device_model::data_item;
using namespace mtconnect::sink::mqtt_sink;
using namespace mtconnect::sink::rest_sink;

using json = nlohmann::json;

const string MqttClientCACert(PROJECT_ROOT_DIR "/test/resources/rootca.crt");
const string MqttClientCert(PROJECT_ROOT_DIR "/test/resources/client.crt");
const string MqttClientKey {PROJECT_ROOT_DIR "/test/resources/client.key"};

const string ServerCertFile(PROJECT_ROOT_DIR "/test/resources/user.crt");
const string ServerKeyFile {PROJECT_ROOT_DIR "/test/resources/user.key"};
const string ServerDhFile {PROJECT_ROOT_DIR "/test/resources/dh2048.pem"};

class MqttIsolatedUnitTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_jsonPrinter = std::make_unique<printer::JsonPrinter>(2, true);
  }

  void TearDown() override
  {
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

  void createServer(const ConfigOptions &options)
  {
    bool withTlsOption = IsOptionSet(options, configuration::MqttTls);

    ConfigOptions opts(options);
    MergeOptions(opts, {{ServerIp, "127.0.0.1"s},
                        {MqttPort, 0},
                        {MqttTls, withTlsOption},
                        {AutoAvailable, false},
                        {TlsCertificateChain, ServerCertFile},
                        {TlsPrivateKey, ServerKeyFile},
                        {TlsCertificatePassword, "mtconnect"s},
                        {RealTime, false}});

    if (withTlsOption)
    {
      m_server =
          make_shared<mtconnect::mqtt_server::MqttTlsServer>(m_agentTestHelper->m_ioContext, opts);
    }
    else
    {
      m_server =
          make_shared<mtconnect::mqtt_server::MqttTcpServer>(m_agentTestHelper->m_ioContext, opts);
    }
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
    bool withTlsOption = IsOptionSet(options, configuration::MqttTls);

    ConfigOptions opts(options);
    MergeOptions(opts, {{MqttHost, "127.0.0.1"s},
                        {MqttPort, m_port},
                        {MqttTls, withTlsOption},
                        {AutoAvailable, false},
                        {MqttCaCert, MqttClientCACert},
                        {MqttCert, MqttClientCert},
                        {MqttPrivateKey, MqttClientKey},
                        {RealTime, false}});

    if (withTlsOption)
    {
      m_client = make_shared<mtconnect::mqtt_client::MqttTlsClient>(m_agentTestHelper->m_ioContext,
                                                                    opts, move(handler));
    }
    else
    {
      m_client = make_shared<mtconnect::mqtt_client::MqttTcpClient>(m_agentTestHelper->m_ioContext,
                                                                    opts, move(handler));
    }
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

  std::unique_ptr<printer::JsonPrinter> m_jsonPrinter;
  std::shared_ptr<mtconnect::mqtt_server::MqttServer> m_server;
  std::shared_ptr<MqttClient> m_client;
  std::shared_ptr<MqttService> m_service;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  uint16_t m_port {0};
};

TEST_F(MqttIsolatedUnitTest, mqtt_client_should_connect_to_broker)
{
  ConfigOptions options;
  createServer(options);

  startServer();

  ASSERT_NE(0, m_port);

  auto handler = make_unique<ClientHandler>();

  createClient(options, move(handler));

  ASSERT_TRUE(startClient());

  ASSERT_TRUE(m_client->isConnected());
}

TEST_F(MqttIsolatedUnitTest, mqtt_tcp_client_should_receive_loopback_publication)
{
  ConfigOptions options;

  createServer(options);
  startServer();

  ASSERT_NE(0, m_port);

  std::uint16_t pid_sub1;

  auto client = mqtt::make_async_client(m_agentTestHelper->m_ioContext, "localhost", m_port);

  client->set_client_id("cliendId1");
  client->set_clean_session(true);
  client->set_keep_alive_sec(30);

  client->set_connack_handler([&client, &pid_sub1](bool sp,
                                                   mqtt::connect_return_code connack_return_code) {
    std::cout << "Connack handler called" << std::endl;
    std::cout << "Session Present: " << std::boolalpha << sp << std::endl;
    std::cout << "Connack Return Code: " << connack_return_code << std::endl;
    if (connack_return_code == mqtt::connect_return_code::accepted)
    {
      pid_sub1 = client->acquire_unique_packet_id();

      client->async_subscribe(pid_sub1, "mqtt_tcp_client_cpp/topic1", MQTT_NS::qos::at_most_once,
                              //[optional] checking async_subscribe completion code
                              [](MQTT_NS::error_code ec) {
                                EXPECT_FALSE(ec);
                                std::cout << "async_tcp_subscribe callback: " << ec.message()
                                          << std::endl;
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
          client->async_publish("mqtt_tcp_client_cpp/topic1", "test1", MQTT_NS::qos::at_most_once,
                                //[optional] checking async_publish completion code
                                [](MQTT_NS::error_code ec) {
                                  EXPECT_FALSE(ec);

                                  std::cout << "async_tcp_publish callback: " << ec.message()
                                            << std::endl;
                                  EXPECT_EQ(ec.message(), "Success");
                                });
          return true;
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
          client->async_publish("mqtt_tcp_client_cpp/topic1", "test1", MQTT_NS::qos::at_most_once,
                                //[optional] checking async_publish completion code
                                [packet_id](MQTT_NS::error_code ec) {
                                  EXPECT_FALSE(ec);

                                  std::cout << "async_tcp_publish callback: " << ec.message()
                                            << std::endl;
                                  ASSERT_TRUE(packet_id);
                                });
        }

        return true;
      });

  bool received = false;
  client->set_publish_handler([&client, &received](mqtt::optional<std::uint16_t> packet_id,
                                                   mqtt::publish_options pubopts,
                                                   mqtt::buffer topic_name, mqtt::buffer contents) {
    std::cout << "publish received."
              << " dup: " << pubopts.get_dup() << " qos: " << pubopts.get_qos()
              << " retain: " << pubopts.get_retain() << std::endl;
    if (packet_id)
      std::cout << "packet_id: " << *packet_id << std::endl;
    std::cout << "topic_name: " << topic_name << std::endl;
    std::cout << "contents: " << contents << std::endl;

    EXPECT_EQ("mqtt_tcp_client_cpp/topic1", topic_name);
    EXPECT_EQ("test1", contents);

    client->async_disconnect();
    received = true;
    return true;
  });

  client->async_connect();

  m_agentTestHelper->m_ioContext.run();

  ASSERT_TRUE(received);
}

TEST_F(MqttIsolatedUnitTest, should_connect_using_tls)
{
  GTEST_SKIP();

  ConfigOptions options {{configuration::MqttTls, true}};

  createServer(options);

  startServer();

  ASSERT_NE(0, m_port);

  auto handler = make_unique<ClientHandler>();

  createClient(options, move(handler));

  ASSERT_TRUE(startClient());

  ASSERT_TRUE(m_client->isConnected());
}

TEST_F(MqttIsolatedUnitTest, should_connect_using_tls_ws)
{
  GTEST_SKIP();

  ConfigOptions options;
  MergeOptions(options, {{ServerIp, "127.0.0.1"s},
                         {MqttPort, 0},
                         {MqttTls, true},
                         {AutoAvailable, false},
                         {TlsCertificateChain, ServerCertFile},
                         {TlsPrivateKey, ServerKeyFile},
                         {RealTime, false}});

  m_server =
      make_shared<mtconnect::mqtt_server::MqttTlsWSServer>(m_agentTestHelper->m_ioContext, options);

  startServer();

  ASSERT_NE(0, m_port);

  auto handler = make_unique<ClientHandler>();

  ConfigOptions opts;
  MergeOptions(opts, {{MqttHost, "127.0.0.1"s},
                      {MqttPort, m_port},
                      {MqttTls, true},
                      {AutoAvailable, false},
                      {MqttCaCert, MqttClientCACert},
                      {MqttCert, MqttClientCert},
                      {MqttPrivateKey, MqttClientKey},
                      {RealTime, false}});

  m_client = make_shared<mtconnect::mqtt_client::MqttTlsWSClient>(m_agentTestHelper->m_ioContext,
                                                                  opts, move(handler));

  ASSERT_TRUE(startClient());

  ASSERT_TRUE(m_client->isConnected());
}

TEST_F(MqttIsolatedUnitTest, should_conenct_using_authentication) { GTEST_SKIP(); }
