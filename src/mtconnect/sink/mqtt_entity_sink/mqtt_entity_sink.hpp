//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include "mtconnect/mqtt/mqtt_client.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/sink/sink.hpp"
#include "mtconnect/utilities.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::mqtt_client;
using json = nlohmann::json;

namespace mtconnect {
  namespace sink {
    namespace mqtt_entity_sink {

      /// @brief MTConnect Entity MQTT Sink - publishes observations per data item
      class AGENT_LIB_API MqttEntitySink : public sink::Sink
      {
      public:
        /// @brief Create an MQTT Entity Sink
        /// @param context the boost asio io_context
        /// @param contract the Sink Contract from the agent
        /// @param options configuration options
        /// @param config additional configuration options
        MqttEntitySink(boost::asio::io_context& context, sink::SinkContractPtr&& contract,
                       const ConfigOptions& options, const boost::property_tree::ptree& config);

        ~MqttEntitySink() = default;

        // Sink Methods
        /// @brief Start the MQTT Entity service
        void start() override;

        /// @brief Shutdown the MQTT Entity service
        void stop() override;

        /// @brief Receive an observation and publish it
        /// @param observation shared pointer to the observation
        /// @return `true` if the publishing was successful
        bool publish(observation::ObservationPtr& observation) override;

        /// @brief Receive an asset
        /// @param asset shared point to the asset
        /// @return `true` if successful
        bool publish(asset::AssetPtr asset) override;

        /// @brief Receive a device
        /// @param device shared pointer to the device
        /// @return `true` if successful
        bool publish(device_model::DevicePtr device) override;

        /// @brief Register the Sink factory to create this sink
        /// @param factory
        static void registerFactory(SinkFactory& factory);

        /// @brief Gets the MQTT Client
        /// @return MqttClient
        std::shared_ptr<MqttClient> getClient() { return m_client; }

        /// @brief Check if MQTT Client is Connected
        /// @return `true` when the client is connected
        bool isConnected() { return m_client && m_client->isConnected(); }

      protected:
        /// @brief Format observation as JSON matching MTConnect.NET format
        /// @param observation the observation to format
        /// @return JSON string
        std::string formatObservationJson(const observation::ObservationPtr& observation);

        /// @brief Format condition observation as JSON
        /// @param condition the condition observation
        /// @return JSON string
        std::string formatConditionJson(const observation::ConditionPtr& condition);

        /// @brief Get topic for observation using flat structure
        /// @param observation the observation
        /// @return formatted topic string
        std::string getObservationTopic(const observation::ObservationPtr& observation);

        /// @brief Get value from observation as string
        /// @param observation the observation
        /// @return value as string
        std::string getObservationValue(const observation::ObservationPtr& observation);

        /// @brief Publish initial device and current observations
        void publishInitialContent();

        /// @brief Convert timestamp to ISO 8601 format
        /// @param timestamp the timestamp
        /// @return ISO 8601 string
        std::string formatTimestamp(const Timestamp& timestamp);

      protected:
        static constexpr size_t MAX_QUEUE_SIZE = 10000; // Maximum queued observations

        std::string m_observationTopicPrefix;  //! Observation topic prefix
        std::string m_deviceTopicPrefix;       //! Device topic prefix
        std::string m_assetTopicPrefix;        //! Asset topic prefix
        std::string m_lastWillTopic;           //! Topic to publish last will

        boost::asio::io_context& m_context;
        boost::asio::io_context::strand m_strand;

        ConfigOptions m_options;

        std::shared_ptr<MqttClient> m_client;
        std::vector<observation::ObservationPtr> m_queuedObservations;
        std::mutex m_queueMutex;
      };
    }  // namespace mqtt_entity_sink
  }  // namespace sink
}  // namespace mtconnect
