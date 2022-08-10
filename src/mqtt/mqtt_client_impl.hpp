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

#include <boost/log/trivial.hpp>
#include <boost/uuid/name_generator_sha1.hpp>

#include <inttypes.h>
#include <mqtt/async_client.hpp>
#include <mqtt/setup_log.hpp>

#include "configuration/config_options.hpp"
#include "mqtt_client.hpp"
#include "source/adapter/adapter.hpp"
#include "source/adapter/mqtt/mqtt_adapter.hpp"

using namespace std;
namespace asio = boost::asio;

namespace mtconnect {
  using namespace observation;
  using namespace entity;
  using namespace pipeline;
  using namespace source::adapter;

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

    template <typename Derived>
    class MqttClientImpl : public MqttClient
    {
    public:
      MqttClientImpl(boost::asio::io_context &ioContext, const ConfigOptions &options,
                     std::unique_ptr<ClientHandler> &&handler)
        : MqttClient(ioContext, move(handler)),
          m_options(options),
          m_host(*GetOption<std::string>(options, configuration::Host)),
          m_port(GetOption<int>(options, configuration::Port).value_or(1883)),
          m_reconnectTimer(ioContext)
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

      ~MqttClientImpl() { stop(); }
      
      Derived &derived() { return static_cast<Derived &>(*this); }

      bool start() override
      {
        NAMED_SCOPE("MqttClient::start");

        //mqtt::setup_log();

        auto client = derived().getClient();

        client->set_client_id(m_identity);
        client->clean_session();
        client->set_keep_alive_sec(10);

        client->set_connack_handler([this](bool sp, mqtt::connect_return_code connack_return_code) {
          if (!m_running)
          {
            return false;
          }
          else if (connack_return_code == mqtt::connect_return_code::accepted)
          {
            m_connected = true;
            if (m_handler && m_handler->m_connected)
              m_handler->m_connected(shared_from_this());
          }
          else
          {
            reconnect();
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
            reconnect();
            return true;
          }
          else
          {
            return false;
          }
        });

        client->set_error_handler([this](mqtt::error_code ec) {
          LOG(error) << "error: " << ec.message();
          if (m_running)
            reconnect();
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
        /// <summary>
        ///
        /// </summary>
        /// <returns></returns>
        m_running = true;
        connect();

        return true;
      }

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
            });
            
            client.reset();
          }
        }
      }

      bool subscribe(const std::string &topic) override
      {
        NAMED_SCOPE("MqttClientImpl::subscribe");
        if (!m_running)
          return false;

        m_clientId = derived().getClient()->acquire_unique_packet_id();
        derived().getClient()->async_subscribe(
            m_clientId, topic.c_str(), mqtt::qos::at_least_once, [](mqtt::error_code ec) {
              if (ec)
              {
                LOG(error) << "Subscribe failed: " << ec.message();
                return false;
              }
              return true;
            });

        return true;
      }
      
      bool publish(const std::string &topic, const std::string &payload) override
      {
        NAMED_SCOPE("MqttClientImpl::publish");
        if (!m_running)
          return false;

        
        m_clientId = derived().getClient()->acquire_unique_packet_id();
        derived().getClient()->async_publish(m_clientId, topic, payload, mqtt::qos::at_most_once,
                                             [](mqtt::error_code ec) {
                                               if (ec)
                                               {
                                                 LOG(error) << "Publish failed: " << ec.message();
                                               }
                                             });
        
        return true;
      }

    protected:

      void subscribe()
      {
        NAMED_SCOPE("MqttClientImpl::subscribe");
        if (!m_running)
          return;

        auto topics = GetOption<StringList>(m_options, configuration::Topics);
        std::vector<std::tuple<string, mqtt::subscribe_options>> topicList;
        if (topics)
        {
          for (auto &topic : *topics)
          {
            auto loc = topic.find(':');
            if (loc != string::npos)
            {
              topicList.emplace_back(make_tuple(topic.substr(loc + 1), mqtt::qos::at_least_once));
            }
          }
        }
        else
        {
          LOG(warning) << "No topics specified, subscribing to '#'";
          topicList.emplace_back(make_tuple("#"s, mqtt::qos::at_least_once));
        }

        m_clientId = derived().getClient()->acquire_unique_packet_id();
        derived().getClient()->async_subscribe(m_clientId, topicList, [](mqtt::error_code ec) {
          if (ec)
          {
            LOG(error) << "Subscribe failed: " << ec.message();
          }
        });
      }

      void connect()
      {
        if (m_handler && m_handler->m_connecting)
          m_handler->m_connecting(shared_from_this());

        derived().getClient()->async_connect();
      }

      void receive(mqtt::buffer &topic, mqtt::buffer &contents)
      {
        if (m_handler && m_handler->m_receive)
          m_handler->m_receive(shared_from_this(), string(topic), string(contents));
      }
      /// <summary>
      /// 
      /// </summary>
      void reconnect()
      {
        NAMED_SCOPE("Mqtt_Client::reconnect");

        if (!m_running)
          return;

        LOG(info) << "Start reconnect timer";

        // Set an expiry time relative to now.
        m_reconnectTimer.expires_after(std::chrono::seconds(5));

        m_reconnectTimer.async_wait([this](const boost::system::error_code &error) {
          if (error != boost::asio::error::operation_aborted)
          {
            LOG(info) << "Reconnect now !!";

            // Connect
            derived().getClient()->async_connect([this](mqtt::error_code ec) {
              LOG(info) << "async_connect callback: " << ec.message();
              if (ec && ec != boost::asio::error::operation_aborted)
              {
                reconnect();
              }
            });
          }
        });
      }

    protected:
      ConfigOptions m_options;

      std::string m_host;
      unsigned int m_port { 1883 };

      std::uint16_t m_clientId {0};

      boost::asio::steady_timer m_reconnectTimer;
    };

    class MqttTcpClient : public MqttClientImpl<MqttTcpClient>
    {
    public:
      using base = MqttClientImpl<MqttTcpClient>;
      using base::base;

      auto &getClient()
      {
        if (!m_client)
        {
          m_client = mqtt::make_async_client(m_ioContext, m_host, m_port);
        }

        return m_client;
      }

    protected:
      mqtt_client m_client;
    };

    class MqttTlsClient : public MqttClientImpl<MqttTlsClient>
    {
    public:
      using base = MqttClientImpl<MqttTlsClient>;
      using base::base;

      auto &getClient()
      {
        if (!m_client)
        {
          m_client = mqtt::make_tls_async_client(m_ioContext, m_host, m_port);
          auto cacert = GetOption<string>(m_options, configuration::MqttCaCert);
          if (cacert)
          {
            m_client->get_ssl_context().load_verify_file(*cacert);
          }
        }

        return m_client;
      }

    protected:
      mqtt_tls_client m_client;
    };

  }  // namespace mqtt_client
}  // namespace mtconnect
