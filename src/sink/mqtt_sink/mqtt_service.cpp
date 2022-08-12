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

#include "mqtt_service.hpp"

#include "configuration/config_options.hpp"
#include "entity/entity.hpp"
#include "entity/factory.hpp"
#include "entity/json_parser.hpp"
#include "mqtt/mqtt_client_impl.hpp"
#include "printer/json_printer.hpp"

using json = nlohmann::json;
using ptree = boost::property_tree::ptree;

using namespace std;
using namespace mtconnect::asset;

namespace asio = boost::asio;
namespace config = ::mtconnect::configuration;

namespace mtconnect {
  namespace sink {
    namespace mqtt_sink {
      // get obeservation in
      // create a json printer
      // call print

      MqttService::MqttService(boost::asio::io_context &context, sink::SinkContractPtr &&contract,
                               const ConfigOptions &options, const ptree &config)
        : Sink("MqttService", move(contract)), m_context(context), m_options(options)
      {
        auto jsonPrinter = dynamic_cast<printer::JsonPrinter*>(m_sinkContract->getPrinter("json"));
        m_jsonPrinter = make_unique<entity::JsonPrinter>(jsonPrinter->getJsonVersion());

        GetOptions(config, m_options, options);
        AddOptions(config, m_options,
                   {{configuration::UUID, string()},
                    {configuration::Manufacturer, string()},
                    {configuration::Station, string()},
                    {configuration::Url, string()},
                    {configuration::MqttCaCert, string()}});
        AddDefaultedOptions(config, m_options,
                            {{configuration::MqttHost, "127.0.0.1"s},
                             {configuration::DeviceTopic, "MTConnect/Device/"s},
                             {configuration::AssetTopic, "MTConnect/Asset/"s},
                             {configuration::ObservationTopic, "MTConnect/Observation/"s},
                             {configuration::MqttPort, 1883},
                             {configuration::MqttTls, false},
                             {configuration::AutoAvailable, false},
                             {configuration::RealTime, false},
                             {configuration::RelativeTime, false}});
        
        auto clientHandler = make_unique<ClientHandler>();
        clientHandler->m_connected = [this](shared_ptr<MqttClient> client) {
          // Publish latest devices, assets, and observations
          for (auto &dev : m_sinkContract->getDevices())
          {
            publish(dev);
          }

          for (auto &obs : m_latest.getObservations())
          {
            pub(obs.second);
          }
          
          AssetList list;
          m_sinkContract->getAssetStorage()->getAssets(list, 100000);
          for (auto &asset : list)
          {
            publish(asset);
          }
        };

        m_devicePrefix = get<string>(m_options[configuration::DeviceTopic]);
        m_assetPrefix = get<string>(m_options[configuration::AssetTopic]);
        m_observationPrefix = get<string>(m_options[configuration::ObservationTopic]);

        if (IsOptionSet(m_options, configuration::MqttTls))
        {
          m_client = make_shared<MqttTlsClient>(m_context, m_options, move(clientHandler));
        }
        else
        {
          m_client = make_shared<MqttTcpClient>(m_context, m_options, move(clientHandler));
        }
      }

      void MqttService::start()
      {
        // mqtt client side not a server side...
        if (!m_client)
          return;

        m_client->start();
      }

      void MqttService::stop()
      {
        // stop client side
        if (m_client)
          m_client->stop();
      }

      std::shared_ptr<MqttClient> MqttService::getClient() { return m_client; }
      
      void MqttService::pub(const observation::ObservationPtr &observation)
      {
        // get the data item from observation
        DataItemPtr dataItem = observation->getDataItem();

        auto topic = m_observationPrefix + dataItem->getTopic();        // client asyn topic
        auto content = dataItem->getTopicName();  // client asyn content

        // We may want to use the observation from the checkpoint.
        auto jsonDoc = m_jsonPrinter->printEntity(observation);
        
        stringstream buffer;
        buffer << jsonDoc;

        if (m_client)
            m_client->publish(topic, buffer.str());
      }


      uint64_t MqttService::publish(observation::ObservationPtr &observation)
      {
        // add obsrvations to the checkpoint. We may want to publish the checkpoint
        // observation vs the delta. The complete observation can be obtained by getting
        // the observation from the checkpoint.
        m_latest.addObservation(observation);
        pub(observation);
        
        return 0;
      }
      
      bool MqttService::publish(device_model::DevicePtr device)
      {
        
        auto topic = m_devicePrefix + *device->getUuid();
        auto doc = m_jsonPrinter->print(device);

        stringstream buffer;
        buffer << doc;

        if (m_client)
            m_client->publish(topic, buffer.str());

        return true;
      }


      bool MqttService::publish(asset::AssetPtr asset)
      {
        auto topic = m_assetPrefix + get<string>(asset->getIdentity());
        auto doc = m_jsonPrinter->print(asset);

        stringstream buffer;
        buffer << doc;

        if (m_client)
            m_client->publish(topic, buffer.str());

        return true;
      }

      // Register the service with the sink factory
      void MqttService::registerFactory(SinkFactory &factory)
      {
        factory.registerFactory(
            "MqttService",
            [](const std::string &name, boost::asio::io_context &io, SinkContractPtr &&contract,
               const ConfigOptions &options, const boost::property_tree::ptree &block) -> SinkPtr {
              auto sink = std::make_shared<MqttService>(io, std::move(contract), options, block);
              return sink;
            });
      }
    }  // namespace mqtt_sink
  }    // namespace sink
}  // namespace mtconnect
