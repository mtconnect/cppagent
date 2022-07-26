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
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <mqtt_server_cpp.hpp>

#include "agent_test_helper.hpp"
#include "mqtt/mqtt_client_impl.hpp"
#include "mqtt/mqtt_server_impl.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"

using json = nlohmann::json;

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::mqtt_sink;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;

class MqttSinkTest : public testing::Test
{
protected:
  void SetUp() override { m_agentTestHelper = make_unique<AgentTestHelper>(); }

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
    options.emplace("MqttSink", true);
    m_agentTestHelper->createAgent("/samples/configuration.xml", 8, 4, "2.0", 25, false, true,
                                   options);

    m_agentTestHelper->getAgent()->start();
  }

  void createServer(const ConfigOptions& options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts(options);
    opts[Port] = 0;
    opts[ServerIp] = "127.0.0.1"s;
    m_server = make_shared<mtconnect::mqtt_server::MqttTlsServer>(m_context, opts);
  }

  void startServer()
  {
    if (m_server)
    {
      bool start = m_server->start();
      /* if (start)
         m_context.run_one();*/
    }
  }

  void createClient(const ConfigOptions& options)
  {
    m_client = make_shared<mtconnect::mqtt_client::MqttTcpClient>(m_context, options);
  }

  void startClient()
  {
    if (m_client)
      m_client->start();
  }

  std::shared_ptr<mtconnect::mqtt_server::MqttServer> m_server;
  std::shared_ptr<MqttClient> m_client;
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

const string MqttCACert(PROJECT_ROOT_DIR "/test/resources/clientca.crt");

TEST_F(MqttSinkTest, Mqtt_Sink_publish)
{
  GTEST_SKIP();
  
  ConfigOptions options {
      {configuration::Host, "localhost"s}, {configuration::Port, 0},
      {configuration::MqttTls, false},     {configuration::AutoAvailable, false},
      {configuration::RealTime, false},    {configuration::MqttCaCert, MqttCACert}};

  createServer(options);

  createClient(options);

  startServer();

  startClient();

  m_client->stop();
}

// works fine with mosquitto server not with mqtt localhost
TEST_F(MqttSinkTest, Mosquitto_Mqtt_CreateClient)
{
  std::uint16_t pid_sub1;

  boost::asio::io_context ioc;
  auto client = mqtt::make_client(ioc, "test.mosquitto.org", 1883);

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

    client->disconnect();
    return true;
  });

  client->connect();

  ioc.run();
}

// Mqtt over web sockets...
namespace mi = boost::multi_index;

using con_t = MQTT_NS::server_tls_ws<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;
using con_wp_t = std::weak_ptr<con_t>;

struct sub_con
{
  sub_con(MQTT_NS::buffer topic, con_sp_t con, MQTT_NS::qos qos_value)
    : topic(std::move(topic)), con(std::move(con)), qos_value(qos_value)
  {}
  MQTT_NS::buffer topic;
  con_sp_t con;
  MQTT_NS::qos qos_value;
};

struct tag_topic
{};
struct tag_con
{};
struct tag_con_topic
{};

using mi_sub_con = mi::multi_index_container<
    sub_con, mi::indexed_by<
                 mi::ordered_unique<
                     mi::tag<tag_con_topic>,
                     mi::composite_key<sub_con, BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con),
                                       BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)>>,
                 mi::ordered_non_unique<mi::tag<tag_topic>,
                                        BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)>,
                 mi::ordered_non_unique<mi::tag<tag_con>,
                                        BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con)>>>;

inline void close_proc(std::set<con_sp_t>& cons, mi_sub_con& subs, con_sp_t const& con)
{
  cons.erase(con);

  auto& idx = subs.get<tag_con>();
  auto r = idx.equal_range(con);
  idx.erase(r.first, r.second);
}

template <typename Server>
void server_proc(Server& s, std::set<con_sp_t>& connections, mi_sub_con& subs)
{
  s.set_error_handler(
      [](MQTT_NS::error_code ec) { std::cout << "[server] error: " << ec.message() << std::endl; });
  s.set_accept_handler([&s, &connections, &subs](con_sp_t spep) {
    auto& ep = *spep;
    std::weak_ptr<con_t> wp(spep);

    using packet_id_t = typename std::remove_reference_t<decltype(ep)>::packet_id_t;
    std::cout << "[server] accept" << std::endl;
    // For server close if ep is closed.
    auto g = MQTT_NS::shared_scope_guard([&s] {
      std::cout << "[server] session end" << std::endl;
      s.close();
    });
    // Pass spep to keep lifetime.
    // It makes sure wp.lock() never return nullptr in the handlers below
    // including close_handler and error_handler.
    ep.start_session(std::make_tuple(std::move(spep), std::move(g)));

    // set connection (lower than MQTT) level handlers
    ep.set_close_handler([&connections, &subs, wp]() {
      std::cout << "[server] closed." << std::endl;
      auto sp = wp.lock();
      BOOST_ASSERT(sp);
      close_proc(connections, subs, sp);
    });
    ep.set_error_handler([&connections, &subs, wp](MQTT_NS::error_code ec) {
      std::cout << "[server] error: " << ec.message() << std::endl;
      auto sp = wp.lock();
      BOOST_ASSERT(sp);
      close_proc(connections, subs, sp);
    });

    // set MQTT level handlers
    ep.set_connect_handler([&connections, wp](MQTT_NS::buffer client_id,
                                              MQTT_NS::optional<MQTT_NS::buffer> username,
                                              MQTT_NS::optional<MQTT_NS::buffer> password,
                                              MQTT_NS::optional<MQTT_NS::will>, bool clean_session,
                                              std::uint16_t keep_alive) {
      using namespace MQTT_NS::literals;
      std::cout << "[server] client_id    : " << client_id << std::endl;
      std::cout << "[server] username     : " << (username ? username.value() : "none"_mb)
                << std::endl;
      std::cout << "[server] password     : " << (password ? password.value() : "none"_mb)
                << std::endl;
      std::cout << "[server] clean_session: " << std::boolalpha << clean_session << std::endl;
      std::cout << "[server] keep_alive   : " << keep_alive << std::endl;
      auto sp = wp.lock();
      BOOST_ASSERT(sp);
      connections.insert(sp);
      sp->connack(false, MQTT_NS::connect_return_code::accepted);
      return true;
    });
    ep.set_disconnect_handler([&connections, &subs, wp]() {
      std::cout << "[server] disconnect received." << std::endl;
      auto sp = wp.lock();
      BOOST_ASSERT(sp);
      close_proc(connections, subs, sp);
    });
    ep.set_puback_handler([](packet_id_t packet_id) {
      std::cout << "[server] puback received. packet_id: " << packet_id << std::endl;
      return true;
    });
    ep.set_pubrec_handler([](packet_id_t packet_id) {
      std::cout << "[server] pubrec received. packet_id: " << packet_id << std::endl;
      return true;
    });
    ep.set_pubrel_handler([](packet_id_t packet_id) {
      std::cout << "[server] pubrel received. packet_id: " << packet_id << std::endl;
      return true;
    });
    ep.set_pubcomp_handler([](packet_id_t packet_id) {
      std::cout << "[server] pubcomp received. packet_id: " << packet_id << std::endl;
      return true;
    });
    ep.set_publish_handler([&subs](MQTT_NS::optional<packet_id_t> packet_id,
                                   MQTT_NS::publish_options pubopts, MQTT_NS::buffer topic_name,
                                   MQTT_NS::buffer contents) {
      std::cout << "[server] publish received."
                << " dup: " << pubopts.get_dup() << " qos: " << pubopts.get_qos()
                << " retain: " << pubopts.get_retain() << std::endl;
      if (packet_id)
        std::cout << "[server] packet_id: " << *packet_id << std::endl;
      std::cout << "[server] topic_name: " << topic_name << std::endl;
      std::cout << "[server] contents: " << contents << std::endl;
      auto const& idx = subs.get<tag_topic>();
      auto r = idx.equal_range(topic_name);
      for (; r.first != r.second; ++r.first)
      {
        r.first->con->publish(topic_name, contents,
                              std::min(r.first->qos_value, pubopts.get_qos()));
      }
      return true;
    });
    ep.set_subscribe_handler(
        [&subs, wp](packet_id_t packet_id, std::vector<MQTT_NS::subscribe_entry> entries) {
          std::cout << "[server] subscribe received. packet_id: " << packet_id << std::endl;
          std::vector<MQTT_NS::suback_return_code> res;
          res.reserve(entries.size());
          auto sp = wp.lock();
          BOOST_ASSERT(sp);
          for (auto const& e : entries)
          {
            std::cout << "[server] topic_filter: " << e.topic_filter
                      << " qos: " << e.subopts.get_qos() << std::endl;
            res.emplace_back(MQTT_NS::qos_to_suback_return_code(e.subopts.get_qos()));
            subs.emplace(std::move(e.topic_filter), sp, e.subopts.get_qos());
          }
          sp->suback(packet_id, res);
          return true;
        });
    ep.set_unsubscribe_handler(
        [&subs, wp](packet_id_t packet_id, std::vector<MQTT_NS::unsubscribe_entry> entries) {
          std::cout << "[server] unsubscribe received. packet_id: " << packet_id << std::endl;
          auto sp = wp.lock();
          for (auto const& e : entries)
          {
            auto it = subs.find(std::make_tuple(sp, e.topic_filter));
            if (it != subs.end())
            {
              subs.erase(it);
            }
          }
          BOOST_ASSERT(sp);
          sp->unsuback(packet_id);
          return true;
        });
  });

  s.listen();
}

// test case for mqtt over websockets
TEST_F(MqttSinkTest, Mqtt_WebsocketsServer)
{
  GTEST_SKIP();

  // MQTT_NS::setup_log();
  //
  // std::uint16_t port = boost::lexical_cast<std::uint16_t>(0);

  // // server
  //
  // boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12);

  ////std::cout << boost::asio::ip::address_v4::loopback() << "\n";

  // boost::asio::io_context iocs;
  // auto s = MQTT_NS::server_tls_ws<>(
  //     boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0),
  //     std::move(ctx), iocs);

  // std::set<con_sp_t> connections;
  // mi_sub_con subs;
  // server_proc(s, connections, subs);

  // iocs.run();
}
