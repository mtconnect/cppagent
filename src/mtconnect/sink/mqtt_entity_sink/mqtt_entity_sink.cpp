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

#include "mqtt_entity_sink.hpp"

#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/mqtt/mqtt_client_impl.hpp"
#include "mtconnect/observation/observation.hpp"

using ptree = boost::property_tree::ptree;
using json = nlohmann::json;

using namespace std;
using namespace mtconnect;
using namespace mtconnect::asset;
using namespace mtconnect::observation;

namespace asio = boost::asio;
namespace config = ::mtconnect::configuration;

namespace mtconnect {
  namespace sink {
    namespace mqtt_entity_sink {

      MqttEntitySink::MqttEntitySink(boost::asio::io_context& context,
                                     sink::SinkContractPtr&& contract, const ConfigOptions& options,
                                     const ptree& config)
        : Sink("MqttEntitySink", std::move(contract)),
          m_context(context),
          m_strand(context),
          m_options(options)
      {
        GetOptions(config, m_options, options);

        AddOptions(config, m_options,
                   {{configuration::MqttCaCert, string()},
                    {configuration::MqttPrivateKey, string()},
                    {configuration::MqttCert, string()},
                    {configuration::MqttClientId, string()},
                    {configuration::MqttUserName, string()},
                    {configuration::MqttPassword, string()}});

        AddDefaultedOptions(
            config, m_options,
            {{configuration::MqttHost, "127.0.0.1"s},
             {configuration::ObservationTopicPrefix, "MTConnect/Devices/[device]/Observations"s},
             {configuration::DeviceTopicPrefix, "MTConnect/Probe/[device]"s},
             {configuration::AssetTopicPrefix, "MTConnect/Asset/[device]"s},
             {configuration::MqttLastWillTopic, "MTConnect/Probe/[device]/Availability"s},
             {configuration::MqttPort, 1883},
             {configuration::MqttTls, false},
             {configuration::MqttQOS, 1},
             {configuration::MqttRetain, false}});

        m_observationTopicPrefix = get<string>(m_options[configuration::ObservationTopicPrefix]);
        m_deviceTopicPrefix = get<string>(m_options[configuration::DeviceTopicPrefix]);
        m_assetTopicPrefix = get<string>(m_options[configuration::AssetTopicPrefix]);
      }

      static MqttClient::QOS parseQos(const ConfigOptions& options)
      {
        if (auto qosInt = GetOption<int>(options, configuration::MqttQOS))
        {
          return *qosInt == 0   ? MqttClient::QOS::at_most_once
                 : *qosInt == 2 ? MqttClient::QOS::exactly_once
                                : MqttClient::QOS::at_least_once;
        }

        if (auto qosStr = GetOption<std::string>(options, configuration::MqttQOS))
        {
          if (*qosStr == "at_most_once" || *qosStr == "0")
            return MqttClient::QOS::at_most_once;
          if (*qosStr == "exactly_once" || *qosStr == "2")
            return MqttClient::QOS::exactly_once;
        }

        return MqttClient::QOS::at_least_once;
      }

      void MqttEntitySink::start()
      {
        if (!m_client)
        {
          auto clientHandler = make_unique<ClientHandler>();
          clientHandler->m_connected = [this](shared_ptr<MqttClient> client) {
            LOG(debug) << "MqttEntitySink: Client connected to broker";
            client->connectComplete();

            auto agentDevice = m_sinkContract->getDeviceByName("Agent");
            if (agentDevice)
            {
              auto lwtTopic = get<string>(m_options[configuration::MqttLastWillTopic]);
              boost::replace_all(lwtTopic, "[device]", *agentDevice->getUuid());

              // Get QoS and Retain from options
              auto qos = parseQos(m_options);
              bool retain = GetOption<bool>(m_options, configuration::MqttRetain).value_or(false);
              LOG(debug) << "Publishing availability to: " << lwtTopic;

              client->publish(lwtTopic, "AVAILABLE", retain, qos);
            }

            // Publish initial content
            publishInitialContent();
          };

          auto agentDevice = m_sinkContract->getDeviceByName("Agent");
          auto lwtTopic = get<string>(m_options[configuration::MqttLastWillTopic]);
          if (agentDevice)
          {
            boost::replace_all(lwtTopic, "[device]", *agentDevice->getUuid());
          }
          m_lastWillTopic = lwtTopic;

          if (IsOptionSet(m_options, configuration::MqttTls))
          {
            m_client = make_shared<MqttTlsClient>(m_context, m_options, std::move(clientHandler),
                                                  m_lastWillTopic, "UNAVAILABLE"s);
          }
          else
          {
            m_client = make_shared<MqttTcpClient>(m_context, m_options, std::move(clientHandler),
                                                  m_lastWillTopic, "UNAVAILABLE"s);
          }
        }
        LOG(debug) << "Starting MQTT Entity Sink client";
        m_client->start();
      }

      void MqttEntitySink::stop()
      {
        if (m_client)
        {
          // Publish UNAVAILABLE before disconnecting
          if (m_client->isConnected())
          {
            auto qos = parseQos(m_options);
            m_client->publish(m_lastWillTopic, "UNAVAILABLE", true, qos);
          }
          m_client->stop();
        }
      }

      void MqttEntitySink::publishInitialContent()
      {
        LOG(debug) << "MqttEntitySink: Publishing initial content";

        // Publish all devices
        int deviceCount = 0;
        for (auto& dev : m_sinkContract->getDevices())
        {
          publish(dev);
          deviceCount++;
        }
        LOG(debug) << "Published " << deviceCount << " devices";

        // Publish current observations for all devices
        int obsCount = 0;
        for (auto& dev : m_sinkContract->getDevices())
        {
          auto& buffer = m_sinkContract->getCircularBuffer();
          std::lock_guard<buffer::CircularBuffer> lock(buffer);

          auto latest = buffer.getLatest();
          observation::ObservationList observations;

          for (auto& di : dev->getDeviceDataItems())
          {
            auto dataItem = di.lock();
            if (dataItem)
            {
              auto obs = latest.getObservation(dataItem->getId());
              if (obs)
              {
                observations.push_back(obs);
              }
            }
          }

          // Publish each observation
          for (auto& obs : observations)
          {
            auto obsCopy = obs;
            if (m_client && m_client->isConnected())
            {
              publish(obsCopy);
              obsCount++;
            }
            else
            {
              std::lock_guard<std::mutex> lock(m_queueMutex);
              if (m_queuedObservations.size() >= MAX_QUEUE_SIZE)
              {
                m_queuedObservations.erase(m_queuedObservations.begin());
              }
              m_queuedObservations.push_back(obsCopy);
              obsCount++;
            }
          }
        }
        LOG(debug) << "Published " << obsCount << " initial observations";
      }

      std::string MqttEntitySink::formatTimestamp(const Timestamp& timestamp)
      {
        using namespace date;
        using namespace std::chrono;

        std::ostringstream oss;
        oss << date::format("%FT%TZ", floor<microseconds>(timestamp));
        return oss.str();
      }

      std::string MqttEntitySink::getObservationValue(
          const observation::ObservationPtr& observation)
      {
        if (observation->isUnavailable())
        {
          return "UNAVAILABLE";
        }

        auto value = observation->getValue();

        if (holds_alternative<string>(value))
        {
          return get<string>(value);
        }
        else if (holds_alternative<int64_t>(value))
        {
          return std::to_string(get<int64_t>(value));
        }
        else if (holds_alternative<double>(value))
        {
          std::ostringstream oss;
          oss << std::fixed << std::setprecision(6) << get<double>(value);
          return oss.str();
        }
        else if (holds_alternative<entity::Vector>(value))
        {
          auto& vec = get<entity::Vector>(value);
          std::ostringstream oss;
          for (size_t i = 0; i < vec.size(); ++i)
          {
            if (i > 0)
              oss << " ";
            oss << std::fixed << std::setprecision(6) << vec[i];
          }
          return oss.str();
        }
        else if (holds_alternative<entity::DataSet>(value))
        {
          // For DataSet, return as JSON string
          json j = json::object();
          auto& ds = get<entity::DataSet>(value);
          for (auto& entry : ds)
          {
            // Convert the variant value to appropriate JSON type
            if (holds_alternative<string>(entry.m_value))
            {
              j[entry.m_key] = get<string>(entry.m_value);
            }
            else if (holds_alternative<int64_t>(entry.m_value))
            {
              j[entry.m_key] = get<int64_t>(entry.m_value);
            }
            else if (holds_alternative<double>(entry.m_value))
            {
              j[entry.m_key] = get<double>(entry.m_value);
            }
          }
          return j.dump();
        }

        return "UNAVAILABLE";
      }

      std::string MqttEntitySink::formatObservationJson(
          const observation::ObservationPtr& observation)
      {
        json j;

        try
        {
          auto dataItem = observation->getDataItem();
          if (!dataItem)
          {
            LOG(error) << "Observation has no data item";
            return "{}";
          }

          j["dataItemId"] = dataItem->getId();

          const auto& name = dataItem->getName();
          if (name && !name->empty())
          {
            j["name"] = *name;
          }

          j["type"] = dataItem->getType();

          auto subType = dataItem->maybeGet<std::string>("subType");
          if (subType && !subType->empty())
          {
            j["subType"] = *subType;
          }

          j["timestamp"] = formatTimestamp(observation->getTimestamp());

          // Get the category
          auto category = dataItem->getCategory();
          if (category == device_model::data_item::DataItem::SAMPLE)
          {
            j["category"] = "SAMPLE";
          }
          else if (category == device_model::data_item::DataItem::EVENT)
          {
            j["category"] = "EVENT";
          }
          else if (category == device_model::data_item::DataItem::CONDITION)
          {
            j["category"] = "CONDITION";
          }

          // Add the result/value
          j["result"] = getObservationValue(observation);

          // Add sequence number
          j["sequence"] = observation->getSequence();

          auto result = j.dump();
          LOG(trace) << "Formatted observation JSON: " << result;
          return result;
        }
        catch (const std::exception& e)
        {
          LOG(error) << "Exception formatting observation: " << e.what();
          return "{}";
        }
      }

      std::string MqttEntitySink::formatConditionJson(const observation::ConditionPtr& condition)
      {
        json j;

        auto dataItem = condition->getDataItem();
        if (!dataItem)
        {
          return "{}";
        }

        j["dataItemId"] = dataItem->getId();

        const auto& name = dataItem->getName();
        if (name && !name->empty())
        {
          j["name"] = *name;
        }

        j["type"] = dataItem->getType();

        auto subType = dataItem->maybeGet<std::string>("subType");
        if (subType && !subType->empty())
        {
          j["subType"] = *subType;
        }

        j["timestamp"] = formatTimestamp(condition->getTimestamp());
        j["category"] = "CONDITION";

        // Add condition-specific fields
        switch (condition->getLevel())
        {
          case Condition::NORMAL:
            j["level"] = "NORMAL";
            break;
          case Condition::WARNING:
            j["level"] = "WARNING";
            break;
          case Condition::FAULT:
            j["level"] = "FAULT";
            break;
          case Condition::UNAVAILABLE:
            j["level"] = "UNAVAILABLE";
            break;
        }

        // Add native code if present
        if (condition->hasProperty("nativeCode"))
        {
          j["nativeCode"] = condition->get<string>("nativeCode");
        }

        // Add condition ID if present
        if (!condition->getCode().empty())
        {
          j["conditionId"] = condition->getCode();
        }

        // Add message/value if present
        if (condition->hasValue())
        {
          j["message"] = getObservationValue(condition);
        }

        // Add sequence number
        j["sequence"] = condition->getSequence();

        return j.dump();
      }

      std::string MqttEntitySink::getObservationTopic(
          const observation::ObservationPtr& observation)
      {
        auto dataItem = observation->getDataItem();
        if (!dataItem)
        {
          return "";
        }

        auto device = dataItem->getComponent()->getDevice();
        if (!device)
        {
          return "";
        }

        std::string topic = m_observationTopicPrefix;
        boost::replace_all(topic, "[device]", *device->getUuid());

        // Append data item ID for flat structure
        topic += "/" + dataItem->getId();

        return topic;
      }

      bool MqttEntitySink::publish(observation::ObservationPtr& observation)
      {
        auto dataItem = observation->getDataItem();
        if (!dataItem)
        {
          LOG(warning) << "MqttEntitySink::publish: Observation has no data item";
          return false;
        }

        if (!m_client || !m_client->isConnected())
        {
          std::lock_guard<std::mutex> lock(m_queueMutex);
          if (m_queuedObservations.size() >= MAX_QUEUE_SIZE)
          {
            LOG(warning) << "MqttEntitySink::publish: Observation queue full (" << MAX_QUEUE_SIZE
                         << "), dropping oldest observation for "
                         << m_queuedObservations.front()->getDataItem()->getId();
            m_queuedObservations.erase(m_queuedObservations.begin());
          }
          LOG(debug) << "MqttEntitySink::publish: Client not connected, queuing observation for "
                     << dataItem->getId();
          m_queuedObservations.push_back(observation);
          return false;
        }

        std::string topic = getObservationTopic(observation);
        if (topic.empty())
        {
          LOG(warning) << "MqttEntitySink::publish: Empty topic for " << dataItem->getId();
          return false;
        }

        // Get QoS setting
        auto qos = parseQos(m_options);
        bool retain = GetOption<bool>(m_options, configuration::MqttRetain).value_or(false);

        try
        {
          auto condition = dynamic_pointer_cast<observation::Condition>(observation);
          if (condition)
          {
            observation::ConditionList condList;
            condition->getFirst()->getConditionList(condList);

            for (auto& cond : condList)
            {
              std::string payload = formatConditionJson(cond);
              LOG(debug) << "Publishing condition to: " << topic
                         << ", payload size: " << payload.size();
              m_client->publish(topic, payload, retain, qos);
            }
          }
          else
          {
            std::string payload = formatObservationJson(observation);
            LOG(debug) << "Publishing observation to: " << topic << ", size: " << payload.size();
            m_client->publish(topic, payload, retain, qos);
          }

          return true;
        }
        catch (const std::exception& e)
        {
          LOG(error) << "Exception publishing observation: " << e.what();
          return false;
        }
      }

      bool MqttEntitySink::publish(device_model::DevicePtr device)
      {
        if (!m_client || !m_client->isConnected())
        {
          return false;
        }

        // For device, we could publish the device XML or JSON
        // For now, just return true as device publishing is optional
        return true;
      }

      bool MqttEntitySink::publish(asset::AssetPtr asset)
      {
        if (!m_client || !m_client->isConnected())
        {
          return false;
        }

        // Asset publishing can be added here if needed
        return true;
      }

      void MqttEntitySink::registerFactory(SinkFactory& factory)
      {
        factory.registerFactory(
            "MqttEntitySink",
            [](const std::string& name, boost::asio::io_context& io, SinkContractPtr&& contract,
               const ConfigOptions& options, const boost::property_tree::ptree& block) -> SinkPtr {
              auto sink = std::make_shared<MqttEntitySink>(io, std::move(contract), options, block);
              return sink;
            });
      }

    }  // namespace mqtt_entity_sink
  }  // namespace sink
}  // namespace mtconnect
