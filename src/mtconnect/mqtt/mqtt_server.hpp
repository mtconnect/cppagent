
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "source/adapter/adapter.hpp"
#include "source/adapter/adapter_pipeline.hpp"

namespace mtconnect {
  namespace mqtt_server {
    class MqttServer : public std::enable_shared_from_this<MqttServer>
    {
    public:
      MqttServer(boost::asio::io_context &ioc) : m_ioContext(ioc), m_port(1883) {}
      virtual ~MqttServer() = default;
      const auto &getUrl() const { return m_url; }
      auto getPort() const { return m_port; }

      virtual bool start() = 0;
      virtual void stop() = 0;

    protected:
      boost::asio::io_context &m_ioContext;
      std::string m_url;
      uint16_t m_port;
    };
  }  // namespace mqtt_server
}  // namespace mtconnect
