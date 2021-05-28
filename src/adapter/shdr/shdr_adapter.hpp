//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <chrono>
#include <date/tz.h>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#include "adapter/adapter.hpp"
#include "connector.hpp"
#include "device_model/data_item/data_item.hpp"
#include "shdr_pipeline.hpp"
#include "source.hpp"
#include "utilities.hpp"

namespace mtconnect {
class Agent;
namespace device_model {
class Device;
}

namespace adapter {
namespace shdr {
class ShdrAdapter : public Connector, public adapter::Adapter
{
public:
  // Associate adapter with a device & connect to the server & port
  ShdrAdapter(boost::asio::io_context &context, const std::string &server, const unsigned int port,
              const ConfigOptions &options, std::unique_ptr<ShdrPipeline> &pipeline);
  ShdrAdapter(const ShdrAdapter &) = delete;

  // Virtual destructor
  ~ShdrAdapter() override { stop(); }

  void setHandler(std::unique_ptr<Handler> &h) { m_handler = std::move(h); }
  auto &getTerminator() const { return m_terminator; }

  // Inherited method to incoming data from the server
  void processData(const std::string &data) override;
  void protocolCommand(const std::string &data) override;

  // Method called when connection is lost.
  void connecting() override
  {
    if (m_handler && m_handler->m_connecting)
      m_handler->m_connecting(getIdentity());
  }
  void disconnected() override
  {
    if (m_handler && m_handler->m_disconnected)
      m_handler->m_disconnected(getIdentity());
  }
  void connected() override
  {
    if (m_handler && m_handler->m_connected)
      m_handler->m_connected(getIdentity());
  }

  // Agent Device methods
  const std::string &getHost() const override { return m_server; }
  unsigned int getPort() const override { return m_port; }

  // Start and Stop
  void stop() override;
  bool start() override
  {
    if (Connector::start())
    {
      m_pipeline->start(m_strand);
      return true;
    }
    else
      return false;
  }

  void setOptions(const ConfigOptions &options)
  {
    for (auto &o : options)
      m_options.insert_or_assign(o.first, o.second);
    m_pipeline->build(m_options);
    if (m_pipeline->started())
      m_pipeline->start(m_strand);
  }

protected:
  std::unique_ptr<ShdrPipeline> m_pipeline;

  // If the connector has been running
  bool m_running;

  std::optional<std::string> m_terminator;
  std::stringstream m_body;
};
}  // namespace shdr
}  // namespace adapter
}  // namespace mtconnect
