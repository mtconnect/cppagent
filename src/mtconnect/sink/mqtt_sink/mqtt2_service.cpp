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

#include "mqtt2_service.hpp"

#include <nlohmann/json.hpp>

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

      Mqtt2Service::Mqtt2Service(boost::asio::io_context &context, sink::SinkContractPtr &&contract,
                                 const ConfigOptions &options, const ptree &config)
        : Sink("Mqtt2Service", std::move(contract)),
          m_context(context),
          m_strand(context),
          m_options(options),
          m_currentTimer(context)
      {
        // Unique id number for agent instance
        m_instanceId = getCurrentTimeInSec();

        auto jsonPrinter = dynamic_cast<printer::JsonPrinter *>(m_sinkContract->getPrinter("json"));

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
                             {configuration::CurrentTopic, "MTConnect/Current/"s},
                             {configuration::SampleTopic, "MTConnect/Sample/"s},
                             {configuration::MqttCurrentInterval, 5000ms},
                             {configuration::MqttSampleInterval, 500ms},
                             {configuration::MqttSampleCount, 1000},
                             {configuration::MqttPort, 1883},
                             {configuration::MqttTls, false}});

        auto clientHandler = make_unique<ClientHandler>();
        clientHandler->m_connected = [this](shared_ptr<MqttClient> client) {
          // Publish latest devices, assets, and observations
          auto &circ = m_sinkContract->getCircularBuffer();
          std::lock_guard<buffer::CircularBuffer> lock(circ);
          client->connectComplete();

          pubishInitialContent();
        };

        m_devicePrefix = get<string>(m_options[configuration::DeviceTopic]);
        m_assetPrefix = get<string>(m_options[configuration::AssetTopic]);
        m_currentPrefix = get<string>(m_options[configuration::CurrentTopic]);
        m_samplePrefix = get<string>(m_options[configuration::SampleTopic]);

        m_currentInterval = *GetOption<Milliseconds>(m_options, configuration::MqttCurrentInterval);
        m_sampleInterval = *GetOption<Milliseconds>(m_options, configuration::MqttSampleInterval);

        m_sampleCount = *GetOption<int>(m_options, configuration::MqttSampleCount);

        int mqttFormatFlatto(GetOption<int>(options, configuration::MqttFormatFlat).value_or(7));

        auto nDeviceFormatFlat = std::count(m_devicePrefix.begin(), m_devicePrefix.end(), '/');
        auto nAssertFormatFlat = std::count(m_assetPrefix.begin(), m_assetPrefix.end(), '/');
        auto currentTopicDepth = std::count(m_currentPrefix.begin(), m_currentPrefix.end(), '/');
        auto sampleTopicDepth = std::count(m_samplePrefix.begin(), m_samplePrefix.end(), '/');

        if (nDeviceFormatFlat > mqttFormatFlatto || nAssertFormatFlat > mqttFormatFlatto ||
            currentTopicDepth > mqttFormatFlatto || sampleTopicDepth > mqttFormatFlatto)
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

        m_currentTimer.cancel();
      }

      struct AsyncSample : public observation::AsyncObserver
      {
        AsyncSample(boost::asio::io_context::strand &strand,
                    mtconnect::buffer::CircularBuffer &buffer, FilterSet &&filter,
                    std::chrono::milliseconds interval, std::chrono::milliseconds heartbeat,
                    std::shared_ptr<MqttClient> client, DevicePtr device)
          : observation::AsyncObserver(strand, buffer, std::move(filter), interval, heartbeat),
            m_device(device),
            m_client(client)
        {}

        void fail(boost::beast::http::status status, const std::string &message) override
        {
          LOG(error) << "MQTT Sample Failed: " << message;
        }

        bool isRunning() override { return m_client->isRunning(); }

        DevicePtr m_device;
        std::shared_ptr<MqttClient> m_client;
      };

      void Mqtt2Service::pubishInitialContent()
      {
        using std::placeholders::_1;
        for (auto &dev : m_sinkContract->getDevices())
        {
          publish(dev);
        }

        auto seq = m_sinkContract->getCircularBuffer().getSequence();
        for (auto &dev : m_sinkContract->getDevices())
        {
          FilterSet filterSet = filterForDevice(dev);
          auto sampler =
              make_shared<AsyncSample>(m_strand, m_sinkContract->getCircularBuffer(),
                                       std::move(filterSet), m_sampleInterval, 600s, m_client, dev);
          sampler->m_handler = boost::bind(&Mqtt2Service::publishSample, this, _1);
          sampler->observe(seq, [this](const std::string &id) {
            return m_sinkContract->getDataItemById(id).get();
          });
          sampler->handlerCompleted();
        }

        AssetList list;
        m_sinkContract->getAssetStorage()->getAssets(list, 100000);
        for (auto &asset : list)
        {
          publish(asset);
        }

        publishCurrent(boost::system::error_code {});
      }

      /// @brief publish sample when observations arrive.
      SequenceNumber_t Mqtt2Service::publishSample(
          std::shared_ptr<observation::AsyncObserver> observer)
      {
        auto sampler = std::dynamic_pointer_cast<AsyncSample>(observer);
        auto topic = m_samplePrefix + *(sampler->m_device->getUuid());

        SequenceNumber_t end {0};
        std::string doc;

        {
          auto &buffer = m_sinkContract->getCircularBuffer();
          std::lock_guard<buffer::CircularBuffer> lock(buffer);

          auto firstSeq = buffer.getFirstSequence();
          auto lastSeq = buffer.getSequence() - 1;
          bool endOfBuffer {true};

          auto observations = m_sinkContract->getCircularBuffer().getObservations(
              m_sampleCount, sampler->getFilter(), sampler->getSequence(), nullopt, end, firstSeq,
              endOfBuffer);
          doc = m_printer->printSample(m_instanceId,
                                       m_sinkContract->getCircularBuffer().getBufferSize(), end,
                                       firstSeq, lastSeq, *observations, false);
        }

        m_client->asyncPublish(topic, doc, [sampler](std::error_code ec) {
          if (!ec)
          {
            sampler->handlerCompleted();
          }
        });

        return end;
      }

      void Mqtt2Service::publishCurrent(boost::system::error_code ec)
      {
        if (ec && ec != boost::asio::error::operation_aborted)
        {
          LOG(warning) << "Mqtt2Service::publishCurrent: " << ec.message();
          return;
        }

        if (!m_client->isRunning() || !m_client->isConnected())
        {
          LOG(warning) << "Mqtt2Service::publishCurrent: client stopped";
          return;
        }

        for (auto &device : m_sinkContract->getDevices())
        {
          auto topic = m_currentPrefix + *(device->getUuid());
          LOG(debug) << "Publishing current for: " << topic;

          ObservationList observations;
          SequenceNumber_t firstSeq, seq;
          auto filterSet = filterForDevice(device);

          {
            auto &buffer = m_sinkContract->getCircularBuffer();
            std::lock_guard<buffer::CircularBuffer> lock(buffer);

            firstSeq = buffer.getFirstSequence();
            seq = buffer.getSequence();
            m_sinkContract->getCircularBuffer().getLatest().getObservations(observations,
                                                                            filterSet);
          }

          auto doc = m_printer->printSample(m_instanceId,
                                            m_sinkContract->getCircularBuffer().getBufferSize(),
                                            seq, firstSeq, seq - 1, observations);

          m_client->publish(topic, doc);
        }

        using std::placeholders::_1;
        m_currentTimer.expires_after(m_currentInterval);
        m_currentTimer.async_wait(boost::asio::bind_executor(
            m_strand, boost::bind(&Mqtt2Service::publishCurrent, this, _1)));
      }

      bool Mqtt2Service::publish(observation::ObservationPtr &observation)
      {
        // Since we are doing periodic publishing, there is nothing to do here.
        return true;
      }

      bool Mqtt2Service::publish(device_model::DevicePtr device)
      {
        m_filters.clear();

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
