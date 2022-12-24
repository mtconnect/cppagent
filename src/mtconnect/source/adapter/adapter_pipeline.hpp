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

#include "pipeline/pipeline.hpp"
#include "pipeline/transform.hpp"

namespace mtconnect::source::adapter {
  struct Handler
  {
    using ProcessData = std::function<void(const std::string &data, const std::string &source)>;
    using ProcessMessage = std::function<void(const std::string &topic, const std::string &data,
                                              const std::string &source)>;
    using Connect = std::function<void(const std::string &source)>;

    ProcessData m_processData;
    ProcessData m_command;
    ProcessMessage m_processMessage;

    Connect m_connecting;
    Connect m_connected;
    Connect m_disconnected;
  };

  class AdapterPipeline : public pipeline::Pipeline
  {
  public:
    AdapterPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &st)
      : Pipeline(context, st)
    {}

    void build(const ConfigOptions &options) override;
    virtual std::unique_ptr<Handler> makeHandler();

  protected:
    void buildDeviceList();
    void buildCommandAndStatusDelivery();
    void buildAssetDelivery(pipeline::TransformPtr next);
    void buildObservationDelivery(pipeline::TransformPtr next);

  protected:
    ConfigOptions m_options;
    StringList m_devices;
    std::optional<std::string> m_device;
    std::string m_identity;
  };
}  // namespace mtconnect::source::adapter
