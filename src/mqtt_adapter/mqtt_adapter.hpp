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

#include "source.hpp"
#include "pipeline/pipeline.hpp"

namespace mtconnect
{
  namespace source
  {
    class MqttAdapterImpl;
    
    class MqttPipeline : public pipeline::Pipeline
    {
    public:
      MqttPipeline(pipeline::PipelineContextPtr context) : Pipeline(context) {}

      const auto &getContract() { return m_context->m_contract; }
      
      void build(const ConfigOptions &options) override;

    protected:
      ConfigOptions m_options;
    };

    class MqttAdapter : public Source
    {
    public:
      MqttAdapter(boost::asio::io_context &context, const ConfigOptions &options,
                  std::unique_ptr<MqttPipeline> &pipeline);
      ~MqttAdapter() override;
      
      bool start() override;
      void stop() override;
      
    protected:
      boost::asio::io_context &m_ioContext;
      ConfigOptions m_options;
      
      // If the connector has been running
      bool m_running;
      
      std::unique_ptr<MqttPipeline> m_pipeline;
      std::shared_ptr<MqttAdapterImpl> m_client;
    };
  };
}

      
      
