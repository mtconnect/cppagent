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

#pragma once

#include "source/adapter/adapter.hpp"
#include "source/adapter/adapter_pipeline.hpp"

namespace mtconnect {
  namespace mqtt_client {
    class MqttClient;

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
      MqttClient(boost::asio::io_context &ioc, std::unique_ptr<ClientHandler> &&handler)
        : m_ioContext(ioc), m_handler(std::move(handler)), m_connectInterval(5000)
      {}
      virtual ~MqttClient() = default;
      const auto &getIdentity() const { return m_identity; }
      const auto &getUrl() const { return m_url; }
      virtual bool start() = 0;
      virtual void stop() = 0;
      virtual bool subscribe(const std::string &topic) = 0;
      virtual bool publish(const std::string &topic, const std::string &payload) = 0;
      auto isConnected() { return m_connected; }
      auto isRunning() { return m_running; }
      void connectComplete() { m_connected = true; }

    protected:
      boost::asio::io_context &m_ioContext;
      std::string m_url;
      std::string m_identity;
      std::unique_ptr<ClientHandler> m_handler;
      std::chrono::milliseconds m_connectInterval;

      bool m_running {false};
      bool m_connected {false};
    };

  }  // namespace mqtt_client
}  // namespace mtconnect
