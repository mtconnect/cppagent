
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//       http://www.apache.org/licenses/LICENSE-2.0
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#pragma once

#include "mtconnect/config.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/adapter_pipeline.hpp"

namespace mtconnect {
  /// @brief MQTT Server namespace
  namespace mqtt_server {
    /// @brief
    class MqttServer : public std::enable_shared_from_this<MqttServer>
    {
    public:
      /// @brief Create an Mqtt server with an asio context
      /// @param ioc a boost asio context
      MqttServer(boost::asio::io_context &ioc) : m_ioContext(ioc), m_port(1883) {}

      virtual ~MqttServer() = default;

      /// @brief Get the Mqtt url
      /// @return Mqtt url
      const auto &getUrl() const { return m_url; }

      /// @brief get the bind port
      /// @return the port being bound
      auto getPort() const { return m_port; }

      /// @brief Start the Mqtt server
      virtual bool start() = 0;

      /// @brief Shutdown the Mqtt server
      virtual void stop() = 0;

      auto &getWill() { return m_will; }

    protected:
      boost::asio::io_context &m_ioContext;
      std::string m_url;
      uint16_t m_port;
      std::optional<mqtt::will> m_will;
    };
  }  // namespace mqtt_server
}  // namespace mtconnect
