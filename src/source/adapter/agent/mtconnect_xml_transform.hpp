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

namespace mtconnect::source::adapter::agent {
  using namespace mtconnect::entity;
  using namespace mtconnect::pipeline;
  class MTConnectXmlTransform : public Transform
  {
  public:
    MTConnectXmlTransform(const MTConnectXmlTransform &) = default;
    MTConnectXmlTransform(PipelineContextPtr context, Handler *handler,
                          const std::optional<std::string> &device = std::nullopt)
      : Transform("MTConnectXmlTransform"),
        m_handler(handler),
        m_context(context),
        m_defaultDevice(device)
    {
      m_guard = EntityNameGuard("Data", RUN);
    }

    const EntityPtr operator()(const EntityPtr entity) override
    {
      using namespace pipeline;
      using namespace entity;

      const auto &data = entity->getValue<string>();
      ResponseDocument rd;
      ResponseDocument::parse(data, rd, m_context);

      auto seq = m_context->getSharedState<NextSequence>("next");
      seq->m_next = rd.m_next;

      for (auto &entity : rd.m_entities)
      {
        next(entity);
      }

      if (!rd.m_assetEvents.empty() && m_handler && m_handler->m_assetUpdated)
      {
        m_handler->m_assetUpdated(rd.m_assetEvents);
      }

      return std::make_shared<Entity>("Entities", Properties {{"VALUE", rd.m_entities}});
    }

  protected:
    Handler *m_handler = nullptr;
    PipelineContextPtr m_context;
    std::optional<std::string> m_defaultDevice;
  };
}  // namespace mtconnect::source::adapter::agent
