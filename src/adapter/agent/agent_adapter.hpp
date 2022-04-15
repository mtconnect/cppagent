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

#include "adapter/adapter.hpp"
#include "adapter/adapter_pipeline.hpp"
#include "session.hpp"
#include "url_parser.hpp"

namespace mtconnect::adapter::agent {
  using namespace mtconnect;
  using namespace adapter;

  class AgentAdapterPipeline : public AdapterPipeline
  {
  public:
    using AdapterPipeline::AdapterPipeline;
    void build(const ConfigOptions &options) override;
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
                   const boost::property_tree::ptree &block) -> SourcePtr {
        auto source = std::make_shared<AgentAdapter>(io, context, options, block);
        return source;
      };
      factory.registerFactory("http", cb);
      factory.registerFactory("https", cb);
    }

    const std::string &getHost() const override { return m_host; }

    unsigned int getPort() const override { return 0; }

    ~AgentAdapter() override;

    bool start() override;
    void stop() override;
    pipeline::Pipeline *getPipeline() override { return &m_pipeline; }

  protected:
    void current();
    bool sample();
    void assets();
    void assetUpdated(const entity::EntityList &entity);


  protected:
    AgentAdapterPipeline m_pipeline;
    Url m_url;
    int m_count;
    int m_heartbeat;
    std::string m_host;
    std::shared_ptr<Session> m_session;
    std::shared_ptr<Session> m_assetSession;
  };

}  // namespace mtconnect::adapter::agent
