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
#include "mtconnect/pipeline/pipeline.hpp"
#include "mtconnect/pipeline/transform.hpp"

namespace mtconnect::source::adapter {
  /// @brief Handler functions for handling data and connection status
  struct Handler
  {
    using ProcessData = std::function<void(const std::string &data, const std::string &source)>;
    using ProcessCommand = std::function<void(const std::string &command, const std::string &value,
                                              const std::string &source)>;
    using ProcessMessage = std::function<void(const std::string &topic, const std::string &data,
                                              const std::string &source)>;
    using Connect = std::function<void(const std::string &source)>;

    /// @brief Process Data Messages
    ProcessData m_processData;
    /// @brief Process an adapter command
    ProcessCommand m_command;
    /// @brief Process a message with a topic
    ProcessMessage m_processMessage;

    /// @brief method to call when connecting
    Connect m_connecting;
    /// @brief method to call when connected
    Connect m_connected;
    /// @brief method to call when disconnected
    Connect m_disconnected;
  };

  /// @brief The adapter pipeline with common pipeline construction methods. This class
  ///        is subclassed for particular adapters
  class AGENT_LIB_API AdapterPipeline : public pipeline::Pipeline
  {
  public:
    /// @brief Create and adapter pipeline
    /// @param context the pipeline context
    /// @param st boost asio strand
    AdapterPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &st)
      : Pipeline(context, st)
    {}

    /// @brief build the pipeline
    /// @param options the configuration options
    void build(const ConfigOptions &options) override;
    /// @brief Create a handler
    /// @return the handler handing over ownership
    virtual std::unique_ptr<Handler> makeHandler();

    /// @brief get the associated device
    /// @return the device
    const auto &getDevice() const { return m_device; }

    /// @brief set the associated device
    /// @param d the device
    void setDevice(const std::string &d) { m_device = d; }

  protected:
    void buildDeviceList();
    void buildCommandAndStatusDelivery(pipeline::TransformPtr next = nullptr);
    void buildDeviceDelivery(pipeline::TransformPtr next);
    void buildAssetDelivery(pipeline::TransformPtr next);
    void buildObservationDelivery(pipeline::TransformPtr next);

  protected:
    StringList m_devices;
    std::optional<std::string> m_device;
    std::string m_identity;
    ConfigOptions m_options;
  };
}  // namespace mtconnect::source::adapter
