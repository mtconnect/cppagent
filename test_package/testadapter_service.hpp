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

#include <boost/dll/alias.hpp>

#include "mtconnect/configuration/agent_config.hpp"
#include "mtconnect/pipeline/pipeline.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/adapter_pipeline.hpp"
#include "mtconnect/source/source.hpp"

using namespace std;

namespace mtconnect {
  using namespace pipeline;
  class adapter_plugin_test : public source::Source
  {
  public:
    adapter_plugin_test(const std::string &name, boost::asio::io_context &io,
                        pipeline::PipelineContextPtr pipelineContext, const ConfigOptions &options,
                        const boost::property_tree::ptree &block)
      : Source(name, io), m_pipeline(pipelineContext, m_strand)

    {
      m_pipeline.build(options);
    }

    ~adapter_plugin_test() = default;

    bool start() override
    {
      m_pipeline.start();
      return true;
    }
    void stop() override { m_pipeline.clear(); }

    // Factory method
    static source::SourcePtr create(const std::string &name, boost::asio::io_context &io,
                                    pipeline::PipelineContextPtr pipelineContext,
                                    const ConfigOptions &options,
                                    const boost::property_tree::ptree &block)
    {
      return std::make_shared<adapter_plugin_test>(name, io, pipelineContext, options, block);
    }

    static void register_factory(const boost::property_tree::ptree &block,
                                 configuration::AgentConfiguration &config)
    {
      mtconnect::configuration::gAgentLogger = config.getLogger();
      PLUGIN_LOG(debug) << "Registering adapter factory for adapter_plugin_test";
      config.getSourceFactory().registerFactory("adapter_plugin_test",
                                                &adapter_plugin_test::create);
    }

    Pipeline *getPipeline() override { return &m_pipeline; }

  protected:
    source::adapter::AdapterPipeline m_pipeline;
  };

  BOOST_DLL_ALIAS(adapter_plugin_test::register_factory,
                  initialize_plugin  // <-- ...this alias name
  )
}  // namespace mtconnect
