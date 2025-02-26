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

#pragma once

#include "boost/asio/io_context.hpp"
#include <boost/dll/alias.hpp>

#include <nlohmann/json.hpp>

#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/configuration/agent_config.hpp"
#include "mtconnect/entity/json_printer.hpp"
#include "mtconnect/mqtt/mqtt_client.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/printer//json_printer.hpp"
#include "mtconnect/printer/printer.hpp"
#include "mtconnect/printer/xml_printer_helper.hpp"
#include "mtconnect/sink/sink.hpp"
#include "mtconnect/utilities.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;
using namespace mtconnect::mqtt_client;

using json = nlohmann::json;

namespace mtconnect {
  class XmlPrinter;

  namespace sink {

    /// @brief MTConnect Mqtt implemention namespace

    namespace mqtt_sink {

      struct AsyncSample;

      class AGENT_LIB_API MqttService : public sink::Sink
      {
        // dynamic loading of sink

      public:
        /// @brief Create a Mqtt Service sink
        /// @param context the boost asio io_context
        /// @param contract the Sink Contract from the agent
        /// @param options configuration options
        /// @param config additional configuration options if specified directly as a sink
        MqttService(boost::asio::io_context &context, sink::SinkContractPtr &&contract,
                    const ConfigOptions &options, const boost::property_tree::ptree &config);

        ~MqttService() = default;

        // Sink Methods
        /// @brief Start the Mqtt service
        void start() override;

        /// @brief Shutdown the Mqtt service
        void stop() override;

        /// @brief Receive an observation
        ///
        /// This does nothing since we are periodically publishing current and samples
        ///
        /// @param observation shared pointer to the observation
        /// @return `true` if the publishing was successful
        bool publish(observation::ObservationPtr &observation) override;

        /// @brief Receive an asset
        /// @param asset shared point to the asset
        /// @return `true` if successful
        bool publish(asset::AssetPtr asset) override;

        /// @brief Receive a device
        /// @param device shared pointer to the device
        /// @return `true` if successful
        bool publish(device_model::DevicePtr device) override;

        /// @brief Publsh all devices, assets, and begin async timer-based publishing
        void pubishInitialContent();

        /// @brief Publish a current using `CurrentInterval` option.
        SequenceNumber_t publishCurrent(boost::system::error_code ec);

        /// @brief publish sample when observations arrive.
        SequenceNumber_t publishSample(std::shared_ptr<observation::AsyncObserver> sampler);

        /// @brief Register the Sink factory to create this sink
        /// @param factory
        static void registerFactory(SinkFactory &factory);

        /// @brief gets a Mqtt Client
        /// @return MqttClient
        std::shared_ptr<MqttClient> getClient() { return m_client; }

        /// @brief Mqtt Client is Connected or not
        /// @return `true` when the client was connected
        bool isConnected() { return m_client && m_client->isConnected(); }

      protected:
        const FilterSet &filterForDevice(const DevicePtr &device)
        {
          auto filter = m_filters.find(*(device->getUuid()));
          if (filter == m_filters.end())
          {
            auto pos = m_filters.emplace(*(device->getUuid()), FilterSet());
            filter = pos.first;
            auto &set = filter->second;
            for (const auto &wdi : device->getDeviceDataItems())
            {
              const auto di = wdi.lock();
              if (di)
                set.insert(di->getId());
            }
          }
          return filter->second;
        }

        std::string formatTopic(const std::string &topic, const DevicePtr device,
                                const std::string defaultUuid = "Unknown")
        {
          string uuid;
          string formatted {topic};
          if (!device)
            uuid = defaultUuid;
          else
          {
            uuid = *(device->getUuid());
            if (std::dynamic_pointer_cast<device_model::AgentDevice>(device))
            {
              uuid.insert(0, "Agent_");
            }
          }

          if (formatted.find("[device]") == std::string::npos)
          {
            if (formatted.back() != '/')
              formatted.append("/");
            formatted.append(uuid);
          }
          else
          {
            boost::replace_all(formatted, "[device]", uuid);
          }
          return formatted;
        }

        std::string getTopic(const std::string &option, int maxTopicDepth)
        {
          auto topic {get<string>(m_options[option])};
          auto depth = std::count(topic.begin(), topic.end(), '/');

          if (depth > maxTopicDepth)
            LOG(warning) << "Mqtt Option " << option
                         << " exceeds maximum number of levels: " << maxTopicDepth;

          return topic;
        }

      protected:
        std::string m_deviceTopic;    //! Device topic prefix
        std::string m_assetTopic;     //! Asset topic prefix
        std::string m_currentTopic;   //! Current topic prefix
        std::string m_sampleTopic;    //! Sample topic prefix
        std::string m_lastWillTopic;  //! Topic to publish the last will when disconnected

        std::chrono::milliseconds m_currentInterval;  //! Interval in ms to update current
        std::chrono::milliseconds m_sampleInterval;   //! min interval in ms to update sample

        uint64_t m_instanceId;

        boost::asio::io_context &m_context;
        boost::asio::io_context::strand m_strand;

        ConfigOptions m_options;

        std::unique_ptr<JsonEntityPrinter> m_jsonPrinter;
        std::unique_ptr<printer::JsonPrinter> m_printer;

        std::shared_ptr<MqttClient> m_client;
        boost::asio::steady_timer m_currentTimer;
        int m_sampleCount;  //! Timer for current requests

        std::map<std::string, FilterSet> m_filters;
        std::map<std::string, std::shared_ptr<AsyncSample>> m_samplers;
      };
    }  // namespace mqtt_sink
  }    // namespace sink
}  // namespace mtconnect
