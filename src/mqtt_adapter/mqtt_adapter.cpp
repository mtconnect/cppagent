//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mqtt_adapter.hpp"

#include <boost/log/trivial.hpp>
#include <boost/uuid/name_generator_sha1.hpp>

#include <mqtt/async_client.hpp>
#include <mqtt/setup_log.hpp>

#include "configuration/config_options.hpp"
#include "device_model/device.hpp"
#include "pipeline/convert_sample.hpp"
#include "pipeline/deliver.hpp"
#include "pipeline/delta_filter.hpp"
#include "pipeline/duplicate_filter.hpp"
#include "pipeline/message_mapper.hpp"
#include "pipeline/period_filter.hpp"
#include "pipeline/shdr_token_mapper.hpp"
#include "pipeline/shdr_tokenizer.hpp"
#include "pipeline/timestamp_extractor.hpp"
#include "pipeline/topic_mapper.hpp"
#include "pipeline/upcase_value.hpp"

using namespace std;
namespace asio = boost::asio;

namespace mtconnect
{
  using namespace observation;
  using namespace entity;
  using namespace pipeline;
  using namespace adapter;

  namespace source
  {
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
    using mqtt_client_ws = mqtt_client_ws_ptr<boost::asio::io_context &, std::string, std::uint16_t,
                                              std::string, mqtt::protocol_version>;
    using mqtt_tls_client_ws =
        mqtt_tls_client_ws_ptr<boost::asio::io_context &, std::string, std::uint16_t, std::string,
                               mqtt::protocol_version>;

    template <typename Derived>
    class MqttAdapterClientBase : public MqttAdapterImpl
    {
    public:
      MqttAdapterClientBase(boost::asio::io_context &ioContext, const ConfigOptions &options,
                            MqttPipeline *pipeline)
        : MqttAdapterImpl(ioContext),
          m_options(options),
          m_host(*GetOption<std::string>(options, configuration::Host)),
          m_port(GetOption<int>(options, configuration::Port).value_or(1883)),
          m_pipeline(pipeline),
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

      ~MqttAdapterClientBase() { stop(); }

      Derived &derived() { return static_cast<Derived &>(*this); }

      bool start() override
      {
        NAMED_SCOPE("MqttAdapterClient::start");
#ifndef NDEBUG
        mqtt::setup_log(mqtt::severity_level::debug);
#else
        mqtt::setup_log();
#endif

        auto client = derived().getClient();
        client->set_client_id(m_identity);
        client->clean_session();
        client->set_keep_alive_sec(10);

        client->set_connack_handler([this](bool sp, mqtt::connect_return_code connack_return_code) {
          if (connack_return_code == mqtt::connect_return_code::accepted)
          {
            auto entity = make_shared<Entity>(
                "ConnectionStatus", Properties {{"VALUE", "CONNECTED"s}, {"source", m_identity}});
            m_pipeline->run(entity);

            subscribe();
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
          auto entity = make_shared<Entity>(
              "ConnectionStatus", Properties {{"VALUE", "DISCONNECTED"s}, {"source", m_identity}});
          m_pipeline->run(entity);
          reconnect();
        });

        client->set_suback_handler(
            [this](std::uint16_t packet_id, std::vector<mqtt::suback_return_code> results) {
              LOG(debug) << "suback received. packet_id: " << packet_id;
              for (auto const &e : results)
              {
                LOG(debug) << "subscribe result: " << e;
                // Do something if the subscription failed...
              }

              return true;
            });

        client->set_error_handler([this](mqtt::error_code ec) {
          LOG(error) << "error: " << ec.message();
          reconnect();
        });

        client->set_publish_handler([this](mqtt::optional<std::uint16_t> packet_id,
                                           mqtt::publish_options pubopts, mqtt::buffer topic_name,
                                           mqtt::buffer contents) {
          if (packet_id)
            LOG(debug) << "packet_id: " << *packet_id;
          LOG(debug) << "topic_name: " << topic_name;
          LOG(debug) << "contents: " << contents;

          receive(topic_name, contents);

          return true;
        });

        m_running = true;
        connect();

        return true;
      }

      void stop() override
      {
        m_running = false;
        derived().getClient().reset();
      }

    protected:
      void subscribe()
      {
        NAMED_SCOPE("MqttAdapterImpl::subscribe");
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

        m_subPid = derived().getClient()->acquire_unique_packet_id();
        derived().getClient()->async_subscribe(m_subPid, topicList, [](mqtt::error_code ec) {
          if (ec)
          {
            LOG(error) << "Subscribe failed: " << ec.message();
          }
        });
      }

      void connect()
      {
        auto entity = make_shared<Entity>(
            "ConnectionStatus", Properties {{"VALUE", "CONNECTING"s}, {"source", m_identity}});
        m_pipeline->run(entity);

        derived().getClient()->async_connect();
      }

      void receive(mqtt::buffer &topic, mqtt::buffer &contents)
      {
        auto entity =
            make_shared<pipeline::Message>("Topic", Properties {{"VALUE", string(contents)},
                                                                {"topic", string(topic)},
                                                                {"source", m_identity}});
        m_pipeline->run(entity);
      }

      void reconnect()
      {
        NAMED_SCOPE("MqttAdapterClient::reconnect");

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
      unsigned int m_port;

      std::uint16_t m_subPid {0};
      bool m_running;

      MqttPipeline *m_pipeline;

      boost::asio::steady_timer m_reconnectTimer;
    };

    class MqttAdapterClient : public MqttAdapterClientBase<MqttAdapterClient>
    {
    public:
      using base = MqttAdapterClientBase<MqttAdapterClient>;
      using Client = mqtt_client;
      using base::base;

      auto getClient()
      {
        if (!m_client)
        {
          m_client = mqtt::make_async_client(m_ioContext, m_host, m_port);
        }

        return m_client;
      }

    protected:
      Client m_client;
    };

    class MqttAdapterTlsClient : public MqttAdapterClientBase<MqttAdapterTlsClient>
    {
    public:
      using base = MqttAdapterClientBase<MqttAdapterTlsClient>;
      using Client = mqtt_tls_client;
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
      Client m_client;
    };
    //
    //    class MqttAdapterClientWs : public MqttAdapterClientBase<mqtt_client_ws>
    //    {
    //    public:
    //      using base = MqttAdapterClientBase<mqtt_client_ws>;
    //      using base::base;
    //    };
    //
    //    class MqttAdapterNoTlsClientWs : public MqttAdapterClientBase<mqtt_tls_client_ws>
    //    {
    //    public:
    //      using base = MqttAdapterClientBase<mqtt_tls_client_ws>;
    //      using base::base;
    //    };

    MqttAdapter::MqttAdapter(boost::asio::io_context &context, const ConfigOptions &options,
                             std::unique_ptr<MqttPipeline> &pipeline)
      : Source("MQTT", options),
        m_ioContext(context),
        m_host(*GetOption<std::string>(options, configuration::Host)),
        m_port(GetOption<int>(options, configuration::Port).value_or(1883)),
        m_pipeline(std::move(pipeline))
    {
      if (IsOptionSet(options, configuration::MqttTls))
        m_client = make_shared<MqttAdapterTlsClient>(m_ioContext, options, m_pipeline.get());
      else
        m_client = make_shared<MqttAdapterClient>(m_ioContext, options, m_pipeline.get());
      m_name = m_client->getIdentity();
      m_options[configuration::AdapterIdentity] = m_name;
      m_pipeline->build(m_options);

      m_identity = m_client->getIdentity();
      m_url = m_client->getUrl();
    }

    void MqttPipeline::build(const ConfigOptions &options)
    {
      clear();
      m_options = options;

      auto identity = GetOption<string>(options, configuration::AdapterIdentity);

      StringList devices;
      auto list = GetOption<StringList>(options, configuration::AdditionalDevices);
      if (list)
        devices = *list;
      auto device = GetOption<string>(options, configuration::Device);
      if (device)
      {
        devices.emplace_front(*device);
        auto dp = m_context->m_contract->findDevice(*device);
        if (dp)
        {
          dp->setOptions(options);
        }
      }
      TransformPtr next;

      bind(make_shared<DeliverConnectionStatus>(
          m_context, devices, IsOptionSet(options, configuration::AutoAvailable)));
      bind(make_shared<DeliverCommand>(m_context, device));

      next = bind(make_shared<TopicMapper>(
          m_context, GetOption<string>(m_options, configuration::Device).value_or("")));

      auto map1 = next->bind(make_shared<JsonMapper>(m_context));
      auto map2 = next->bind(make_shared<DataMapper>(m_context));

      next = make_shared<NullTransform>(TypeGuard<Observation, asset::Asset>(SKIP));

      map1->bind(next);
      map2->bind(next);

      // Go directly to asset delivery
      std::optional<string> assetMetrics;
      if (identity)
        assetMetrics = *identity + "_asset_update_rate";

      next->bind(make_shared<DeliverAsset>(m_context, assetMetrics));
      next->bind(make_shared<DeliverAssetCommand>(m_context));

      // Uppercase Events
      if (IsOptionSet(m_options, configuration::UpcaseDataItemValue))
        next = next->bind(make_shared<UpcaseValue>());

      // Filter dups, by delta, and by period
      next = next->bind(make_shared<DuplicateFilter>(m_context));
      next = next->bind(make_shared<DeltaFilter>(m_context));
      next = next->bind(make_shared<PeriodFilter>(m_context));

      // Convert values
      if (IsOptionSet(m_options, configuration::ConversionRequired))
        next = next->bind(make_shared<ConvertSample>());

      // Deliver
      std::optional<string> obsMetrics;
      if (identity)
        obsMetrics = *identity + "_observation_update_rate";
      next->bind(make_shared<DeliverObservation>(m_context, obsMetrics));
    }
  }  // namespace source
}  // namespace mtconnect
