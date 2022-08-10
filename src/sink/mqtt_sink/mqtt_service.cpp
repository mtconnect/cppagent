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
#include "entity/xml_parser.hpp"
#include "mqtt/mqtt_client_impl.hpp"
#include "printer/xml_printer.hpp"

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
        auto jsonPrinter = dynamic_cast<JsonPrinter *>(m_sinkContract->getPrinter("json"));
        m_jsonPrinter = std::unique_ptr<JsonPrinter>(jsonPrinter);

        GetOptions(config, m_options, options);
        AddOptions(config, m_options,
                   {{configuration::UUID, string()},
                    {configuration::Manufacturer, string()},
                    {configuration::Station, string()},
                    {configuration::Url, string()},
                    {configuration::MqttCaCert, string()}});
        AddDefaultedOptions(config, m_options,
                            {{configuration::Host, "localhost"s},
                             {configuration::Port, 1883},
                             {configuration::MqttTls, false},
                             {configuration::AutoAvailable, false},
                             {configuration::RealTime, false},
                             {configuration::RelativeTime, false}});

        loadTopics(config, m_options);
        
        auto clientHandler = make_unique<ClientHandler>();


        if (IsOptionSet(m_options, configuration::MqttTls))
        {
          m_client = make_shared<MqttTlsClient>(m_context, m_options, move(clientHandler));
        }
        else
        {
          m_client = make_shared<MqttTcpClient>(m_context, m_options, move(clientHandler));
        }
      }

      void MqttService::loadTopics(const boost::property_tree::ptree &tree, ConfigOptions &options)
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

      void MqttService::start()
      {
        // mqtt client side not a server side...
        m_client->start();

        for (const auto &dev : m_sinkContract->getDevices())
        {
          publish(dev);
        }
      }

      void MqttService::stop()
      {
        // stop client side
        m_client->stop();
      }

      std::shared_ptr<MqttClient> MqttService::getClient() { return m_client; }

      uint64_t MqttService::publish(observation::ObservationPtr &observation)
      {
        // get the data item from observation
        DataItemPtr dataItem = observation->getDataItem();

        auto topic = dataItem->getTopic();        // client asyn topic
        auto content = dataItem->getTopicName();  // client asyn content

        auto jsonDoc = m_jsonPrinter->printEntity(observation);
        
        stringstream buffer;
        buffer << jsonDoc;

        m_client->publish(topic, buffer.str());
        
        return 0;
      }
      
      bool MqttService::publish(device_model::DevicePtr device)
      {
        
        return true;
      }


      bool MqttService::publish(asset::AssetPtr asset)
      {
        return false;
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
