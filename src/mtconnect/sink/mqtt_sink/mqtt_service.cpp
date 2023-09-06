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

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/factory.hpp"
#include "mtconnect/entity/json_parser.hpp"
#include "mtconnect/mqtt/mqtt_client_impl.hpp"
#include "mtconnect/printer/json_printer.hpp"

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
        : Sink("MqttService", std::move(contract)), m_context(context), m_options(options)
      {
        auto jsonPrinter = dynamic_cast<printer::JsonPrinter *>(m_sinkContract->getPrinter("json"));
        m_jsonPrinter = make_unique<entity::JsonEntityPrinter>(jsonPrinter->getJsonVersion());

        GetOptions(config, m_options, options);
        AddOptions(config, m_options,
                   {{configuration::MqttCaCert, string()},
                    {configuration::MqttPrivateKey, string()},
                    {configuration::MqttCert, string()},
                    {configuration::MqttUserName, string()},
                    {configuration::MqttPassword, string()},
                    {configuration::MqttClientId, string()}});
        AddDefaultedOptions(config, m_options,
                            {{configuration::MqttHost, "127.0.0.1"s},
                             {configuration::DeviceTopic, "MTConnect/Device/"s},
                             {configuration::AssetTopic, "MTConnect/Asset/"s},
                             {configuration::ObservationTopic, "MTConnect/Observation/"s},
                             {configuration::MqttPort, 1883},
                             {configuration::MqttTls, false}});

        auto clientHandler = make_unique<ClientHandler>();
        clientHandler->m_connected = [this](shared_ptr<MqttClient> client) {
          // Publish latest devices, assets, and observations
          auto &circ = m_sinkContract->getCircularBuffer();
          std::lock_guard<buffer::CircularBuffer> lock(circ);
          client->connectComplete();

          for (auto &dev : m_sinkContract->getDevices())
          {
            publish(dev);
          }

          auto obsList {circ.getLatest().getObservations()};
          for (auto &obs : obsList)
          {
            observation::ObservationPtr p {obs.second};
            publish(p);
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
          m_client = make_shared<MqttTlsClient>(m_context, m_options, std::move(clientHandler));
        }
        else
        {
          m_client = make_shared<MqttTcpClient>(m_context, m_options, std::move(clientHandler));
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

      bool MqttService::publish(observation::ObservationPtr &observation)
      {
        // get the data item from observation
        if (observation->isOrphan())
          return false;

        DataItemPtr dataItem = observation->getDataItem();

        auto topic = m_observationPrefix + dataItem->getTopic();  // client asyn topic
        auto content = dataItem->getTopicName();                  // client asyn content

        // We may want to use the observation from the checkpoint.
        auto doc = m_jsonPrinter->printEntity(observation);

        if (m_client)
          m_client->publish(topic, doc);

        return true;
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
