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

#include "pipeline/mtconnect_xml_transform.hpp"
#include "session.hpp"
#include "source/adapter/adapter.hpp"
#include "source/adapter/adapter_pipeline.hpp"
#include "url_parser.hpp"

namespace boost::asio::ssl {
  class context;
}

namespace mtconnect::source::adapter::agent_adapter {
  using namespace mtconnect;
  using namespace source::adapter;

  class AgentAdapterPipeline : public AdapterPipeline
  {
  public:
    AgentAdapterPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &st,
                         pipeline::XmlTransformFeedback &feedback)
      : AdapterPipeline(context, st), m_feedback(feedback)
    {}
    void build(const ConfigOptions &options) override;

    Handler *m_handler = nullptr;
    pipeline::XmlTransformFeedback &m_feedback;
  };

  class AgentAdapter : public Adapter
  {
  public:
    AgentAdapter(boost::asio::io_context &io, pipeline::PipelineContextPtr context,
                 const ConfigOptions &options, const boost::property_tree::ptree &block);

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

    const std::string &getHost() const override { return m_host; }

    unsigned int getPort() const override { return 0; }
    auto &getFeedback() { return m_feedback; }

    ~AgentAdapter() override;

    bool start() override;
    void stop() override;
    pipeline::Pipeline *getPipeline() override { return &m_pipeline; }

    auto getptr() const { return std::dynamic_pointer_cast<AgentAdapter>(Source::getptr()); }

  protected:
    void run();
    void recover();
    void current();
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
    Url m_url;
    int m_count = 1000;
    std::chrono::milliseconds m_heartbeat;
    bool m_reconnecting = false;
    bool m_failed = false;
    bool m_stopped = false;
    bool m_usePolling = false;

    std::chrono::milliseconds m_reconnectInterval;
    std::chrono::milliseconds m_pollingInterval;
    std::string m_host;
    std::string m_sourceDevice;
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

    // For testing
    bool m_closeConnectionAfterResponse;
  };

}  // namespace mtconnect::source::adapter::agent_adapter
