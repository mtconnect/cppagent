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

#include "entity/entity.hpp"
#include "transform.hpp"

namespace mtconnect
{
  class Agent;
  namespace adapter
  {
    class Adapter;
    struct Handler;
  }
  namespace pipeline
  {
    class Pipeline
    {
    public:
      Pipeline(const ConfigOptions &options, Agent *agent)
      : m_agent(agent), m_options(options)
      {}
      virtual ~Pipeline() = default;
      virtual void build() = 0;
      
      const entity::EntityPtr run(const entity::EntityPtr entity)
      {
        return (*m_start)(entity);
      }
      
    protected:
      TransformPtr m_start;
      Agent *m_agent;
      ConfigOptions m_options;
    };
    
    class AdapterPipeline : public Pipeline
    {
      AdapterPipeline(const ConfigOptions &options, Agent *agent, adapter::Adapter *adapter);
      void build() override;

    protected:
      adapter::Adapter *m_adapter;
      std::unique_ptr<adapter::Handler> m_handler;
    };
  }
}  // namespace mtconnect
