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

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/pipeline.hpp"
#include "mtconnect/pipeline/pipeline_context.hpp"
#include "mtconnect/source/source.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::source {
  /// @brief A pipeline for a loopback source
  class AGENT_LIB_API LoopbackPipeline : public pipeline::Pipeline
  {
  public:
    /// @brief Create a loopback pipeline
    /// @param[in] context pipeline context
    /// @param[in] st boost asio strand
    LoopbackPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &st)
      : pipeline::Pipeline(context, st)
    {}
    /// @brief build the pipeline
    /// @param options configuration options
    void build(const ConfigOptions &options) override;

  protected:
    ConfigOptions m_options;
  };

  /// @brief Loopback source for sending entities back to the agent
  class AGENT_LIB_API LoopbackSource : public Source
  {
  public:
    /// @brief Create a loopback source
    /// @param name the name of the source
    /// @param io boost asio strand
    /// @param pipelineContext pipeline context
    /// @param options loopback source options
    LoopbackSource(const std::string &name, boost::asio::io_context::strand &io,
                   pipeline::PipelineContextPtr pipelineContext, const ConfigOptions &options)
      : Source(name, io), m_pipeline(pipelineContext, Source::m_strand)
    {
      m_pipeline.build(options);
    }

    /// @brief this is a loopback source
    /// @return always `true`
    bool isLoopback() override { return true; }

    bool start() override
    {
      m_pipeline.start();
      return true;
    }
    void stop() override { m_pipeline.clear(); }
    pipeline::Pipeline *getPipeline() override { return &m_pipeline; }

    /// @brief send an observation running it through the pipeline
    /// @param observation the observation
    /// @return the sequence number
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
    /// @brief create and send an observation through the pipeline
    /// @param dataItem the observation's data item
    /// @param props observation properties
    /// @param timestamp optional observation timestamp
    /// @return the sequence number
    SequenceNumber_t receive(DataItemPtr dataItem, entity::Properties props,
                             std::optional<Timestamp> timestamp = std::nullopt);
    /// @brief create and send an observation through the pipeline
    /// @param dataItem the observation's data item
    /// @param value simple string value
    /// @param timestamp optional observation timestamp
    /// @return the sequence number
    SequenceNumber_t receive(DataItemPtr dataItem, const std::string &value,
                             std::optional<Timestamp> timestamp = std::nullopt);
    /// @brief create and send an observation with shdr through the pipeline
    /// @param shdr shdr pipe deliminated text
    /// @return the sequence number
    SequenceNumber_t receive(const std::string &shdr);

    /// @brief receives a device and sends it to the sinks
    /// @param device the device to be received
    /// @return 0 since there is no sequence number for this.
    void receive(DevicePtr device);

    /// @brief send an asset through the pipeline
    /// @param asset the asset
    void receive(asset::AssetPtr asset) { m_pipeline.run(asset); }
    /// @brief create and send an asset through the pipeline
    /// @param device the device associated with the asset
    /// @param document the asset document
    /// @param id optional asset id
    /// @param type optional asset type
    /// @param time optional asset timestamp
    /// @param[out] errors errors if any occurred
    /// @return shared pointer to the asset
    asset::AssetPtr receiveAsset(DevicePtr device, const std::string &document,
                                 const std::optional<std::string> &id,
                                 const std::optional<std::string> &type,
                                 const std::optional<std::string> &time, entity::ErrorList &errors);
    /// @brief set a remove asset command through the pipeline
    /// @param device optional device
    /// @param id the asset id
    void removeAsset(const std::optional<std::string> device, const std::string &id);

  protected:
    LoopbackPipeline m_pipeline;
  };
}  // namespace mtconnect::source
