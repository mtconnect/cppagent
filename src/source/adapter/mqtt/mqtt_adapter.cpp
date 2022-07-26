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

#include "configuration/config_options.hpp"
#include "device_model/device.hpp"
#include "mqtt/mqtt_client_impl.hpp"
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
                  {configuration::MqttCaCert, string()}});

      AddDefaultedOptions(block, m_options,
                          {{configuration::Host, "localhost"s},
                           {configuration::Port, 1883},
                           {configuration::MqttTls, false},
                           {configuration::AutoAvailable, false},
                           {configuration::RealTime, false},
                           {configuration::RelativeTime, false}});
      loadTopics(block, m_options);

      m_handler = m_pipeline.makeHandler();
      if (IsOptionSet(m_options, configuration::MqttTls))
        m_client = make_shared<mtconnect::mqtt_client::MqttTlsClient>(m_ioContext, m_options,
                                                                      &m_pipeline, m_handler.get());
      else
        m_client = make_shared<mtconnect::mqtt_client::MqttTcpClient>(m_ioContext, m_options,
                                                                      &m_pipeline, m_handler.get());

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
          list.emplace_back(":" + topics->get_value<string>());
        }
        else
        {
          for (auto &f : *topics)
          {
            list.emplace_back(f.first + ":" + f.second.data());
          }
        }
        options[configuration::Topics] = list;
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

    mtconnect::pipeline::Pipeline *MqttAdapter::getPipeline() { return &m_pipeline; }

    void MqttPipeline::build(const ConfigOptions &options)
    {
      AdapterPipeline::build(options);

      buildDeviceList();
      buildCommandAndStatusDelivery();

      auto next = bind(make_shared<TopicMapper>(
          m_context, GetOption<string>(m_options, configuration::Device).value_or("")));

      auto map1 = next->bind(make_shared<JsonMapper>(m_context));
      auto map2 = next->bind(make_shared<DataMapper>(m_context));

      next = make_shared<NullTransform>(TypeGuard<Observation, asset::Asset>(SKIP));

      map1->bind(next);
      map2->bind(next);

      buildAssetDelivery(next);
      buildObservationDelivery(next);
      applySplices();
    }

  }  // namespace source::adapter::mqtt_adapter
}  // namespace mtconnect
