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

#include <boost/algorithm/string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/name_generator_sha1.hpp>

#include <chrono>
#include <inttypes.h>
#include <mqtt/async_client.hpp>
#include <mqtt/setup_log.hpp>
#include <mqtt/will.hpp>
#include <random>

#include "mqtt_client.hpp"
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

  /// @brief MQTT Cient namespace
  namespace mqtt_client {
    template <typename... Ts>
    using mqtt_client_ptr = decltype(mqtt::make_async_client(std::declval<Ts>()...));
    template <typename... Ts>
    using mqtt_tls_client_ptr = decltype(mqtt::make_tls_async_client(std::declval<Ts>()...));
    template <typename... Ts>
    using mqtt_client_ws_ptr = decltype(mqtt::make_async_client_ws(std::declval<Ts>()...));
    template <typename... Ts>
    using mqtt_tls_client_ws_ptr = decltype(mqtt::make_tls_async_client_ws(std::declval<Ts>()...));

    using mqtt_client = mqtt_client_ptr<boost::asio::io_context &, std::string, std::uint16_t,
                                        mqtt::protocol_version>;
    using mqtt_tls_client = mqtt_tls_client_ptr<boost::asio::io_context &, std::string,
                                                std::uint16_t, mqtt::protocol_version>;
    using mqtt_tls_client_ws =
        mqtt_tls_client_ws_ptr<boost::asio::io_context &, std::string, std::uint16_t, std::string,
                               mqtt::protocol_version>;
    using mqtt_client_ws = mqtt_client_ws_ptr<boost::asio::io_context &, std::string, std::uint16_t,
                                              std::string, mqtt::protocol_version>;

    /// @brief The Mqtt Client Source
    template <typename Derived>
    class MqttClientImpl : public MqttClient
    {
    public:
      /// @brief Create an Mqtt Client with an asio context and options
      /// @param context a boost asio context
      /// @param options configuration options
      /// - Port, defaults to 1883
      /// - MqttTls, defaults to false
      /// - MqttHost, defaults to LocalHost
      MqttClientImpl(boost::asio::io_context &ioContext, const ConfigOptions &options,
                     std::unique_ptr<ClientHandler> &&handler,
                     const std::optional<std::string> willTopic = std::nullopt,
                     const std::optional<std::string> willPayload = std::nullopt)
        : MqttClient(ioContext, std::move(handler), willTopic, willPayload),
          m_options(options),
          m_host(GetOption<std::string>(options, configuration::MqttHost).value_or("localhost")),
          m_port(GetOption<int>(options, configuration::MqttPort).value_or(1883)),
          m_reconnectTimer(ioContext)
      {
        m_username = GetOption<std::string>(options, configuration::MqttUserName);
        m_password = GetOption<std::string>(options, configuration::MqttPassword);

        std::stringstream url;
        url << "mqtt://" << m_host << ':' << m_port << '/';
        m_url = url.str();

        // Some brokers require specific ClientID provided.
        // If not specified in configuration then generate random one.
        auto client_id = GetOption<string>(options, configuration::MqttClientId);
        if (client_id)
        {
          m_identity = *client_id;
        }
        else
        {
          using namespace boost::uuids;
          std::stringstream identity;
          const auto now = std::chrono::high_resolution_clock::now();

          auto seed = now.time_since_epoch().count();
          std::mt19937_64 gen(seed);
          identity << "mtc_" << std::hex << gen();
          m_identity = identity.str();
        }

        LOG(debug) << "Using ClientID " << m_identity;

        auto ci = GetOption<Seconds>(options, configuration::MqttConnectInterval);
        if (ci)
          m_connectInterval = *ci;
      }

      ~MqttClientImpl() { stop(); }

      Derived &derived() { return static_cast<Derived &>(*this); }

      /// @brief Start the Mqtt Client
      bool start() override
      {
        NAMED_SCOPE("MqttClient::start");

        auto client = derived().getClient();

        client->set_client_id(m_identity);
        client->clean_session();
        client->set_keep_alive_sec(10);

        client->set_connack_handler([this](bool sp, mqtt::connect_return_code ec) {
          if (!m_running)
          {
            return false;
          }
          else if (ec == mqtt::connect_return_code::accepted)
          {
            LOG(info) << "MQTT ConnAck: MQTT Connected";

            if (m_handler && m_handler->m_connected)
            {
              m_handler->m_connected(shared_from_this());
            }
            else
            {
              LOG(debug) << "No connect handler, setting connected";
              m_connected = true;
            }
          }
          else
          {
            LOG(info) << "MQTT ConnAck: MQTT connection failed: " << ec;
            disconnected();
          }
          return true;
        });

        client->set_close_handler([this]() {
          LOG(info) << "MQTT " << m_url << ": connection closed";
          // Queue on a strand
          m_connected = false;
          if (m_handler && m_handler->m_disconnected)
            m_handler->m_disconnected(shared_from_this());

          if (m_running)
          {
            disconnected();
            return true;
          }
          else
          {
            return false;
          }
        });

        client->set_error_handler([this](mqtt::error_code ec) {
          LOG(error) << "error: " << ec.message();
          m_connected = false;
          if (m_running)
            disconnected();
        });

        client->set_publish_handler([this](mqtt::optional<std::uint16_t> packet_id,
                                           mqtt::publish_options pubopts, mqtt::buffer topic_name,
                                           mqtt::buffer contents) {
          if (packet_id)
            LOG(debug) << "packet_id: " << *packet_id;
          LOG(debug) << "topic_name: " << topic_name;
          LOG(debug) << "contents: " << contents;

          if (m_running)
          {
            receive(topic_name, contents);
            return true;
          }
          else
          {
            return false;
          }
        });

        if (m_willTopic && m_willPayload)
        {
          uint32_t will_expiry_interval = 1;
          MQTT_NS::v5::properties ps {
              MQTT_NS::v5::property::message_expiry_interval(will_expiry_interval),
          };

          mqtt::buffer topic(std::string_view(m_willTopic->c_str()));
          mqtt::buffer payload(std::string_view(m_willPayload->c_str()));

          client->set_will(mqtt::will(topic, payload, mqtt::retain::yes, mqtt::force_move(ps)));
        }

        m_running = true;
        connect();

        return true;
      }

      /// @brief Stop the Mqtt Client
      void stop() override
      {
        if (m_running)
        {
          m_running = false;

          m_reconnectTimer.cancel();
          auto client = derived().getClient();
          auto url = m_url;

          if (client)
          {
            client->async_disconnect(10s, [client, url](mqtt::error_code ec) {
              LOG(warning) << url << " disconnected: " << ec.message();
              client->set_close_handler(nullptr);
            });

            client.reset();
          }
        }
      }

      /// @brief Subscribe Topic to the Mqtt Client
      /// @param topic Subscribing to the topic
      /// @return boolean either topic sucessfully connected and subscribed
      bool subscribe(const std::string &topic) override
      {
        NAMED_SCOPE("MqttClientImpl::subscribe");
        if (!m_connected)
        {
          LOG(debug) << "Not connected, cannot publish to " << topic;
          return false;
        }

        LOG(debug) << "Subscribing to topic: " << topic;
        m_packetId = derived().getClient()->acquire_unique_packet_id();
        derived().getClient()->async_subscribe(
            m_packetId, topic.c_str(), mqtt::qos::at_least_once, [topic](mqtt::error_code ec) {
              if (ec)
              {
                LOG(error) << "Subscribe failed: " << topic << ": " << ec.message();
                return false;
              }
              else
              {
                LOG(debug) << "Subscribed to: " << topic;
                return true;
              }
            });

        return true;
      }

      /// @brief Publish Topic to the Mqtt Client
      /// @param topic Publishing to the topic
      /// @param payload Publishing to the payload
      /// @return boolean either topic sucessfully connected and published
      bool publish(const std::string &topic, const std::string &payload, bool retain = true,
                   QOS qos = QOS::at_least_once) override
      {
        NAMED_SCOPE("MqttClientImpl::publish");
        if (!m_connected)
        {
          LOG(debug) << "Not connected, cannot publish to " << topic;
          return false;
        }

        mqtt::qos mqos;
        switch (qos)
        {
          case QOS::at_most_once:
            mqos = mqtt::qos::at_most_once;
            break;

          case QOS::at_least_once:
            mqos = mqtt::qos::at_least_once;
            break;

          case QOS::exactly_once:
            mqos = mqtt::qos::exactly_once;
            break;
        }

        mqtt::retain mretain;
        if (retain)
          mretain = mqtt::retain::yes;
        else
          mretain = mqtt::retain::no;

        m_packetId = derived().getClient()->acquire_unique_packet_id();
        derived().getClient()->async_publish(
            m_packetId, topic, payload, mqos | mretain, [topic](mqtt::error_code ec) {
              if (ec)
              {
                LOG(error) << "MqttClientImpl::publish: Publish failed to topic " << topic << ": "
                           << ec.message();
              }
            });

        return true;
      }

      /// @brief Publish Topic to the Mqtt Client
      /// @param topic Publishing to the topic
      /// @param payload Publishing to the payload
      /// @return boolean either topic sucessfully connected and published
      bool asyncPublish(const std::string &topic, const std::string &payload,
                        std::function<void(std::error_code)> callback, bool retain = true,
                        QOS qos = QOS::at_least_once) override
      {
        NAMED_SCOPE("MqttClientImpl::publish");
        if (!m_connected)
        {
          LOG(debug) << "Not connected, cannot publish to " << topic;
          return false;
        }

        mqtt::qos mqos;
        switch (qos)
        {
          case QOS::at_most_once:
            mqos = mqtt::qos::at_most_once;
            break;

          case QOS::at_least_once:
            mqos = mqtt::qos::at_least_once;
            break;

          case QOS::exactly_once:
            mqos = mqtt::qos::exactly_once;
            break;
        }

        mqtt::retain mretain;
        if (retain)
          mretain = mqtt::retain::yes;
        else
          mretain = mqtt::retain::no;

        m_packetId = derived().getClient()->acquire_unique_packet_id();
        derived().getClient()->async_publish(
            m_packetId, topic, payload, mqos | mretain, [topic, callback](mqtt::error_code ec) {
              if (ec)
              {
                LOG(error) << "MqttClientImpl::publish: Publish failed to topic " << topic << ": "
                           << ec.message();
              }
              callback(ec);
            });

        return true;
      }

    protected:
      void connect()
      {
        if (m_handler && m_handler->m_connecting)
          m_handler->m_connecting(shared_from_this());

        derived().getClient()->set_clean_session(true);
        derived().getClient()->async_connect([this](mqtt::error_code ec) {
          if (ec)
          {
            LOG(warning) << "MqttClientImpl::connect: cannot connect: " << ec.message()
                         << ", will retry";

            reconnect();
          }
          else
          {
            LOG(info) << "MqttClientImpl::connect: connected";
          }
        });
      }

      void receive(mqtt::buffer &topic, mqtt::buffer &contents)
      {
        if (m_handler && m_handler->m_receive)
          m_handler->m_receive(shared_from_this(), string(topic), string(contents));
      }

      void disconnected()
      {
        NAMED_SCOPE("MqttClientImpl::disconnected");

        LOG(info) << "Calling handler disconnected";

        if (m_handler && m_handler->m_disconnected)
          m_handler->m_disconnected(shared_from_this());

        if (m_running)
          reconnect();
      }

      /// <summary>
      ///
      /// </summary>
      void reconnect()
      {
        NAMED_SCOPE("MqttClientImpl::reconnect");

        if (!m_running)
          return;

        LOG(info) << "Start reconnect timer";

        // Set an expiry time relative to now.
        m_reconnectTimer.expires_after(m_connectInterval);

        m_reconnectTimer.async_wait(boost::asio::bind_executor(
            derived().getClient()->get_executor(), [this](const boost::system::error_code &error) {
              if (error != boost::asio::error::operation_aborted)
              {
                LOG(info) << "MqttClientImpl::reconnect: reconnect now";

                // Connect
                derived().getClient()->async_connect([this](mqtt::error_code ec) {
                  LOG(info) << "MqttClientImpl::reconnect async_connect callback: " << ec.message();
                  if (ec && ec != boost::asio::error::operation_aborted)
                  {
                    reconnect();
                  }
                });
              }
            }));
      }

    protected:
      ConfigOptions m_options;

      std::string m_host;

      unsigned int m_port {1883};

      std::uint16_t m_packetId {0};

      std::optional<std::string> m_username;
      std::optional<std::string> m_password;

      boost::asio::steady_timer m_reconnectTimer;
    };

    /// @brief Create an Mqtt TCP Client
    class AGENT_LIB_API MqttTcpClient : public MqttClientImpl<MqttTcpClient>
    {
    public:
      using base = MqttClientImpl<MqttTcpClient>;
      using base::base;
      /// @brief Get a shared pointer to this
      /// @return shared pointer to this
      shared_ptr<MqttTcpClient> getptr()
      {
        return static_pointer_cast<MqttTcpClient>(shared_from_this());
      }

      /// @brief Get the Mqtt TCP Client
      /// @return pointer to the Mqtt TCP Client
      auto &getClient()
      {
        if (!m_client)
        {
          m_client = mqtt::make_async_client(m_ioContext, m_host, m_port);
          if (m_username)
            m_client->set_user_name(*m_username);
          if (m_password)
            m_client->set_password(*m_password);
        }

        return m_client;
      }

    protected:
      mqtt_client m_client;
    };

    /// @brief Create an Mqtt TLS Client
    class MqttTlsClient : public MqttClientImpl<MqttTlsClient>
    {
    public:
      using base = MqttClientImpl<MqttTlsClient>;
      using base::base;
      /// @brief Get a shared pointer to this
      /// @return shared pointer to this
      shared_ptr<MqttTlsClient> getptr()
      {
        return static_pointer_cast<MqttTlsClient>(shared_from_this());
      }

      /// @brief Get the Mqtt TLS Client
      /// @return pointer to the Mqtt TLS Client
      auto &getClient()
      {
        if (!m_client)
        {
          m_client = mqtt::make_tls_async_client(m_ioContext, m_host, m_port);
          if (m_username)
            m_client->set_user_name(*m_username);
          if (m_password)
            m_client->set_password(*m_password);

          auto cacert = GetOption<string>(m_options, configuration::MqttCaCert);
          if (cacert)
          {
            m_client->get_ssl_context().load_verify_file(*cacert);
          }

          auto private_key = GetOption<string>(m_options, configuration::MqttPrivateKey);
          auto cert = GetOption<string>(m_options, configuration::MqttCert);
          if (private_key && cert)
          {
            m_client->get_ssl_context().set_verify_mode(boost::asio::ssl::verify_peer);
            m_client->get_ssl_context().use_certificate_chain_file(*cert);
            m_client->get_ssl_context().use_private_key_file(*private_key,
                                                             boost::asio::ssl::context::pem);
          }
        }

        return m_client;
      }

    protected:
      mqtt_tls_client m_client;
    };

    /// @brief Create an Mqtt TLS WebSocket Client
    class MqttTlsWSClient : public MqttClientImpl<MqttTlsWSClient>
    {
    public:
      using base = MqttClientImpl<MqttTlsWSClient>;
      using base::base;
      /// @brief Get a shared pointer to this
      /// @return shared pointer to this
      shared_ptr<MqttTlsWSClient> getptr()
      {
        return static_pointer_cast<MqttTlsWSClient>(shared_from_this());
      }

      /// @brief Get the Mqtt TLS WebSocket Client
      /// @return pointer to the Mqtt TLS WebSocket Client
      auto &getClient()
      {
        if (!m_client)
        {
          m_client = mqtt::make_tls_async_client_ws(m_ioContext, m_host, m_port);
          if (m_username)
            m_client->set_user_name(*m_username);
          if (m_password)
            m_client->set_password(*m_password);

          auto cacert = GetOption<string>(m_options, configuration::MqttCaCert);
          if (cacert)
          {
            m_client->get_ssl_context().load_verify_file(*cacert);
          }
        }

        return m_client;
      }

    protected:
      mqtt_tls_client_ws m_client;
    };

    /// @brief Create an Mqtt TLS WebSocket Client
    class MqttWSClient : public MqttClientImpl<MqttWSClient>
    {
    public:
      using base = MqttClientImpl<MqttWSClient>;
      using base::base;
      /// @brief Get a shared pointer to this
      /// @return shared pointer to this
      shared_ptr<MqttWSClient> getptr()
      {
        return static_pointer_cast<MqttWSClient>(shared_from_this());
      }

      /// @brief Get the Mqtt TLS WebSocket Client
      /// @return pointer to the Mqtt TLS WebSocket Client
      auto &getClient()
      {
        if (!m_client)
        {
          m_client = mqtt::make_async_client_ws(m_ioContext, m_host, m_port);
          if (m_username)
            m_client->set_user_name(*m_username);
          if (m_password)
            m_client->set_password(*m_password);
        }

        return m_client;
      }

    protected:
      mqtt_client_ws m_client;
    };

  }  // namespace mqtt_client
}  // namespace mtconnect
