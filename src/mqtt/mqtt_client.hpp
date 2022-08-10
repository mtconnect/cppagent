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
    class MqttClient : public std::enable_shared_from_this<MqttClient>
    {
    public:
      MqttClient(boost::asio::io_context &ioc) : m_ioContext(ioc) {}
      virtual ~MqttClient() = default;
      const auto &getIdentity() const { return m_identity; }
      const auto &getUrl() const { return m_url; }
      virtual bool start() = 0;
      virtual void stop() = 0;
      virtual bool subscribe(const std::string &topic) = 0;
      virtual bool publish(const std::string &topic, const std::string &payload) = 0;
      auto isConnected() { return m_connected; }
      auto isRunning() { return m_running; }

    protected:
      boost::asio::io_context &m_ioContext;
      std::string m_url;
      std::string m_identity;
      
      bool m_running { false };
      bool m_connected { false };
    };
  }  // namespace mqtt_client
}  // namespace mtconnect
