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

#include <chrono>
#include <regex>

#include "entity/entity.hpp"
#include "observation/observation.hpp"
#include "pipeline/transform.hpp"
#include "response_document.hpp"

namespace mtconnect::pipeline {
  class InstanceIdChanged : public std::runtime_error
  {
  public:
    explicit InstanceIdChanged(const char *msg, int old, int nid) 
    : std::runtime_error(msg), m_instanceId(nid), m_oldInstanceId(old) {}
    explicit InstanceIdChanged(const std::string &msg, int old, int nid)
    : std::runtime_error(msg), m_instanceId(nid), m_oldInstanceId(old) {}

    int m_instanceId;
    int m_oldInstanceId;
  };
  
  struct XmlTransformFeedback : public pipeline::TransformState
  {
    int m_instanceId = 0;
    SequenceNumber_t m_next = 0;
    entity::EntityList m_assetEvents;
  };

  using namespace mtconnect::entity;
  class MTConnectXmlTransform : public Transform
  {
  public:
    MTConnectXmlTransform(const MTConnectXmlTransform &) = default;
    MTConnectXmlTransform(PipelineContextPtr context,
                          const std::optional<std::string> &device = std::nullopt)
      : Transform("MTConnectXmlTransform"), m_context(context), m_defaultDevice(device)
    {
      m_guard = EntityNameGuard("Data", RUN);
    }

    const EntityPtr operator()(const EntityPtr entity) override
    {
      using namespace pipeline;
      using namespace entity;

      const auto &data = entity->getValue<std::string>();
      ResponseDocument rd;
      ResponseDocument::parse(data, rd, m_context);

      auto feedback = m_context->getSharedState<XmlTransformFeedback>("XmlTransformFeedback");
      
      if (feedback->m_instanceId != 0 && feedback->m_instanceId != rd.m_instanceId)
      {
        LOG(warning) << "MTConnectXmlTransform: instance id changed";
        throw InstanceIdChanged("Instance Id Changed", feedback->m_instanceId, rd.m_instanceId);
      }
      
      feedback->m_next = rd.m_next;
      feedback->m_assetEvents = rd.m_assetEvents;

      for (auto &entity : rd.m_entities)
      {
        next(entity);
      }

      return std::make_shared<Entity>("Entities", Properties {{"VALUE", rd.m_entities}});
    }

  protected:
    PipelineContextPtr m_context;
    std::optional<std::string> m_defaultDevice;
  };
}  // namespace mtconnect::pipeline
