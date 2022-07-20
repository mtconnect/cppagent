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


#include <inttypes.h>

#include "mqtt_server.hpp"


#include "configuration/config_options.hpp"

#include <mqtt/async_client.hpp>
#include <mqtt/setup_log.hpp>

#include "source/adapter/adapter.hpp"
#include "source/adapter/mqtt/mqtt_adapter.hpp"

#include <boost/log/trivial.hpp>
#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>

#include <mqtt_server_cpp.hpp>

using namespace std;
namespace asio = boost::asio;

namespace mtconnect {
  using namespace observation;
  using namespace entity;
  using namespace pipeline;
  using namespace source::adapter;
  namespace mi = boost::multi_index;

  namespace mqtt_server {

    using con_t = MQTT_NS::server_tls_ws<>::endpoint_t;
    using con_sp_t = std::shared_ptr<con_t>;

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
        sub_con,
        mi::indexed_by<
            mi::ordered_unique<
                mi::tag<tag_con_topic>,
                mi::composite_key<sub_con, BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con),
                                  BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)>>,
            mi::ordered_non_unique<mi::tag<tag_topic>,
                                   BOOST_MULTI_INDEX_MEMBER(sub_con, MQTT_NS::buffer, topic)>,
            mi::ordered_non_unique<mi::tag<tag_con>,
                                   BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con)>>>;

    template <typename Derived>
    class MqttServerBase : public MqttServerImpl
    {
    public:
      MqttServerBase(boost::asio::io_context &ioContext, const ConfigOptions &options)
        : MqttServerImpl(ioContext),
          m_options(options),
          m_host(*GetOption<std::string>(options, configuration::Host)),
          m_port(GetOption<int>(options, configuration::Port).value_or(0))
      {
        std::stringstream url;
        url << "mqtt://" << m_host << ':' << m_port;
        m_url = url.str();

        std::stringstream identity;
        identity << '_' << m_host << '_' << m_port;

        boost::uuids::detail::sha1 sha1;
        sha1.process_bytes(identity.str().c_str(), identity.str().length());
        boost::uuids::detail::sha1::digest_type digest;
        sha1.get_digest(digest);

        identity.str("");
        identity << std::hex << digest[0] << digest[1] << digest[2];
        m_identity = std::string("_") + (identity.str()).substr(0, 10);
      }

      ~MqttServerBase() { stop(); }

      Derived &derived() { return static_cast<Derived &>(*this); }

      bool start() override
      {
        NAMED_SCOPE("MqttServer::start");

        mqtt::setup_log();

        auto server = derived().getServer();

        server.set_accept_handler([&server](con_sp_t spep) {
          auto &ep = *spep;
          std::weak_ptr<con_t> wp(spep);

          using packet_id_t = typename std::remove_reference_t<decltype(ep)>::packet_id_t;
          LOG(info) << "[server] accept" << std::endl;
          // For server close if ep is closed.
          auto g = MQTT_NS::shared_scope_guard([&server] {
            LOG(info) << "[server] session end" << std::endl;
            server.close();
          });
          // Pass spep to keep lifetime.
          // It makes sure wp.lock() never return nullptr in the handlers below
          // including close_handler and error_handler.
          ep.start_session(std::make_tuple(std::move(spep), std::move(g)));

          ep.set_connack_handler([](bool sp, mqtt::connect_return_code connack_return_code) {
            // ep.set_connack_handler([/*&m_connections,*/ wp](
            //                            MQTT_NS::buffer client_id,
            //                                              MQTT_NS::optional<MQTT_NS::buffer>
            //                                              username,
            //                                              MQTT_NS::optional<MQTT_NS::buffer>
            //                                              password,
            //                                              MQTT_NS::optional<MQTT_NS::will>,
            //                                              bool clean_session,
            //                                              std::uint16_t keep_alive) {
            if (connack_return_code == mqtt::connect_return_code::accepted)
            {
              /* using namespace MQTT_NS::literals;
               LOG(info) << "[server] client_id    : " << client_id << std::endl;
               LOG(info) << "[server] username     : " << (username ? username.value() : "none"_mb)
                         << std::endl;
               LOG(info) << "[server] password     : " << (password ? password.value() : "none"_mb)
                         << std::endl;
               LOG(info) << "[server] clean_session: " << std::boolalpha << clean_session
                         << std::endl;
               LOG(info) << "[server] keep_alive   : " << keep_alive << std::endl;*/

              // auto sp = wp.lock();
              // m_connections.insert(sp);
              // sp->connack(false, MQTT_NS::connect_return_code::accepted);
            }

            return true;
          });

          ep.set_close_handler([wp]() {
            LOG(info) << "MQTT "
                      << ": server closed";
            // auto con = wp.lock();
            // m_connections.erase(con);
            /*auto &idx = m_subs.get<tag_con>();
            auto r = idx.equal_range(con);
            idx.erase(r.first, r.second);*/
          });

          ep.set_error_handler([wp](mqtt::error_code ec) {
            LOG(error) << "error: " << ec.message();
            // auto con = wp.lock();
            // m_connections.erase(con);
            /* auto &idx = m_subs.get<tag_con>();
             auto r = idx.equal_range(con);
             idx.erase(r.first, r.second);*/
          });

          /*ep.set_puback_handler(
              [](std::uint16_t packet_id, std::vector<mqtt::suback_return_code> results) {
                LOG(debug) << "server puback received. packet_id: " << packet_id;
                for (auto const &e : results)
                {
                  LOG(debug) << "suback result: " << e;
                }

                return true;
              });*/

          ep.set_publish_handler([](mqtt::optional<std::uint16_t> packet_id,
                                    mqtt::publish_options pubopts, mqtt::buffer topic_name,
                                    mqtt::buffer contents) {
            LOG(debug) << "[server] publish received."
                       << " dup: " << pubopts.get_dup() << " qos: " << pubopts.get_qos()
                       << " retain: " << pubopts.get_retain() << std::endl;

            if (packet_id)
              LOG(debug) << "server packet_id: " << *packet_id;

            LOG(debug) << "server topic_name: " << topic_name;
            LOG(debug) << "server contents: " << contents;

            /* auto const &idx = subs.get<tag_topic>();
             auto r = idx.equal_range(topic_name);
             for (; r.first != r.second; ++r.first)
             {
               r.first->con->publish(topic_name, contents,
                                     std::min(r.first->qos_value, pubopts.get_qos()));
             }*/

            return true;
          });

          // connect();

          return true;
        });

        server.listen();

        return true;
      }

      void stop() override
      {
        auto server = derived().getServer();
        auto url = m_url;

        LOG(warning) << url << "server disconnected: ";
      }

    protected:
      void subscribeHandler()
      {
        NAMED_SCOPE("MqttServerImpl::subscribeHandler");
        /*ep.set_subscribe_handler(
            [&subs, wp](packet_id_t packet_id, std::vector<MQTT_NS::subscribe_entry> entries) {
              locked_cout() << "[server] subscribe received. packet_id: " << packet_id << std::endl;
              std::vector<MQTT_NS::suback_return_code> res;
              res.reserve(entries.size());
              auto sp = wp.lock();
              BOOST_ASSERT(sp);
              for (auto const &e : entries)
              {
                locked_cout() << "[server] topic_filter: " << e.topic_filter
                              << " qos: " << e.subopts.get_qos() << std::endl;
                res.emplace_back(MQTT_NS::qos_to_suback_return_code(e.subopts.get_qos()));
                subs.emplace(std::move(e.topic_filter), sp, e.subopts.get_qos());
              }
              sp->suback(packet_id, res);
              return true;
            });*/
      }

      void unSubscribeHandler()
      {
        NAMED_SCOPE("MqttServerImpl::subscribeHandler");

        /* ep.set_unsubscribe_handler([&subs, wp](packet_id_t packet_id,
                                               std::vector<MQTT_NS::unsubscribe_entry> entries) {
          locked_cout() << "[server] unsubscribe received. packet_id: " << packet_id << std::endl;
          auto sp = wp.lock();
          for (auto const &e : entries)
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
        });*/
      }

      void publishHandler() { NAMED_SCOPE("MqttServerImpl::publishHandler"); }

      void disconnect()
      {
        /*ep.set_disconnect_handler([&connections, &subs, wp]() {
        locked_cout() << "[server] disconnect received." << std::endl;
        auto sp = wp.lock();
        BOOST_ASSERT(sp);
        close_proc(connections, subs, sp);
      });*/
      }
      void connect()
      {
        /* if (m_handler)
           m_handler->m_connecting(m_identity);*/
      }

    protected:
      ConfigOptions m_options;

      std::string m_host;

      unsigned int m_port;

      std::uint16_t m_clientId {0};

      std::set<con_sp_t> m_connections;

      mi_sub_con m_subs;
    };

    class MqttTlsServer : public MqttServerBase<MqttTlsServer>
    {
    public:
      using base = MqttServerBase<MqttTlsServer>;
      using base::base;

      auto getServer()
      {
        boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12);

        auto server = MQTT_NS::server_tls_ws<>(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_port), std::move(ctx),
            m_ioContext);

        return server;
      }
    };
  }  // namespace mqtt_server
}  // namespace mtconnect


