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
#include "mtconnect/pipeline/mtconnect_xml_transform.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/adapter_pipeline.hpp"
#include "mtconnect/utilities.hpp"
#include "session.hpp"

namespace boost::asio::ssl {
  class context;
}

/// @brief  @brief the agent adapter namespace
namespace mtconnect::source::adapter::agent_adapter {
  using namespace mtconnect;
  using namespace source::adapter;

  /// @brief The Agent adapter pipeline
  class AGENT_LIB_API AgentAdapterPipeline : public AdapterPipeline
  {
  public:
    /// @brief Create an adapter pipeline
    ///
    /// Feedback is used to determine if recovery is possible and when to start a
    /// new http streaming session with the upstream agent
    /// @param context the pipeline context
    /// @param st strand to run in
    /// @param feedback feedback from the pipeline to the adapter when an error occurs
    AgentAdapterPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &st,
                         pipeline::XmlTransformFeedback &feedback)
      : AdapterPipeline(context, st), m_feedback(feedback)
    {}
    void build(const ConfigOptions &options) override;

    Handler *m_handler = nullptr;
    pipeline::XmlTransformFeedback &m_feedback;
    std::optional<std::string> m_uuid;
  };

  /// @brief An adapter to connnect to another Agent and replicate data
  class AGENT_LIB_API AgentAdapter : public Adapter
  {
  public:
    /// @brief Create an agent adapter
    /// @param io boost asio io context
    /// @param context pipeline context
    /// @param options configation options
    /// @param block additional configuration options
    AgentAdapter(boost::asio::io_context &io, pipeline::PipelineContextPtr context,
                 const ConfigOptions &options, const boost::property_tree::ptree &block);

    /// @brief Register the agent adapter with the factory for `http` and `https`
    /// @param factory source factory
    static void registerFactory(SourceFactory &factory)
    {
      auto cb = [](const std::string &name, boost::asio::io_context &io,
                   pipeline::PipelineContextPtr context, const ConfigOptions &options,
                   const boost::property_tree::ptree &block) -> source::SourcePtr {
        auto source = std::make_shared<AgentAdapter>(io, context, options, block);
        return source;
      };
      factory.registerFactory("http", cb);
      factory.registerFactory("https", cb);
    }

    /// @name Agent Device methods
    ///@{
    const std::string &getHost() const override { return m_host; }
    unsigned int getPort() const override { return 0; }
    ///@}

    /// @brief get a reference to the transform feedback
    /// @return reference to the transform feedback
    auto &getFeedback() { return m_feedback; }

    ~AgentAdapter() override;

    /// @name Source interface
    ///@{
    bool start() override;
    void stop() override;
    pipeline::Pipeline *getPipeline() override { return &m_pipeline; }
    ///@}

    /// @brief get a shared pointer to the source
    /// @return shared pointer to this
    auto getptr() const { return std::dynamic_pointer_cast<AgentAdapter>(Source::getptr()); }

  protected:
    void run();
    void recover();
    bool probe();
    bool current();
    bool sample();
    void assets();
    void updateAssets();

    void streamsFailed(std::error_code &ec);
    void assetsFailed(std::error_code &ec);
    void recoverStreams();
    void recoverAssetRequest();

    void clear();

    auto instanceId() { return m_feedback.m_instanceId; }

    bool canRecover() { return m_feedback.m_instanceId != 0 && m_feedback.m_next != 0; }

  protected:
    pipeline::XmlTransformFeedback m_feedback;
    AgentAdapterPipeline m_pipeline;
    url::Url m_url;
    int m_count = 1000;
    std::chrono::milliseconds m_heartbeat;
    bool m_reconnecting = false;
    bool m_failed = false;
    bool m_stopped = false;
    bool m_usePolling = false;
    bool m_probeAgent = false;

    std::chrono::milliseconds m_reconnectInterval;
    std::chrono::milliseconds m_pollingInterval;
    std::string m_host;
    std::optional<std::string> m_sourceDevice;
    std::string m_feedbackId;
    std::shared_ptr<Session> m_session;
    std::shared_ptr<Session> m_assetSession;
    boost::asio::steady_timer m_reconnectTimer;
    boost::asio::steady_timer m_pollingTimer;
    boost::asio::steady_timer m_assetRetryTimer;

    std::unique_ptr<boost::asio::ssl::context> m_streamContext;
    std::unique_ptr<boost::asio::ssl::context> m_assetContext;

    // Current and Asset Request
    std::optional<Session::Request> m_streamRequest;
    std::optional<Session::Request> m_assetRequest;

    int32_t m_agentVersion = 0;

    // For testing
    bool m_closeConnectionAfterResponse;
  };

}  // namespace mtconnect::source::adapter::agent_adapter
