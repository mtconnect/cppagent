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
#include "mtconnect/mqtt/mqtt_client.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/adapter_pipeline.hpp"

using namespace mtconnect::mqtt_client;

/// @brief  @brief the Mqtt adapter namespace
namespace mtconnect::source::adapter::mqtt_adapter {
  /// @brief The Mqtt adapter pipeline
  class AGENT_LIB_API MqttPipeline : public AdapterPipeline
  {
  public:
    /// @brief Create an adapter pipeline
    ///
    /// Feedback is used to determine if recovery is possible and when to start a
    /// new Mqtt session with the upstream agent
    /// @param context the pipeline context
    /// @param st strand to run in
    MqttPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &strand)
      : AdapterPipeline(context, strand)
    {}

    void build(const ConfigOptions &options) override;
    Handler *m_handler {nullptr};
  };

  /// @brief An Mqtt adapter to connnect to another Agent and replicate data

  class AGENT_LIB_API MqttAdapter : public Adapter
  {
  public:
    /// @brief Create an Mqtt adapter
    /// @param io boost asio io context
    /// @param context pipeline context
    /// @param options configation options
    /// @param block additional configuration options
    MqttAdapter(boost::asio::io_context &io, pipeline::PipelineContextPtr pipelineContext,
                const ConfigOptions &options, const boost::property_tree::ptree &block);

    ~MqttAdapter() override {}

    /// @brief Register the Mqtt adapter with the factory
    /// @param factory source factory
    static void registerFactory(SourceFactory &factory);

    /// @name Agent Device methods
    ///@{
    const std::string &getHost() const override;

    unsigned int getPort() const override;
    ///@}

    /// @name Source interface
    ///@{
    bool start() override;

    void stop() override;

    pipeline::Pipeline *getPipeline() override;
    ///@}

    /// @brief subcribe to topics
    void subscribeToTopics();

  protected:
    /// @brief load all topics
    /// @param ptree the property tree coming from configuration parser
    /// @param options configation options
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
