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

#include "asset/asset.hpp"
#include "observation/observation.hpp"
#include "pipeline/pipeline.hpp"
#include "pipeline/pipeline_context.hpp"
#include "source/source.hpp"
#include "utilities.hpp"

namespace mtconnect::source {
  class LoopbackPipeline : public pipeline::Pipeline
  {
  public:
    LoopbackPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &st)
      : pipeline::Pipeline(context, st)
    {}
    void build(const ConfigOptions &options) override;

  protected:
    ConfigOptions m_options;
  };

  class LoopbackSource : public Source
  {
  public:
    LoopbackSource(const std::string &name, boost::asio::io_context::strand &io,
                   pipeline::PipelineContextPtr pipelineContext, const ConfigOptions &options)
      : Source(name, io), m_pipeline(pipelineContext, Source::m_strand)
    {
      m_pipeline.build(options);
    }

    bool isLoopback() override { return true; }

    bool start() override
    {
      m_pipeline.start();
      return true;
    }
    void stop() override { m_pipeline.clear(); }
    pipeline::Pipeline *getPipeline() override { return &m_pipeline; }

    SequenceNumber_t receive(observation::ObservationPtr observation)
    {
      auto res = m_pipeline.run(observation);
      if (auto obs = std::dynamic_pointer_cast<observation::Observation>(res))
      {
        return obs->getSequence();
      }
      else
      {
        return 0;
      }
    }
    SequenceNumber_t receive(DataItemPtr dataItem, entity::Properties props,
                             std::optional<Timestamp> timestamp = std::nullopt);
    SequenceNumber_t receive(DataItemPtr dataItem, const std::string &value,
                             std::optional<Timestamp> timestamp = std::nullopt);
    SequenceNumber_t receive(const std::string &shdr);

    void receive(asset::AssetPtr asset) { m_pipeline.run(asset); }
    asset::AssetPtr receiveAsset(DevicePtr device, const std::string &document,
                                 const std::optional<std::string> &id,
                                 const std::optional<std::string> &type,
                                 const std::optional<std::string> &time, entity::ErrorList &errors);
    void removeAsset(const std::optional<std::string> device, const std::string &id);

  protected:
    LoopbackPipeline m_pipeline;
  };
}  // namespace mtconnect::source
