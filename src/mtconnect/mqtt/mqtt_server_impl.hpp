//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/log/trivial.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/uuid/name_generator_sha1.hpp>

#include <inttypes.h>
#include <mqtt/async_client.hpp>
#include <mqtt/setup_log.hpp>
#include <mqtt_server_cpp.hpp>

#include "mqtt_server.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/mqtt/mqtt_adapter.hpp"

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
    class MqttServerImpl : public MqttServer
    {
    public:
      /// @brief Create an Mqtt server with an asio context and options
      /// @param context a boost asio context
      /// @param options configuration options
      /// - Port, defaults to 0/1883
      /// - MqttTls, defaults to false
      /// - ServerIp, defaults to 127.0.0.1/LocalHost
      MqttServerImpl(boost::asio::io_context &ioContext, const ConfigOptions &options)
        : MqttServer(ioContext),
          m_options(options),
          m_host(*GetOption<std::string>(options, configuration::ServerIp))
      {
        std::stringstream url;
        url << "mqtt://" << m_host << ':' << m_port;
        m_url = url.str();
      }

      ~MqttServerImpl() { stop(); }

      Derived &derived() { return static_cast<Derived &>(*this); }

      /// @brief Start the Mqtt server

      bool start() override
      {
        NAMED_SCOPE("MqttServer::start");

        auto &server = derived().createServer();

        server.set_accept_handler([&server, this](con_sp_t spep) {
          auto &ep = *spep;
          std::weak_ptr<con_t> wp = spep;
          using packet_id_t = typename std::remove_reference_t<decltype(ep)>::packet_id_t;
          LOG(info) << "Server: Accepted" << std::endl;
          // For server close if ep is closed.
          auto g = MQTT_NS::shared_scope_guard([&server] {
            LOG(info) << "Server: session end" << std::endl;
            server.close();
          });
          // Pass spep to keep lifetime.
          // It makes sure wp.lock() never return nullptr in the handlers below
          // including close_handler and error_handler.
          ep.start_session(std::make_tuple(std::move(spep), std::move(g)));
          ep.set_connect_handler([this, wp](MQTT_NS::buffer client_id,
                                            MQTT_NS::optional<MQTT_NS::buffer> username,
                                            MQTT_NS::optional<MQTT_NS::buffer> password,
                                            MQTT_NS::optional<MQTT_NS::will> will,
                                            bool clean_session, std::uint16_t keep_alive) {
            using namespace MQTT_NS::literals;
            LOG(info) << "Server: Client_id    : " << client_id << std::endl;
            LOG(info) << "Server: User Name     : " << (username ? username.value() : "none"_mb)
                      << std::endl;
            LOG(info) << "Server: Password     : " << (password ? password.value() : "none"_mb)
                      << std::endl;
            LOG(info) << "Server: Clean_session: " << std::boolalpha << clean_session << std::endl;
            LOG(info) << "Server: Keep_alive   : " << keep_alive << std::endl;
            auto sp = wp.lock();
            if (!sp)
            {
              LOG(error) << "Server: Endpoint has been deleted";
              return false;
            }
            if (will)
              m_will = will;
            m_connections.insert(sp);
            sp->connack(false, MQTT_NS::connect_return_code::accepted);
            return true;
          });

          ep.set_close_handler([this, wp]() {
            LOG(info) << "MQTT "
                      << ": Server closed";
            auto con = wp.lock();
            if (!con)
            {
              LOG(error) << "Server Endpoint has been deleted";
              return false;
            }
            m_connections.erase(con);
            auto &idx = m_subs.get<tag_con>();
            auto r = idx.equal_range(con);
            idx.erase(r.first, r.second);

            return true;
          });

          ep.set_error_handler([this, wp](mqtt::error_code ec) {
            LOG(error) << "error: " << ec.message();
            auto con = wp.lock();
            if (!con)
            {
              LOG(error) << "Server Endpoint has been deleted";
              return false;
            }
            m_connections.erase(con);
            auto &idx = m_subs.get<tag_con>();
            auto r = idx.equal_range(con);
            idx.erase(r.first, r.second);

            return true;
          });

          ep.set_subscribe_handler(
              [this, wp](packet_id_t packet_id, std::vector<MQTT_NS::subscribe_entry> entries) {
                LOG(debug) << "Server: Subscribe received. packet_id: " << packet_id << std::endl;
                std::vector<MQTT_NS::suback_return_code> res;
                res.reserve(entries.size());
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                if (!sp)
                {
                  LOG(error) << "Server Endpoint has been deleted";
                  return false;
                }
                for (auto const &e : entries)
                {
                  LOG(debug) << "Server: topic_filter: " << e.topic_filter
                             << " qos: " << e.subopts.get_qos() << std::endl;
                  res.emplace_back(MQTT_NS::qos_to_suback_return_code(e.subopts.get_qos()));
                  m_subs.emplace(std::move(e.topic_filter), sp, e.subopts.get_qos());
                }
                sp->suback(packet_id, res);
                return true;
              });

          ep.set_publish_handler([this](mqtt::optional<std::uint16_t> packet_id,
                                        mqtt::publish_options pubopts, mqtt::buffer topic_name,
                                        mqtt::buffer contents) {
            LOG(debug) << "Server: publish received."
                       << " dup: " << pubopts.get_dup() << " qos: " << pubopts.get_qos()
                       << " retain: " << pubopts.get_retain() << std::endl;

            if (packet_id)
              LOG(debug) << "Server packet_id: " << *packet_id;

            LOG(debug) << "Server topic_name: " << topic_name;
            LOG(debug) << "Server contents: " << contents;

            auto const &idx = m_subs.get<tag_topic>();
            auto r = idx.equal_range(topic_name);
            for (; r.first != r.second; ++r.first)
            {
              r.first->con->publish(topic_name, contents,
                                    std::min(r.first->qos_value, pubopts.get_qos()));
            }

            return true;
          });

          return true;
        });

        server.listen();
        m_port = server.port();

        return true;
      }

      /// @brief Stop the Mqtt server
      void stop() override
      {
        auto &server = derived().getServer();
        auto url = m_url;

        if (server)
        {
          LOG(info) << "MQTT "
                    << ": Server closed";

          server->close();
        }
      }

    protected:
      ConfigOptions m_options;
      std::set<con_sp_t> m_connections;
      mi_sub_con m_subs;
      std::string m_host;
    };

    /// @brief Create an Mqtt TCP server
    class MqttTcpServer : public MqttServerImpl<MqttTcpServer>
    {
    public:
      using base = MqttServerImpl<MqttTcpServer>;
      using base::base;
      using server = MQTT_NS::server<>;

      /// @brief Create an Mqtt TCP Server with an asio context and options
      /// @param context a boost asio context
      /// @param options configuration options
      /// - Port, defaults to 0/1883
      /// - MqttTls, defaults to false
      /// - ServerIp, defaults to 127.0.0.1/LocalHost
      MqttTcpServer(boost::asio::io_context &ioContext, const ConfigOptions &options)
        : base(ioContext, options)
      {
        m_port = GetOption<int>(options, configuration::MqttPort).value_or(1883);
      }

      /// @brief Get the Mqtt TCP Server
      /// @return pointer to the Mqtt TCP Server
      auto &getServer() { return m_server; }

      auto &createServer()
      {
        if (!m_server)
        {
          m_server.emplace(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_port),
                           m_ioContext);
        }

        return *m_server;
      }

    protected:
      std::optional<server> m_server;
    };

    /// @brief Create an Mqtt TLS server
    class MqttTlsServer : public MqttServerImpl<MqttTlsServer>
    {
    public:
      using base = MqttServerImpl<MqttTlsServer>;

      /// @brief Create an Mqtt TLS Server with an asio context and options
      /// @param context a boost asio context
      /// @param options configuration options
      /// - Port, defaults to 0/1883
      /// - MqttTls, defaults to True
      /// - ServerIp, defaults to 127.0.0.1/LocalHost
      MqttTlsServer(boost::asio::io_context &ioContext, const ConfigOptions &options)
        : base(ioContext, options)
      {
        m_port = GetOption<int>(options, configuration::Port).value_or(8883);
      }

      using base::base;
      using server = MQTT_NS::server_tls<>;

      /// @brief Get the Mqtt TLS Server
      /// @return pointer to the Mqtt TLS Server
      auto &getServer() { return m_server; }

      auto &createServer()
      {
        if (!m_server)
        {
          boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12);
          ctx.set_options(boost::asio::ssl::context::default_workarounds |
                          boost::asio::ssl::context::single_dh_use);

          if (HasOption(m_options, configuration::TlsCertificateChain) &&
              HasOption(m_options, configuration::TlsPrivateKey) &&
              HasOption(m_options, configuration::TlsDHKey))
          {
            LOG(info) << "Server: Initializing TLS support";
            if (HasOption(m_options, configuration::TlsCertificatePassword))
            {
              ctx.set_password_callback(
                  [this](size_t, boost::asio::ssl::context_base::password_purpose) -> string {
                    return *GetOption<string>(m_options, configuration::TlsCertificatePassword);
                  });
            }

            auto serverPrivateKey = GetOption<string>(m_options, configuration::TlsPrivateKey);
            auto serverCert = GetOption<string>(m_options, configuration::TlsCertificateChain);
            auto serverDHKey = GetOption<string>(m_options, configuration::TlsDHKey);

            ctx.use_certificate_chain_file(*serverCert);
            ctx.use_private_key_file(*serverPrivateKey, boost::asio::ssl::context::pem);
            ctx.use_tmp_dh_file(*serverDHKey);

            if (IsOptionSet(m_options, configuration::TlsVerifyClientCertificate))
            {
              LOG(info) << "Server: Will only accept client connections with valid certificates";

              ctx.set_verify_mode(boost::asio::ssl::verify_peer);
              if (HasOption(m_options, configuration::TlsClientCAs))
              {
                LOG(info) << "Server: Adding Client Certificates.";
                ctx.load_verify_file(*GetOption<string>(m_options, configuration::TlsClientCAs));
              }
            }
          }
          m_server.emplace(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_port),
                           std::move(ctx), m_ioContext);
        }

        return *m_server;
      }

    protected:
      std::optional<server> m_server;
    };

    /// @brief Create an Mqtt TLS with WebSocket Server
    class MqttTlsWSServer : public MqttServerImpl<MqttTlsWSServer>
    {
    public:
      using base = MqttServerImpl<MqttTlsWSServer>;

      /// @brief Create an Mqtt TLS WebSocket Server with an asio context and options
      /// @param context a boost asio context
      /// @param options configuration options
      /// - Port, defaults to 0/1883
      /// - MqttTls, defaults to True
      /// - ServerIp, defaults to 127.0.0.1/LocalHost
      MqttTlsWSServer(boost::asio::io_context &ioContext, const ConfigOptions &options)
        : base(ioContext, options)
      {
        m_port = GetOption<int>(options, configuration::Port).value_or(8883);
      }

      using base::base;
      using server = MQTT_NS::server_tls_ws<>;

      /// @brief Get the Mqtt TLS WebSocket Server
      /// @return pointer to the Mqtt TLS WebSocket Server
      auto &getServer() { return m_server; }

      auto &createServer()
      {
        if (!m_server)
        {
          boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12);
          ctx.set_options(boost::asio::ssl::context::default_workarounds |
                          boost::asio::ssl::context::single_dh_use);

          if (HasOption(m_options, configuration::TlsCertificateChain) &&
              HasOption(m_options, configuration::TlsPrivateKey))
          {
            auto serverPrivateKey = GetOption<string>(m_options, configuration::TlsPrivateKey);
            auto serverCert = GetOption<string>(m_options, configuration::TlsCertificateChain);
            ctx.use_certificate_chain_file(*serverCert);
            ctx.use_private_key_file(*serverPrivateKey, boost::asio::ssl::context::pem);
          }

          m_server.emplace(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_port),
                           std::move(ctx), m_ioContext);
        }

        return *m_server;
      }

    protected:
      std::optional<server> m_server;
    };
  }  // namespace mqtt_server
}  // namespace mtconnect
