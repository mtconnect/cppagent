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

#include "transform.hpp"
#include "observation/observation.hpp"
#include "pipeline_contract.hpp"

namespace mtconnect
{
  class Agent;
  class Device;
  class DataItem;
  class Asset;
  using AssetPtr = std::shared_ptr<Asset>;
    
  namespace adapter
  {
    class Adapter;
    struct Handler;
  }
  namespace pipeline
  {    
    class PipelineContext : public std::enable_shared_from_this<PipelineContext>
    {
    public:
      auto getptr() { return shared_from_this(); }
      
      template<typename T>
      std::shared_ptr<T> getSharedState(const std::string &name)
      {
        auto &state = m_sharedState[name];
        if (!state)
          state = std::make_shared<T>();
        return std::dynamic_pointer_cast<T>(state);
      }
            
      std::unique_ptr<PipelineContract> m_contract;

    protected:
      using SharedState = std::unordered_map<std::string, TransformStatePtr>;
      SharedState m_sharedState;
    };
    using PipelineContextPtr = std::shared_ptr<PipelineContext>;
    
    class Pipeline
    {
    public:
      Pipeline(const ConfigOptions &options, PipelineContextPtr context)
      : m_start(std::make_shared<Start>()),
        m_options(options), m_context(context)
      {}
      virtual ~Pipeline() = default;
      virtual void build() = 0;
      
      const entity::EntityPtr run(const entity::EntityPtr entity)
      {
        return m_start->next(entity);
      }
      
      TransformPtr bind(TransformPtr transform)
      {
        m_start->bind(transform);
        return transform;
      }
      
    protected:
      class Start : public Transform
      {
      public:
        Start()
        : Transform("Start")
        {
          m_guard = [](const entity::EntityPtr entity) { return SKIP; };
        }
        ~Start() override = default;

        const entity::EntityPtr operator()(const entity::EntityPtr entity) override
        {
          return entity::EntityPtr();
        }
      };
      
      TransformPtr m_start;
      ConfigOptions m_options;
      PipelineContextPtr m_context;
    };    
  }
}  // namespace mtconnect
