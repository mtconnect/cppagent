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

#include <boost/uuid/name_generator_sha1.hpp>
#include <mqtt/async_client.hpp>
#include <mqtt/log.hpp>

#include "mqtt_adapter.hpp"
#include "configuration/config_options.hpp"
#include "pipeline/convert_sample.hpp"
#include "pipeline/deliver.hpp"
#include "pipeline/delta_filter.hpp"
#include "pipeline/duplicate_filter.hpp"
#include "pipeline/period_filter.hpp"
#include "pipeline/timestamp_extractor.hpp"
#include "pipeline/shdr_token_mapper.hpp"
#include "pipeline/shdr_tokenizer.hpp"
#include "pipeline/upcase_value.hpp"
#include "pipeline/topic_mapper.hpp"
#include "pipeline/message_mapper.hpp"
#include "device_model/device.hpp"

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
    template<typename... Ts>
    using mqtt_client_ptr =  decltype(mqtt::make_async_client(std::declval<Ts>()...));
    template<typename... Ts>
    using mqtt_tls_client_ptr =  decltype(mqtt::make_tls_async_client(std::declval<Ts>()...));
    using mqtt_client = mqtt_client_ptr<boost::asio::io_context&, std::string, std::uint16_t, mqtt::protocol_version>;
    using mqtt_tls_client = mqtt_tls_client_ptr<boost::asio::io_context&, std::string, std::uint16_t, mqtt::protocol_version>;

    template<class Client>
    class MqttAdapterClient : public MqttAdapterImpl
    {
    public:
      MqttAdapterClient(boost::asio::io_context &ioContext,
                      const ConfigOptions &options,
                      MqttPipeline *pipeline) :
      MqttAdapterImpl(ioContext), m_options(options),
      m_host(*GetOption<std::string>(options, configuration::Host)),
      m_port(GetOption<int>(options, configuration::Port).value_or(1883)),
      m_pipeline(pipeline), m_reconnectTimer(ioContext)
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
      
      ~MqttAdapterClient()
      {
        stop();
      }
                  
      bool start() override
      {
        NAMED_SCOPE("MqttAdapterClient::start");
        
        m_client = mqtt::make_async_client(m_ioContext, m_host, m_port);
        m_client->set_client_id(m_identity);
        m_client->clean_session();
        m_client->set_keep_alive_sec(10);
        
        m_client->set_connack_handler([this](bool sp, mqtt::connect_return_code connack_return_code) {
          if (connack_return_code == mqtt::connect_return_code::accepted) {
            auto entity = make_shared<Entity>("ConnectionStatus",
                                              Properties {{"VALUE", "CONNECTED"s}, {"source", m_identity}});
            m_pipeline->run(entity);

            subscribe();
          }
          else
          {
            reconnect();
          }
          return true;
        });
        
        m_client->set_close_handler([this]() {
          LOG(info) << "MQTT " << m_url << ": connection closed";
          // Queue on a strand
          auto entity = make_shared<Entity>("ConnectionStatus",
                                            Properties {{"VALUE", "DISCONNECTED"s}, {"source", m_identity}});
          m_pipeline->run(entity);
          reconnect();
        });
        
        m_client->set_suback_handler([this](std::uint16_t packet_id, std::vector<mqtt::suback_return_code> results){
          LOG(debug) << "suback received. packet_id: " << packet_id;
          for (auto const& e : results)
          {
            LOG(debug) << "subscribe result: " << e;
            // Do something if the subscription failed...
          }

          return true;
        });
        
        m_client->set_error_handler([this](mqtt::error_code ec){
          LOG(error) << "error: " << ec.message();
          reconnect();
        });

                
        m_client->set_publish_handler([this](mqtt::optional<std::uint16_t> packet_id,
                                              mqtt::publish_options pubopts,
                                              mqtt::buffer topic_name,
                                              mqtt::buffer contents) {
          if (packet_id)
            LOG(debug)  << "packet_id: " << *packet_id;
          LOG(debug) << "topic_name: " << topic_name;
          LOG(debug) << "contents: " << contents;
          
          receive(topic_name, contents);

          return true;
        });
        
        connect();
        
        return true;
      }

      void stop() override
      {
        m_client.reset();
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
              topicList.emplace_back(make_tuple(
                topic.substr(loc + 1),
                mqtt::qos::at_least_once));
            }
          }
        }
        else
        {
          LOG(warning) << "No topics specified, subscribing to '#'";
          topicList.emplace_back(make_tuple("#"s, mqtt::qos::at_least_once));
        }
        
        m_subPid = m_client->acquire_unique_packet_id();
        m_client->async_subscribe(m_subPid, topicList,
                                             [](mqtt::error_code ec) {
          if (ec)
          {
            LOG(error) << "Subscribe failed: " << ec.message();
          }
        });
      }
      
      void connect()
      {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties {{"VALUE", "CONNECTING"s}, {"source", m_identity}});
        m_pipeline->run(entity);
        
        m_client->async_connect();
      }
      
      void receive(mqtt::buffer &topic, mqtt::buffer &contents)
      {
        auto entity = make_shared<pipeline::Message>("Topic", Properties {{"VALUE", string(contents)}, {"topic", string(topic)}, {"source", m_identity}});
        m_pipeline->run(entity);
      }
      
      void reconnect()
      {
        NAMED_SCOPE("MqttAdapterClient::reconnect");
        
        LOG(info) << "Start reconnect timer";

        // Set an expiry time relative to now.
        m_reconnectTimer.expires_after(std::chrono::seconds(5));

        m_reconnectTimer.async_wait([this](const boost::system::error_code& error) {
            if (error != boost::asio::error::operation_aborted) {
              LOG(info) << "Reconnect now !!";

                // Connect
                m_client->async_connect(
                    [this](mqtt::error_code ec){
                        LOG(info)  << "async_connect callback: " << ec.message();
                        if (ec && ec != boost::asio::error::operation_aborted) {
                            reconnect();
                        }
                    }
                );
            }
        });
      }

    protected:
      ConfigOptions m_options;
      
      std::string m_host;
      unsigned int m_port;
      
      std::uint16_t m_subPid{0};
      bool m_running;
      
      MqttPipeline *m_pipeline;

      Client m_client;
      boost::asio::steady_timer m_reconnectTimer;
    };

    MqttAdapter::MqttAdapter(boost::asio::io_context &context,
                             const ConfigOptions &options,
                             std::unique_ptr<MqttPipeline> &pipeline)
    : Source("MQTT", options), m_ioContext(context),
    m_host(*GetOption<std::string>(options, configuration::Host)),
    m_port(GetOption<int>(options, configuration::Port).value_or(1883)),
    m_pipeline(std::move(pipeline))
    {
      m_client = make_shared<MqttAdapterClient<mqtt_client>>(m_ioContext, options, m_pipeline.get());
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
      
      next = bind(make_shared<TopicMapper>(m_context, GetOption<string>(m_options, configuration::Device).value_or("")));

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
  }
}
      
      
