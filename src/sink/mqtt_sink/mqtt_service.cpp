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

#include "mqtt/mqtt_client_impl.hpp"
#include "configuration/config_options.hpp"
#include "entity/entity.hpp"
#include "entity/factory.hpp"
#include "entity/xml_parser.hpp"
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

        if (IsOptionSet(m_options, configuration::MqttTls))
        {
          m_client = make_shared<MqttTlsClient>(m_context, m_options);
        }
        else
        {
          m_client = make_shared<MqttTcpClient>(m_context, m_options);
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

        entity::EntityPtr dataEntity =
            make_shared<entity::Entity>("LIST");  // dataItem->getTopicList();

        auto jsonDoc = m_jsonPrinter->print(dataEntity);

        return 0;
      }

      std::string MqttService::printAssets(const unsigned int instanceId,
                                           const unsigned int bufferSize,
                                           const unsigned int assetCount,
                                           const asset::AssetList &asset) const
      {
        /*  json doc = json::object(
              {{"MTConnectAssets",
                {{"Header", probeAssetHeader(m_version, hostname(), instanceId, 0, bufferSize,
                                             assetCount, m_schemaVersion, m_modelChangeTime)},
                 {"Assets", assetDoc}}}});
          return print(doc);*/

        return "";
      }

      std::string MqttService::printDeviceStreams(const unsigned int instanceId,
                                                  const unsigned int bufferSize,
                                                  const uint64_t nextSeq, const uint64_t firstSeq,
                                                  const uint64_t lastSeq,
                                                  ObservationList &observations) const
      {
        /*json streams = json::array();
        json streams;

        if (observations.size() > 0)
        {
          observations.sort(ObservationCompare);
          vector<DeviceRef> devices;
          DeviceRef *deviceRef = nullptr;
          for (auto &observation : observations)
          {
            const auto dataItem = observation->getDataItem();
            const auto component = dataItem->getComponent();
            const auto device = component->getDevice();

            if (deviceRef == nullptr || !deviceRef->isDevice(device))
              if (!deviceRef || !deviceRef->isDevice(device))
              {
                devices.emplace_back(device);
                devices.emplace_back(device, m_jsonVersion);
                deviceRef = &devices.back();
              }

            deviceRef->addObservation(observation, device, component, dataItem);
          }

          streams = json::array();
          for (auto &ref : devices)
            streams.emplace_back(ref.toJson());

          if (m_jsonVersion == 2)
          {
            if (devices.size() == 1)
            {
              streams = json::object({{"DeviceStream", streams.front()}});
            }
            else
            {
              streams = json::object({{"DeviceStream", streams}});
            }
          }
        }*/
        return "";
      }

      std::string MqttService::printProbe(const unsigned int instanceId,
                                          const unsigned int bufferSize, const uint64_t nextSeq,
                                          const unsigned int assetBufferSize,
                                          const unsigned int assetCount,
                                          const std::list<DevicePtr> &devices,
                                          const std::map<std::string, size_t> *count) const
      {
        // entity::JsonPrinter printer;
        // entity::JsonPrinter printer(m_jsonVersion);

        // json devicesDoc = json::array();
        // for (const auto &device : devices)
        //  devicesDoc.emplace_back(printer.print(device));
        // json devicesDoc;

        // if (m_jsonVersion == 1)
        //{
        //  devicesDoc = json::array();
        //  for (const auto &device : devices)
        //    devicesDoc.emplace_back(printer.print(device));
        //}
        // else if (m_jsonVersion == 2)
        //{
        //  entity::EntityList list;
        //  copy(devices.begin(), devices.end(), back_inserter(list));
        //  entity::EntityPtr entity = make_shared<entity::Entity>("LIST");
        //  entity->setProperty("LIST", list);
        //  devicesDoc = printer.printEntity(entity);
        //}
        // else
        //{
        //  throw runtime_error("invalid json printer version");
        //}

        // json doc =
        //    json::object({{"MTConnectDevices",
        //                   {{"Header", probeAssetHeader(m_version, hostname(), instanceId,
        //                                                bufferSize, assetBufferSize, assetCount,
        //                                                m_schemaVersion, m_modelChangeTime)},
        //                    {"Devices", devicesDoc}}}});
        // return print(doc);
        return "";
      }

      bool MqttService::publish(asset::AssetPtr asset) { return false; }

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
