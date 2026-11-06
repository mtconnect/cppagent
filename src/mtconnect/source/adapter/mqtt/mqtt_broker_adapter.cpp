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

#include "mqtt_broker_adapter.hpp"

#include <boost/asio/ip/address.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <fstream>
#include <mqtt/broker/broker.hpp>
#include <mqtt_server_cpp.hpp>
#include <openssl/ssl.h>

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/mqtt/mqtt_client_impl.hpp"

using namespace std;
namespace asio = boost::asio;
namespace pt = boost::property_tree;

namespace mtconnect {
  using namespace pipeline;
  using namespace mqtt_client;

  namespace source::adapter::mqtt_adapter {
    namespace mb = MQTT_NS::broker;

    /// @brief Embedded MQTT broker with an external listener for devices and an internal
    ///        loopback listener used by the adapter to subscribe to the ingest topics.
    class MqttBroker : public std::enable_shared_from_this<MqttBroker>
    {
    public:
      MqttBroker(asio::io_context& io, const ConfigOptions& options)
        : m_ioContext(io), m_options(options), m_broker(io)
      {
        // The ingest user only exists inside this process and can only subscribe to the
        // configured Topics.
        boost::uuids::random_generator gen;
        m_ingestUser = "mtconnect-ingest-" + boost::uuids::to_string(gen());
        m_ingestPassword = boost::uuids::to_string(gen()) + boost::uuids::to_string(gen());
        m_broker.set_security(makeSecurity());
      }

      ~MqttBroker() { close(); }

      /// @brief Start the external and internal listeners
      /// @return the port the external listener is bound to
      uint16_t start(const asio::ip::address& address, uint16_t port)
      {
        asio::ip::tcp::endpoint endpoint(address, port);
        bool tls = IsOptionSet(m_options, configuration::MqttTls);
        bool ws = IsOptionSet(m_options, configuration::MqttWs);

        if (tls && ws)
          m_port = listen(m_tlsWsServer.emplace(endpoint, makeTlsContext(), m_ioContext), true);
        else if (tls)
          m_port = listen(m_tlsServer.emplace(endpoint, makeTlsContext(), m_ioContext), true);
        else if (ws)
          m_port = listen(m_wsServer.emplace(endpoint, m_ioContext), false);
        else
          m_port = listen(m_tcpServer.emplace(endpoint, m_ioContext), false);

        m_internalPort = listen(
            m_internalServer.emplace(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0),
                                     m_ioContext),
            false);

        LOG(info) << "MQTT Broker: listening on " << address << ':' << m_port
                  << (tls ? " (TLS)" : "") << (ws ? " (WebSocket)" : "");

        return m_port;
      }

      /// @brief Close the listeners and disconnect all clients
      void stop()
      {
        if (!close())
          return;

        // The broker's connection handlers capture the broker, keep it alive until the aborted
        // operations from the closed connections have completed.
        if (!m_ioContext.stopped())
        {
          auto timer = make_shared<asio::steady_timer>(m_ioContext, 2s);
          timer->async_wait([self = shared_from_this(), timer](boost::system::error_code) {});
        }
      }

      auto getPort() const { return m_port; }
      auto getInternalPort() const { return m_internalPort; }
      const auto& getIngestUser() const { return m_ingestUser; }
      const auto& getIngestPassword() const { return m_ingestPassword; }

    protected:
      bool close()
      {
        if (!m_running)
          return false;
        m_running = false;

        if (m_tcpServer)
          m_tcpServer->close();
        if (m_tlsServer)
          m_tlsServer->close();
        if (m_wsServer)
          m_wsServer->close();
        if (m_tlsWsServer)
          m_tlsWsServer->close();
        if (m_internalServer)
          m_internalServer->close();

        {
          std::lock_guard<std::mutex> lock(m_mutex);
          for (auto& wp : m_connections)
          {
            if (auto con = wp.lock())
              con->force_disconnect();
          }
          m_connections.clear();
        }

        m_broker.clear_all_sessions();
        m_broker.clear_all_retained_topics();

        return true;
      }

      template <typename Server>
      uint16_t listen(Server& server, bool tls)
      {
        if constexpr (std::is_same_v<Server, MQTT_NS::server_tls<>> ||
                      std::is_same_v<Server, MQTT_NS::server_tls_ws<>>)
        {
          // mqtt_cpp always requests a peer certificate, so accept any presented certificate
          // unless client verification is required.
          bool verify = IsOptionSet(m_options, configuration::TlsVerifyClientCertificate);
          server.set_verify_callback(
              [verify](bool preverified, asio::ssl::verify_context&,
                       std::shared_ptr<MQTT_NS::optional<std::string>> const&) {
                return !verify || preverified;
              });
        }

        server.set_accept_handler([this, tls](mb::con_sp_t spep) { accept(std::move(spep), tls); });
        server.set_error_handler([this](MQTT_NS::error_code ec) {
          if (m_running && ec != asio::error::operation_aborted)
            LOG(error) << "MQTT Broker: accept failed: " << ec.message();
        });
        server.listen();
        m_running = true;

        return server.port();
      }

      void accept(mb::con_sp_t spep, bool tls)
      {
        if (tls && IsOptionSet(m_options, configuration::TlsVerifyClientCertificate) &&
            !hasVerifiedPeerCertificate(*spep))
        {
          LOG(warning) << "MQTT Broker: rejecting connection without a valid client certificate";
          boost::system::error_code ec;
          spep->socket().lowest_layer().close(ec);
          return;
        }

        {
          std::lock_guard<std::mutex> lock(m_mutex);
          m_connections.remove_if([](const auto& wp) { return wp.expired(); });
          m_connections.emplace_back(spep);
        }

        m_broker.handle_accept(std::move(spep));
      }

      static bool hasVerifiedPeerCertificate(mb::endpoint_t& ep)
      {
        try
        {
          auto ssl = MQTT_NS::any_cast<SSL*>(ep.socket().native_handle());
          if (auto cert = SSL_get1_peer_certificate(ssl))
          {
            X509_free(cert);
            return SSL_get_verify_result(ssl) == X509_V_OK;
          }
        }
        catch (std::exception& e)
        {
          LOG(error) << "MQTT Broker: cannot get TLS session: " << e.what();
        }
        return false;
      }

      asio::ssl::context makeTlsContext()
      {
        using namespace configuration;
        using ctx_t = asio::ssl::context;

        auto cert = GetOption<string>(m_options, TlsCertificateChain);
        auto key = GetOption<string>(m_options, TlsPrivateKey);
        if (!cert || !key)
        {
          string msg("MQTT Broker: MqttTls requires TlsCertificateChain and TlsPrivateKey");
          LOG(fatal) << msg;
          throw FatalException(msg);
        }

        ctx_t ctx(ctx_t::tls_server);
        ctx.set_options(ctx_t::default_workarounds | ctx_t::no_sslv2 | ctx_t::no_sslv3 |
                        ctx_t::no_tlsv1 | ctx_t::no_tlsv1_1 | ctx_t::single_dh_use);

        if (auto password = GetOption<string>(m_options, TlsCertificatePassword))
        {
          ctx.set_password_callback(
              [password = *password](size_t, ctx_t::password_purpose) { return password; });
        }

        ctx.use_certificate_chain_file(*cert);
        ctx.use_private_key_file(*key, ctx_t::pem);
        if (auto dh = GetOption<string>(m_options, TlsDHKey))
          ctx.use_tmp_dh_file(*dh);

        if (IsOptionSet(m_options, TlsVerifyClientCertificate))
        {
          LOG(info) << "MQTT Broker: Will only accept client connections with valid certificates";
          if (auto cas = GetOption<string>(m_options, TlsClientCAs))
            ctx.load_verify_file(*cas);
          else
            LOG(warning) << "MQTT Broker: TlsVerifyClientCertificate without TlsClientCAs";
        }

        return ctx;
      }

      /// @brief Build the broker security from either MqttBrokerSecurity or
      ///        MqttUserName/MqttPassword, and add the internal ingest user.
      mb::security makeSecurity()
      {
        using namespace configuration;

        auto user = [](const string& name, const string& method) {
          pt::ptree u;
          u.put("name", name);
          u.put("method", method);
          return u;
        };
        auto names = [](const StringList& list) {
          pt::ptree a;
          for (const auto& n : list)
            a.push_back({"", pt::ptree(n)});
          return a;
        };
        auto rule = [&names](const string& topic, const StringList& pub, const StringList& sub) {
          pt::ptree r, allow;
          r.put("topic", topic);
          if (!pub.empty())
            allow.add_child("pub", names(pub));
          if (!sub.empty())
            allow.add_child("sub", names(sub));
          r.add_child("allow", allow);
          return r;
        };

        pt::ptree root;
        if (auto file = GetOption<string>(m_options, MqttBrokerSecurity))
        {
          ifstream in(*file);
          if (!in)
          {
            string msg("MQTT Broker: Cannot open MqttBrokerSecurity file: " + *file);
            LOG(fatal) << msg;
            throw FatalException(msg);
          }
          istringstream json(mb::json_remove_comments(in));
          pt::read_json(json, root);
          LOG(info) << "MQTT Broker: Loaded security configuration from " << *file;
        }
        else
        {
          string name;
          pt::ptree authentication;
          if (auto username = GetOption<string>(m_options, MqttUserName))
          {
            auto password = GetOption<string>(m_options, MqttPassword);
            if (!password)
            {
              string msg("MQTT Broker: MqttPassword is required with MqttUserName");
              LOG(fatal) << msg;
              throw FatalException(msg);
            }
            name = *username;
            auto u = user(name, "plain_password");
            u.put("password", *password);
            authentication.push_back({"", u});
          }
          else
          {
            LOG(warning) << "MQTT Broker: No MqttUserName or MqttBrokerSecurity given, clients "
                            "can connect anonymously";
            name = "anonymous";
            authentication.push_back({"", user(name, "anonymous")});
          }

          pt::ptree authorization;
          authorization.push_back({"", rule("#", {name}, {name})});
          root.add_child("authentication", authentication);
          root.add_child("authorization", authorization);
        }

        // Add the internal ingest user, it can only subscribe to the ingest topics
        auto ingest = user(m_ingestUser, "plain_password");
        ingest.put("password", m_ingestPassword);
        if (!root.get_child_optional("authentication"))
          root.add_child("authentication", pt::ptree());
        root.get_child("authentication").push_back({"", ingest});

        if (!root.get_child_optional("authorization"))
          root.add_child("authorization", pt::ptree());
        auto& authorization = root.get_child("authorization");
        if (auto topics = GetOption<StringList>(m_options, Topics))
        {
          for (const auto& topic : *topics)
            authorization.push_back({"", rule(topic, {}, {m_ingestUser})});
        }

        stringstream json;
        pt::write_json(json, root);
        mb::security security;
        security.load_json(json);
        return security;
      }

    protected:
      asio::io_context& m_ioContext;
      ConfigOptions m_options;
      mb::broker_t m_broker;
      bool m_running {false};

      std::optional<MQTT_NS::server<>> m_tcpServer;
      std::optional<MQTT_NS::server_tls<>> m_tlsServer;
      std::optional<MQTT_NS::server_ws<>> m_wsServer;
      std::optional<MQTT_NS::server_tls_ws<>> m_tlsWsServer;
      std::optional<MQTT_NS::server<>> m_internalServer;

      uint16_t m_port {0};
      uint16_t m_internalPort {0};

      std::string m_ingestUser;
      std::string m_ingestPassword;

      std::mutex m_mutex;
      std::list<std::weak_ptr<mb::endpoint_t>> m_connections;
    };

    MqttBrokerAdapter::MqttBrokerAdapter(boost::asio::io_context& io,
                                         pipeline::PipelineContextPtr pipelineContext,
                                         const ConfigOptions& options,
                                         const boost::property_tree::ptree& block)
      : Adapter("MQTT Broker", io, options),
        m_ioContext(io),
        m_pipeline(pipelineContext, Source::m_strand)
    {
      using namespace configuration;

      GetOptions(block, m_options, options);

      // Listener, transport, and credentials must come from the adapter block, the top level
      // values of these options configure the MQTT sinks.
      for (auto o : {MqttHost, MqttPort, MqttTls, MqttWs, MqttUserName, MqttPassword,
                     MqttBrokerSecurity, MqttClientId})
        m_options.erase(o);

      AddOptions(block, m_options,
                 {{UUID, string()},
                  {Manufacturer, string()},
                  {AdapterIdentity, string()},
                  {Station, string()},
                  {Url, string()},
                  {Topics, StringList()},
                  {TlsCertificateChain, string()},
                  {TlsPrivateKey, string()},
                  {TlsCertificatePassword, string()},
                  {TlsDHKey, string()},
                  {TlsClientCAs, string()},
                  {MqttUserName, string()},
                  {MqttPassword, string()},
                  {MqttBrokerSecurity, string()},
                  {MqttHost, string()},
                  {MqttPort, int()}});

      AddDefaultedOptions(block, m_options,
                          {{MqttTls, false},
                           {MqttWs, false},
                           {TlsVerifyClientCertificate, false},
                           {AutoAvailable, false},
                           {RealTime, false},
                           {RelativeTime, false}});
      MqttAdapter::loadTopics(block, m_options);

      bool tls = IsOptionSet(m_options, MqttTls);
      m_host = GetOption<string>(m_options, MqttHost)
                   .value_or(GetOption<string>(m_options, Host).value_or("0.0.0.0"));
      m_port = GetOption<int>(m_options, MqttPort)
                   .value_or(GetOption<int>(m_options, Port).value_or(tls ? 8883 : 1883));

      stringstream url;
      url << (tls ? "mqtts://" : "mqtt://") << m_host << ':' << m_port << '/';
      m_name = url.str();

      if (auto ident = GetOption<string>(m_options, AdapterIdentity))
      {
        m_identity = *ident;
      }
      else
      {
        stringstream identity;
        identity << m_name;
        if (auto topics = GetOption<StringList>(m_options, Topics))
        {
          for (const auto& s : *topics)
            identity << s;
        }

        m_identity = CreateIdentityHash(identity.str());
        m_options[AdapterIdentity] = m_identity;
      }

      m_broker = make_shared<MqttBroker>(m_ioContext, m_options);

      m_handler = m_pipeline.makeHandler();
      m_pipeline.m_handler = m_handler.get();
      m_pipeline.build(m_options);
    }

    MqttBrokerAdapter::~MqttBrokerAdapter()
    {
      if (m_client)
        m_client->stop();
      if (m_broker)
        m_broker->stop();
    }

    void MqttBrokerAdapter::registerFactory(SourceFactory& factory)
    {
      factory.registerFactory(
          "mqtt_broker",
          [](const std::string& name, boost::asio::io_context& io,
             pipeline::PipelineContextPtr context, const ConfigOptions& options,
             const boost::property_tree::ptree& block) -> source::SourcePtr {
            return std::make_shared<MqttBrokerAdapter>(io, context, options, block);
          });
    }

    unsigned int MqttBrokerAdapter::getPort() const
    {
      if (m_broker && m_broker->getPort() != 0)
        return m_broker->getPort();
      return m_port;
    }

    bool MqttBrokerAdapter::start()
    {
      using namespace configuration;

      m_pipeline.start();

      boost::system::error_code ec;
      auto address = asio::ip::make_address(m_host, ec);
      if (ec)
      {
        LOG(error) << "MQTT Broker: Invalid bind address '" << m_host << "': " << ec.message();
        return false;
      }

      try
      {
        m_broker->start(address, static_cast<uint16_t>(m_port));
      }
      catch (std::exception& e)
      {
        LOG(error) << "MQTT Broker: Cannot start broker on " << m_host << ':' << m_port << ": "
                   << e.what();
        return false;
      }

      // The ingest subscriber connects to the broker on the loopback listener
      ConfigOptions clientOptions {{MqttHost, "127.0.0.1"s},
                                   {MqttPort, int(m_broker->getInternalPort())},
                                   {MqttUserName, m_broker->getIngestUser()},
                                   {MqttPassword, m_broker->getIngestPassword()},
                                   {MqttClientId, m_broker->getIngestUser()},
                                   {MqttConnectInterval, Seconds(1)}};

      auto clientHandler = make_unique<ClientHandler>();
      clientHandler->m_connecting = [this](shared_ptr<MqttClient> client) {
        m_handler->m_connecting(m_identity);
      };
      clientHandler->m_connected = [this](shared_ptr<MqttClient> client) {
        client->connectComplete();
        m_handler->m_connected(m_identity);
        subscribeToTopics();
      };
      clientHandler->m_disconnected = [this](shared_ptr<MqttClient> client) {
        m_handler->m_disconnected(m_identity);
      };
      clientHandler->m_receive = [this](shared_ptr<MqttClient> client, const std::string& topic,
                                        const std::string& payload) {
        m_handler->m_processMessage(topic, payload, m_identity);
      };

      m_client = make_shared<MqttTcpClient>(m_ioContext, clientOptions, std::move(clientHandler));
      return m_client->start();
    }

    void MqttBrokerAdapter::stop()
    {
      if (m_client)
        m_client->stop();
      if (m_broker)
        m_broker->stop();
      m_pipeline.clear();
    }

    void MqttBrokerAdapter::subscribeToTopics()
    {
      if (auto topics = GetOption<StringList>(m_options, configuration::Topics))
      {
        for (const auto& topic : *topics)
          m_client->subscribe(topic);
      }
    }
  }  // namespace source::adapter::mqtt_adapter
}  // namespace mtconnect
