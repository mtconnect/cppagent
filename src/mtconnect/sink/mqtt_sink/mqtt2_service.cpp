  //
  // Copyright Copyright 2009-2023, AMT – The Association For Manufacturing Technology (“AMT”)
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

  #include <nlohmann/json.hpp>

  #include "mqtt2_service.hpp"

  #include "mtconnect/configuration/config_options.hpp"
  #include "mtconnect/entity/entity.hpp"
  #include "mtconnect/entity/factory.hpp"
  #include "mtconnect/entity/json_parser.hpp"
  #include "mtconnect/mqtt/mqtt_client_impl.hpp"
  #include "mtconnect/printer/json_printer.hpp"

  using ptree = boost::property_tree::ptree;
  using json = nlohmann::json;

  using namespace std;
  using namespace mtconnect;
  using namespace mtconnect::asset;

  namespace asio = boost::asio;
  namespace config = ::mtconnect::configuration;

  namespace mtconnect {
    namespace sink {
      namespace mqtt_sink {
        // get obeservation in
        // create a json printer
        // call print


        Mqtt2Service::Mqtt2Service(boost::asio::io_context &context,
                                   sink::SinkContractPtr &&contract, const ConfigOptions &options,
                                   const ptree &config)
          : Sink("Mqtt2Service", std::move(contract)),
            m_context(context),
            m_strand(context),
            m_options(options),
            m_timer(context)
        {
          // Unique id number for agent instance
          m_instanceId = getCurrentTimeInSec();

          auto jsonPrinter =
              dynamic_cast<printer::JsonPrinter *>(m_sinkContract->getPrinter("json"));

          m_jsonPrinter = make_unique<entity::JsonEntityPrinter>(jsonPrinter->getJsonVersion());
          
          m_printer = std::make_unique<printer::JsonPrinter>(jsonPrinter->getJsonVersion());

          GetOptions(config, m_options, options);
          AddOptions(config, m_options,
                     {{configuration::MqttCaCert, string()},
                      {configuration::MqttPrivateKey, string()},
                      {configuration::MqttCert, string()},
                      {configuration::MqttClientId, string()},
                      {configuration::MqttUserName, string()},
                      {configuration::MqttPassword, string()}});
          AddDefaultedOptions(config, m_options,
                              {{configuration::MqttHost, "127.0.0.1"s},
                               {configuration::DeviceTopic, "MTConnect/Device/"s},
                               {configuration::AssetTopic, "MTConnect/Asset/"s},
                               {configuration::ObservationTopic, "MTConnect/Observation/"s},
                               {configuration::MqttPort, 1883},
                               {configuration::MqttTls, false}});

          createProbeRoutings();
          //createCurrentRoutings();
          //createSampleRoutings();

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

           int mqttFormatFlatto(GetOption<int>(options, configuration::MqttFormatFlat).value_or(7));

          std::string::difference_type nDeviceFormatFlat =
              std::count(m_devicePrefix.begin(), m_devicePrefix.end(), '/');
          std::string::difference_type nAssertFormatFlat =
              std::count(m_assetPrefix.begin(), m_assetPrefix.end(), '/');
          std::string::difference_type nObservationFormatFlat =
              std::count(m_observationPrefix.begin(), m_observationPrefix.end(), '/');

          if (nDeviceFormatFlat > mqttFormatFlatto || nAssertFormatFlat > mqttFormatFlatto ||
              nObservationFormatFlat > mqttFormatFlatto)
            LOG(warning) << "Mqtt Flat Format is Exceeds the limit allowed";

          if (IsOptionSet(m_options, configuration::MqttTls))
          {
            m_client = make_shared<MqttTlsClient>(m_context, m_options, std::move(clientHandler));
          }
          else
          {
            m_client = make_shared<MqttTcpClient>(m_context, m_options, std::move(clientHandler));
          }
        }

        void Mqtt2Service::start()
        {
          // mqtt client side not a server side...
          if (!m_client)
            return;

          m_client->start();
        }

        void Mqtt2Service::stop()
        {
          // stop client side
          if (m_client)
            m_client->stop();
        }

        std::shared_ptr<MqttClient> Mqtt2Service::getClient() { return m_client; }


        struct AsyncCurrentServiceResponse
        {
          AsyncCurrentServiceResponse(asio::io_context &context)
            : m_timer(context)
          {}

          std::weak_ptr<Sink> m_service;
          chrono::milliseconds m_interval;
          unique_ptr<JsonEntityPrinter> m_printer {nullptr};
          boost::asio::steady_timer m_timer;
          bool m_pretty {false};
        };

        void Mqtt2Service::createProbeRoutings()
        {
            // Probe
         //   auto handler = [&](SessionPtr session, const RequestPtr request) -> bool {
           // auto device = request->parameter<string>("device");
           // auto pretty = *request->parameter<bool>("pretty");

            /* auto printer = printerForAccepts(request->m_accepts);

             if (device && !ends_with(request->m_path, string("probe")) &&
                 m_sinkContract->findDeviceByUUIDorName(*device) == nullptr)
               return false;

             respond(session, probeRequest(printer, device, pretty));*/
            //return true;
          //};
        }

        bool Mqtt2Service::probeRequest(const Printer *printer,
                                        const std::optional<std::string> &device)
        {
          NAMED_SCOPE("Mqtt2Service::probeRequest");

          auto topic = "Topic: Devices";

          list<DevicePtr> deviceList;

          if (device)
          {
            auto dev = checkDevice(printer, *device);
            deviceList.emplace_back(dev);
          }
          else
          {
            deviceList = m_sinkContract->getDevices();
          }

          auto counts = m_sinkContract->getAssetStorage()->getCountsByType();

          auto doc =
              printer->printProbe(m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
                                  m_sinkContract->getCircularBuffer().getSequence(),
                                  uint32_t(m_sinkContract->getAssetStorage()->getMaxAssets()),
                                  uint32_t(m_sinkContract->getAssetStorage()->getCount()),
                                  deviceList, &counts, false, true);

          stringstream buffer;
          buffer << doc;

          if (m_client)
            m_client->publish(topic, buffer.str());

          return true;
        }

        DevicePtr Mqtt2Service::checkDevice(const Printer *printer, const std::string &uuid) const
        {
          auto dev = m_sinkContract->findDeviceByUUIDorName(uuid);
          if (!dev)
          {
            LOG(warning) << "Mqtt2Service::checkDevice Could not find the device '" + uuid + "'";
          }

          return dev;
        }

        void Mqtt2Service::probeCurrentRequest(const int interval,
                                               const std::optional<std::string> &device)
        {
          auto asyncResponse = make_shared<AsyncCurrentServiceResponse>(m_context);

         /* boost::asio::bind_executor(m_strand, [this, asyncResponse]() {
            streamNextCurrent(asyncResponse, boost::system::error_code {});
          });*/
        }


        bool Mqtt2Service::publish(observation::ObservationPtr &observation)
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

        bool Mqtt2Service::publish(device_model::DevicePtr device)
        {
          auto topic = m_devicePrefix + *device->getUuid();
          auto doc = m_jsonPrinter->print(device);

          stringstream buffer;
          buffer << doc;

          if (m_client)         
              m_client->publish(topic, buffer.str());

          return true;
        }


        bool Mqtt2Service::publish_Probe(device_model::DevicePtr device)
        {
          /*auto topic = m_devicePrefix + *device->getUuid();
          auto doc = m_jsonPrinter->print(device);

          stringstream buffer;
          buffer << doc;

          if (m_client)

            m_client->publish(topic, buffer.str());
*/
          return true;
        }

        bool Mqtt2Service::publish_Current(device_model::DevicePtr device)
        {
          auto topic = m_devicePrefix + *device->getUuid();
          auto doc = m_jsonPrinter->print(device);

          stringstream buffer;
          buffer << doc;

          if (m_client)

            m_client->publish(topic, buffer.str());

          return true;
        }

        bool Mqtt2Service::publish_Samples(device_model::DevicePtr device)
        {
          auto topic = m_devicePrefix + *device->getUuid();
          auto doc = m_jsonPrinter->print(device);

          stringstream buffer;
          buffer << doc;

          if (m_client)

            m_client->publish(topic, buffer.str());

          return true;
        }


        bool Mqtt2Service::publish(asset::AssetPtr asset)
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
        void Mqtt2Service::registerFactory(SinkFactory &factory)
        {
          factory.registerFactory(
              "Mqtt2Service",
              [](const std::string &name, boost::asio::io_context &io, SinkContractPtr &&contract,
                 const ConfigOptions &options, const boost::property_tree::ptree &block) -> SinkPtr {
                auto sink = std::make_shared<Mqtt2Service>(io, std::move(contract), options, block);
                return sink;
              });
        }
      }  // namespace mqtt_sink
    }    // namespace sink
  }  // namespace mtconnect
