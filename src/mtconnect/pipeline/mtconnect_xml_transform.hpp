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

#include <chrono>
#include <regex>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/transform.hpp"
#include "mtconnect/source/error_code.hpp"
#include "response_document.hpp"

namespace mtconnect::pipeline {
  /// @brief Used to manage Agent streaming protocol from upstream agent
  struct XmlTransformFeedback
  {
    uint64_t m_instanceId = 0;
    int32_t m_agentVersion = 0;
    SequenceNumber_t m_next = 0;
    entity::EntityList m_assetEvents;
    ResponseDocument::Errors m_errors;
  };

  using namespace mtconnect::entity;

  /// @brief Transform, parse, and map the XML documents extracting the data for feedback
  class AGENT_LIB_API MTConnectXmlTransform : public Transform
  {
  public:
    MTConnectXmlTransform(const MTConnectXmlTransform &) = default;

    /// @brief Construct a transfor,
    /// @param context the pipeline context
    /// @param feedback a feedback object to pass back protocol info
    /// @param device an associated device
    MTConnectXmlTransform(PipelineContextPtr context, XmlTransformFeedback &feedback,
                          const std::optional<std::string> &device = std::nullopt,
                          const std::optional<std::string> &uuid = std::nullopt)
      : Transform("MTConnectXmlTransform"),
        m_context(context),
        m_defaultDevice(device),
        m_uuid(uuid),
        m_feedback(feedback)
    {
      m_guard = EntityNameGuard("Data", RUN);
    }

    EntityPtr operator()(EntityPtr &&entity) override
    {
      using namespace pipeline;
      using namespace entity;
      using namespace mtconnect::source;

      const auto &data = entity->getValue<std::string>();
      ResponseDocument rd;
      ResponseDocument::parse(data, rd, m_context, m_defaultDevice, m_uuid);

      if (m_feedback.m_instanceId != 0 && m_feedback.m_instanceId != rd.m_instanceId)
      {
        m_feedback.m_assetEvents.clear();
        m_feedback.m_errors.clear();

        LOG(warning) << "MTConnectXmlTransform: instance id changed from "
                     << m_feedback.m_instanceId << " to " << rd.m_instanceId;
        throw std::system_error(make_error_code(ErrorCode::INSTANCE_ID_CHANGED));
      }

      m_feedback.m_instanceId = rd.m_instanceId;
      m_feedback.m_agentVersion = rd.m_agentVersion;
      m_feedback.m_next = rd.m_next;
      m_feedback.m_assetEvents = rd.m_assetEvents;
      m_feedback.m_errors = rd.m_errors;

      if (rd.m_errors.size() > 0)
      {
        throw std::system_error(make_error_code(ErrorCode::RESTART_STREAM));
      }

      if (rd.m_enityType == ResponseDocument::DEVICE)
      {
        auto e = std::make_shared<Entity>(*entity);
        e->setName("Devices");
        e->setValue(rd.m_entities);
        next(std::move(e));
      }
      else
      {
        for (auto &entity : rd.m_entities)
        {
          next(std::move(entity));
        }
      }
      return std::make_shared<Entity>("Entities", Properties {{"VALUE", rd.m_entities}});
    }

  protected:
    PipelineContextPtr m_context;
    std::optional<std::string> m_defaultDevice;
    std::optional<std::string> m_uuid;
    XmlTransformFeedback &m_feedback;
  };
}  // namespace mtconnect::pipeline
