//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>

#include <mqtt_client_cpp.hpp>

#include "agent_test_helper.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/source/adapter/mqtt/mqtt_broker_adapter.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::configuration;
using namespace mtconnect::source::adapter::mqtt_adapter;

using namespace std::literals;

const string ServerCertFile(TEST_RESOURCE_DIR "/user.crt");
const string ServerKeyFile(TEST_RESOURCE_DIR "/user.key");
const string ServerCAFile(TEST_RESOURCE_DIR "/rootca.crt");
const string ClientCAFile(TEST_RESOURCE_DIR "/clientca.crt");
const string ClientCertFile(TEST_RESOURCE_DIR "/client.crt");
const string ClientKeyFile(TEST_RESOURCE_DIR "/client.key");

// main
int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MqttBrokerAdapterTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25);
  }

  void TearDown() override
  {
    m_clients.clear();
    m_agentTestHelper->getAgent()->stop();
    m_agentTestHelper->m_ioContext.run_for(100ms);
    m_adapter.reset();
    m_agentTestHelper.reset();
  }

  void startBroker(const map<string, string>& settings)
  {
    boost::property_tree::ptree block;
    block.put(MqttPort, "0");
    block.put(MqttHost, "127.0.0.1");
    block.put(Topics, "LinuxCNC/#");
    for (const auto& [k, v] : settings)
      block.put(k, v);

    ConfigOptions options {{Device, "LinuxCNC"s}};
    m_adapter = make_shared<MqttBrokerAdapter>(m_agentTestHelper->m_ioContext.get(),
                                               m_agentTestHelper->m_context, options, block);
    m_agentTestHelper->getAgent()->addSource(m_adapter, false);
    m_agentTestHelper->getAgent()->start();

    ASSERT_TRUE(waitFor(5s, [this]() { return m_adapter->isIngesting(); }));
    ASSERT_NE(0, m_adapter->getPort());
  }

  template <typename Client>
  optional<MQTT_NS::connect_return_code> connect(Client& client,
                                                 const optional<string>& user = nullopt,
                                                 const optional<string>& password = nullopt)
  {
    optional<MQTT_NS::connect_return_code> result;
    bool closed = false;
    client->set_client_id("test_"s + to_string(m_clients.size()));
    client->set_clean_session(true);
    if (user)
      client->set_user_name(*user);
    if (password)
      client->set_password(*password);
    client->set_connack_handler([&result](bool, MQTT_NS::connect_return_code rc) {
      result = rc;
      return true;
    });
    client->set_close_handler([&closed]() { closed = true; });
    client->set_error_handler([&closed](MQTT_NS::error_code) { closed = true; });
    client->async_connect([&closed](MQTT_NS::error_code ec) {
      if (ec)
        closed = true;
    });
    waitFor(5s, [&]() { return result.has_value() || closed; });

    client->set_connack_handler([](bool, MQTT_NS::connect_return_code) { return true; });
    client->set_close_handler([]() {});
    client->set_error_handler([](MQTT_NS::error_code) {});
    m_clients.push_back(client);

    return result;
  }

  template <typename Client>
  void publish(Client& client, const string& topic, const string& payload)
  {
    client->async_publish(topic, payload, MQTT_NS::qos::at_least_once);
  }

  observation::ObservationPtr latest(const string& id)
  {
    return m_agentTestHelper->getAgent()->getCircularBuffer().getLatest().getObservation(id);
  }

  bool waitForValue(const string& id, const string& value)
  {
    return waitFor(5s, [&]() {
      auto obs = latest(id);
      return obs && obs->getValue<string>() == value;
    });
  }

  template <typename Rep, typename Period>
  bool waitFor(const chrono::duration<Rep, Period>& time, function<bool()> pred)
  {
    auto end = chrono::steady_clock::now() + time;
    while (!pred() && chrono::steady_clock::now() < end)
      m_agentTestHelper->m_ioContext.run_for(50ms);
    return pred();
  }

  auto& ioc() { return m_agentTestHelper->m_ioContext.get(); }

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::shared_ptr<MqttBrokerAdapter> m_adapter;
  std::list<std::shared_ptr<void>> m_clients;
};

TEST_F(MqttBrokerAdapterTest, should_ingest_published_observations)
{
  startBroker({});

  auto client = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(client));

  publish(client, "LinuxCNC/line", "204");
  ASSERT_TRUE(waitForValue("p3", "204"));
}

TEST_F(MqttBrokerAdapterTest, should_only_ingest_matching_topics)
{
  startBroker({{Topics, "LinuxCNC/program"}});

  auto client = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(client));

  publish(client, "LinuxCNC/line", "204");
  publish(client, "LinuxCNC/program", "main.ngc");
  ASSERT_TRUE(waitForValue("p4", "main.ngc"));

  auto line = latest("p3");
  ASSERT_TRUE(line);
  ASSERT_TRUE(line->isUnavailable());
}

TEST_F(MqttBrokerAdapterTest, should_mirror_publications_to_subscribers)
{
  startBroker({});

  auto subscriber = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(subscriber));

  string received;
  subscriber->set_publish_handler([&received](MQTT_NS::optional<std::uint16_t>,
                                              MQTT_NS::publish_options, MQTT_NS::buffer topic,
                                              MQTT_NS::buffer contents) {
    received = string(topic) + "=" + string(contents);
    return true;
  });
  bool subscribed = false;
  subscriber->set_suback_handler(
      [&subscribed](std::uint16_t, std::vector<MQTT_NS::suback_return_code>) {
        subscribed = true;
        return true;
      });
  subscriber->async_subscribe("LinuxCNC/#", MQTT_NS::qos::at_least_once);
  ASSERT_TRUE(waitFor(5s, [&]() { return subscribed; }));

  auto publisher = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(publisher));
  publish(publisher, "LinuxCNC/line", "100");

  ASSERT_TRUE(waitFor(5s, [&]() { return !received.empty(); }));
  ASSERT_EQ("LinuxCNC/line=100", received);
  ASSERT_TRUE(waitForValue("p3", "100"));
}

TEST_F(MqttBrokerAdapterTest, should_require_user_name_and_password)
{
  startBroker({{MqttUserName, "device"}, {MqttPassword, "secret"}});

  auto anonymous = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  auto rc = connect(anonymous);
  ASSERT_NE(MQTT_NS::connect_return_code::accepted, rc.value_or(MQTT_NS::connect_return_code::accepted));

  auto wrong = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  rc = connect(wrong, "device"s, "wrong"s);
  ASSERT_NE(MQTT_NS::connect_return_code::accepted, rc.value_or(MQTT_NS::connect_return_code::accepted));

  auto client = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(client, "device"s, "secret"s));

  publish(client, "LinuxCNC/line", "301");
  ASSERT_TRUE(waitForValue("p3", "301"));
}

TEST_F(MqttBrokerAdapterTest, should_load_users_and_acls_from_security_file)
{
  auto file = filesystem::temp_directory_path() / "mqtt_broker_security_test.json";
  {
    ofstream out(file);
    out << R"({
  # Comments are allowed
  "authentication": [
    { "name": "writer", "method": "plain_password", "password": "w" },
    { "name": "reader", "method": "plain_password", "password": "r" }
  ],
  "authorization": [
    { "topic": "#", "allow": { "pub": ["writer"], "sub": ["writer", "reader"] } }
  ]
})";
  }

  startBroker({{MqttBrokerSecurity, file.string()}});

  auto reader = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(reader, "reader"s, "r"s));
  publish(reader, "LinuxCNC/line", "1");

  auto writer = MQTT_NS::make_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(writer, "writer"s, "w"s));
  publish(writer, "LinuxCNC/program", "writer.ngc");

  ASSERT_TRUE(waitForValue("p4", "writer.ngc"));
  ASSERT_TRUE(latest("p3")->isUnavailable());

  filesystem::remove(file);
}

TEST_F(MqttBrokerAdapterTest, should_accept_tls_connections)
{
  startBroker({{MqttTls, "true"},
               {TlsCertificateChain, ServerCertFile},
               {TlsPrivateKey, ServerKeyFile},
               {MqttUserName, "device"},
               {MqttPassword, "secret"}});

  auto client = MQTT_NS::make_tls_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  client->get_ssl_context().load_verify_file(ServerCAFile);
  client->get_ssl_context().set_verify_mode(boost::asio::ssl::verify_peer);
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(client, "device"s, "secret"s));

  publish(client, "LinuxCNC/line", "42");
  ASSERT_TRUE(waitForValue("p3", "42"));
}

TEST_F(MqttBrokerAdapterTest, should_require_client_certificate_when_verifying)
{
  startBroker({{MqttTls, "true"},
               {TlsCertificateChain, ServerCertFile},
               {TlsPrivateKey, ServerKeyFile},
               {TlsVerifyClientCertificate, "true"},
               {TlsClientCAs, ClientCAFile}});

  auto noCert = MQTT_NS::make_tls_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  noCert->get_ssl_context().load_verify_file(ServerCAFile);
  ASSERT_FALSE(connect(noCert));

  auto client = MQTT_NS::make_tls_async_client(ioc(), "127.0.0.1", m_adapter->getPort());
  client->get_ssl_context().load_verify_file(ServerCAFile);
  client->get_ssl_context().use_certificate_chain_file(ClientCertFile);
  client->get_ssl_context().use_private_key_file(ClientKeyFile, boost::asio::ssl::context::pem);
  ASSERT_EQ(MQTT_NS::connect_return_code::accepted, connect(client));

  publish(client, "LinuxCNC/line", "7");
  ASSERT_TRUE(waitForValue("p3", "7"));
}
