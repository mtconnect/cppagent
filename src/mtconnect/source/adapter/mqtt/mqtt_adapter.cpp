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

#include "mqtt_adapter.hpp"

#include <boost/log/trivial.hpp>
#include <boost/uuid/name_generator_sha1.hpp>

#include <mqtt/async_client.hpp>
#include <mqtt/setup_log.hpp>

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/mqtt/mqtt_client_impl.hpp"
#include "mtconnect/pipeline/convert_sample.hpp"
#include "mtconnect/pipeline/deliver.hpp"
#include "mtconnect/pipeline/delta_filter.hpp"
#include "mtconnect/pipeline/duplicate_filter.hpp"
#include "mtconnect/pipeline/message_mapper.hpp"
#include "mtconnect/pipeline/period_filter.hpp"
#include "mtconnect/pipeline/shdr_token_mapper.hpp"
#include "mtconnect/pipeline/shdr_tokenizer.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "mtconnect/pipeline/topic_mapper.hpp"
#include "mtconnect/pipeline/upcase_value.hpp"

using namespace std;
namespace asio = boost::asio;

namespace mtconnect {
  using namespace observation;
  using namespace entity;
  using namespace pipeline;
  using namespace source::adapter;

  namespace source::adapter::mqtt_adapter {

    MqttAdapter::MqttAdapter(boost::asio::io_context &io,
                             pipeline::PipelineContextPtr pipelineContext,
                             const ConfigOptions &options, const boost::property_tree::ptree &block)
      : Adapter("MQTT", io, options),
        m_ioContext(io),
        m_strand(Source::m_strand),
        m_pipeline(pipelineContext, Source::m_strand)
    {
      GetOptions(block, m_options, options);
      AddOptions(block, m_options,
                 {{configuration::UUID, string()},
                  {configuration::Manufacturer, string()},
                  {configuration::Station, string()},
                  {configuration::Url, string()},
                  {configuration::MqttCaCert, string()},
                  {configuration::MqttPrivateKey, string()},
                  {configuration::MqttCert, string()},
                  {configuration::MqttUserName, string()},
                  {configuration::MqttPassword, string()},
                  {configuration::MqttClientId, string()},
                  {configuration::MqttHost, string()}});

      AddDefaultedOptions(block, m_options,
                          {{configuration::MqttPort, 1883},
                           {configuration::MqttTls, false},
                           {configuration::AutoAvailable, false},
                           {configuration::RealTime, false},
                           {configuration::RelativeTime, false}});
      loadTopics(block, m_options);

      if (!HasOption(m_options, configuration::MqttHost) &&
          HasOption(m_options, configuration::Host))
      {
        m_options[configuration::MqttHost] = m_options[configuration::Host];
      }

      m_handler = m_pipeline.makeHandler();
      auto clientHandler = make_unique<ClientHandler>();

      m_pipeline.m_handler = m_handler.get();

      clientHandler->m_connecting = [this](shared_ptr<MqttClient> client) {
        m_handler->m_connecting(client->getIdentity());
      };

      clientHandler->m_connected = [this](shared_ptr<MqttClient> client) {
        client->connectComplete();
        m_handler->m_connected(client->getIdentity());
        subscribeToTopics();
      };

      clientHandler->m_disconnected = [this](shared_ptr<MqttClient> client) {
        m_handler->m_disconnected(client->getIdentity());
      };

      clientHandler->m_receive = [this](shared_ptr<MqttClient> client, const std::string &topic,
                                        const std::string &payload) {
        m_handler->m_processMessage(topic, payload, client->getIdentity());
      };

      if (IsOptionSet(m_options, configuration::MqttTls))
        m_client = make_shared<mtconnect::mqtt_client::MqttTlsClient>(m_ioContext, m_options,
                                                                      std::move(clientHandler));
      else
        m_client = make_shared<mtconnect::mqtt_client::MqttTcpClient>(m_ioContext, m_options,
                                                                      std::move(clientHandler));

      m_identity = m_client->getIdentity();
      m_name = m_client->getUrl();

      m_options[configuration::AdapterIdentity] = m_name;
      m_pipeline.build(m_options);
    }

    void MqttAdapter::loadTopics(const boost::property_tree::ptree &tree, ConfigOptions &options)
    {
      auto topics = tree.get_child_optional(configuration::Topics);
      if (topics)
      {
        StringList list;
        if (topics->size() == 0)
        {
          boost::split(list, topics->get_value<string>(), boost::is_any_of(":"),
                       boost::token_compress_on);
        }
        else
        {
          for (auto &f : *topics)
          {
            list.emplace_back(f.second.data());
          }
        }
        options[configuration::Topics] = list;
      }
      else
      {
        LOG(error) << "MQTT Adapter requires at least one topic to subscribe to. Provide 'Topics = "
                      "' or Topics block";
        exit(1);
      }
    }
    /// <summary>
    ///
    /// </summary>
    /// <param name="factory"></param>
    void MqttAdapter::registerFactory(SourceFactory &factory)
    {
      factory.registerFactory("mqtt",
                              [](const std::string &name, boost::asio::io_context &io,
                                 pipeline::PipelineContextPtr context, const ConfigOptions &options,
                                 const boost::property_tree::ptree &block) -> source::SourcePtr {
                                auto source =
                                    std::make_shared<MqttAdapter>(io, context, options, block);
                                return source;
                              });
    }

    const std::string &MqttAdapter::getHost() const { return m_host; }

    unsigned int MqttAdapter::getPort() const { return m_port; }

    bool MqttAdapter::start()
    {
      m_pipeline.start();
      return m_client->start();
    }
    void MqttAdapter::stop()
    {
      m_client->stop();
      m_pipeline.clear();
    }

    void MqttAdapter::subscribeToTopics()
    {
      // If we have topics, subscribe
      auto topics = GetOption<StringList>(m_options, configuration::Topics);
      LOG(info) << "MqttClientImpl::connect: subscribing to topics";
      if (topics)
      {
        for (const auto &topic : *topics)
        {
          m_client->subscribe(topic);
        }
      }
    }

    mtconnect::pipeline::Pipeline *MqttAdapter::getPipeline() { return &m_pipeline; }

    void MqttPipeline::build(const ConfigOptions &options)
    {
      AdapterPipeline::build(options);

      buildDeviceList();
      buildCommandAndStatusDelivery();

      // SHDR Parsing Branch, if Data is sent down...
      auto tokenizer = bind(make_shared<ShdrTokenizer>());
      auto shdr = tokenizer;

      auto extract =
          make_shared<ExtractTimestamp>(IsOptionSet(m_options, configuration::RelativeTime));
      shdr = shdr->bind(extract);

      // Token mapping to data items and asset
      auto mapper = make_shared<ShdrTokenMapper>(
          m_context, m_device.value_or(""),
          GetOption<int>(m_options, configuration::ShdrVersion).value_or(1));

      buildAssetDelivery(mapper);
      mapper->bind(make_shared<NullTransform>(TypeGuard<Observations>(RUN)));
      shdr = shdr->bind(mapper);

      // Build topic mapper pipeline
      auto next = bind(make_shared<TopicMapper>(
          m_context, GetOption<string>(m_options, configuration::Device).value_or("")));

      auto map1 = next->bind(make_shared<JsonMapper>(m_context));
      auto map2 = next->bind(make_shared<DataMapper>(m_context, m_handler));
      map2->bind(tokenizer);

      next = make_shared<NullTransform>(TypeGuard<Observation, asset::Asset>(SKIP));

      map1->bind(next);
      map2->bind(next);

      // Merge the pipelines
      shdr->bind(next);

      buildObservationDelivery(next);
      applySplices();
    }

  }  // namespace source::adapter::mqtt_adapter
}  // namespace mtconnect
