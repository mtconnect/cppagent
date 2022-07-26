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

#include "mqtt/mqtt_client.hpp"
#include "source/adapter/adapter.hpp"
#include "source/adapter/adapter_pipeline.hpp"

using namespace mtconnect::mqtt_client;

namespace mtconnect::source::adapter::mqtt_adapter {

  class MqttPipeline : public AdapterPipeline
  {
  public:
    MqttPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &strand)
      : AdapterPipeline(context, strand)
    {}

    void build(const ConfigOptions &options) override;

  protected:
    ConfigOptions m_options;
  };

  class MqttAdapter : public Adapter
  {
  public:
    MqttAdapter(boost::asio::io_context &io, pipeline::PipelineContextPtr pipelineContext,
                const ConfigOptions &options, const boost::property_tree::ptree &block);
    ~MqttAdapter() override {}

    static void registerFactory(SourceFactory &factory);

    const std::string &getHost() const override;

    unsigned int getPort() const override;

    bool start() override;

    void stop() override;

    pipeline::Pipeline *getPipeline() override;

  protected:
    void loadTopics(const boost::property_tree::ptree &tree, ConfigOptions &options);

  protected:
    boost::asio::io_context &m_ioContext;

    boost::asio::io_context::strand m_strand;
    // If the connector has been running
    bool m_running;

    std::string m_host;

    unsigned int m_port;

    MqttPipeline m_pipeline;

    std::shared_ptr<MqttClient> m_client;
  };
}  // namespace mtconnect::source::adapter::mqtt_adapter
