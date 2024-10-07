//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/config.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/adapter_pipeline.hpp"

namespace mtconnect {
  /// @brief MQTT Cient namespace
  namespace mqtt_client {
    class MqttClient;

    /// @brief MQTT Cient Handler for to know the status of the client connection
    struct ClientHandler
    {
      using Connect = std::function<void(std::shared_ptr<MqttClient>)>;
      using Received = std::function<void(std::shared_ptr<MqttClient>, const std::string &topic,
                                          const std::string &payload)>;

      Connect m_connected;
      Connect m_connecting;
      Connect m_disconnected;
      Received m_receive;
    };

    class MqttClient : public std::enable_shared_from_this<MqttClient>
    {
    public:
      enum class QOS
      {
        at_most_once,
        at_least_once,
        exactly_once
      };

      /// @brief Create an Mqtt Client with an asio context and ClientHandler
      /// @param context a boost asio context
      /// @param ClientHandler configuration options
      /// - ConnectInterval, defaults to 5000

      MqttClient(boost::asio::io_context &ioc, std::unique_ptr<ClientHandler> &&handler,
                 const std::optional<std::string> willTopic = std::nullopt,
                 const std::optional<std::string> willPayload = std::nullopt)
        : m_ioContext(ioc),
          m_handler(std::move(handler)),
          m_connectInterval(5000),
          m_willTopic(willTopic),
          m_willPayload(willPayload)
      {}
      virtual ~MqttClient() = default;

      /// @brief get the clientId
      /// @return the clientId
      const auto &getIdentity() const { return m_identity; }

      /// @brief get the Url link mqtt://localhost:1883
      /// @return the Url to access localhost
      const auto &getUrl() const { return m_url; }

      /// @brief Start the Mqtt Client
      virtual bool start() = 0;

      /// @brief Stop the Mqtt Client
      virtual void stop() = 0;

      /// @brief Subscribe Topic to the Mqtt Client
      /// @param topic Subscribing to the topic
      /// @return boolean either topic sucessfully connected and subscribed
      virtual bool subscribe(const std::string &topic) = 0;

      /// @brief Publish Topic to the Mqtt Client
      /// @param topic Publishing to the topic
      /// @param payload Publishing to the payload
      /// @return boolean either topic sucessfully connected and published
      virtual bool publish(const std::string &topic, const std::string &payload, bool retain = true,
                           QOS qos = QOS::at_least_once) = 0;

      /// @brief Publish Topic to the Mqtt Client and call the async handler
      /// @param topic Publishing to the topic
      /// @param payload Publishing to the payload
      /// @return boolean either topic sucessfully connected and published
      virtual bool asyncPublish(const std::string &topic, const std::string &payload,
                                std::function<void(std::error_code)> callback, bool retain = true,
                                QOS qos = QOS::at_least_once) = 0;

      /// @brief Mqtt Client is connected
      /// @return bool Either Client is sucessfully connected or not
      auto isConnected() { return m_connected; }

      /// @brief Mqtt Client is Running
      /// @return bool Either Client is sucessfully running or not
      auto isRunning() { return m_running; }

      /// @brief set the Mqtt Client is completly connected
      void connectComplete() { m_connected = true; }

    protected:
      boost::asio::io_context &m_ioContext;
      std::string m_url;
      std::string m_identity;
      std::unique_ptr<ClientHandler> m_handler;
      std::chrono::milliseconds m_connectInterval;
      std::optional<std::string> m_willTopic;
      std::optional<std::string> m_willPayload;

      bool m_running {false};
      bool m_connected {false};
    };

  }  // namespace mqtt_client
}  // namespace mtconnect
