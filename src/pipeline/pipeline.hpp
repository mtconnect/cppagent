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

#include "pipeline_context.hpp"
#include "pipeline_contract.hpp"
#include "transform.hpp"

namespace mtconnect {
class Agent;
namespace device_model {
class Device;
}
namespace asset {
class Asset;
using AssetPtr = std::shared_ptr<Asset>;
}  // namespace asset

namespace adapter {
class Adapter;
}  // namespace adapter
namespace pipeline {
class Pipeline
{
public:
  Pipeline(PipelineContextPtr context) : m_start(std::make_shared<Start>()), m_context(context) {}
  virtual ~Pipeline() { m_start->stop(); }
  virtual void build(const ConfigOptions &options) = 0;
  bool started() const { return m_started; }
  void clear()
  {
    m_start->stop();
    m_started = false;
    m_start = std::make_shared<Start>();
  }
  virtual void start(boost::asio::io_context::strand &st)
  {
    if (m_start)
    {
      m_start->start(st);
      m_started = true;
    }
  }

  const entity::EntityPtr run(const entity::EntityPtr entity) { return m_start->next(entity); }

  TransformPtr bind(TransformPtr transform)
  {
    m_start->bind(transform);
    return transform;
  }

  bool hasContext() const { return bool(m_context); }
  bool hasContract() const { return bool(m_context) && bool(m_context->m_contract); }

protected:
  class Start : public Transform
  {
  public:
    Start() : Transform("Start")
    {
      m_guard = [](const entity::EntityPtr entity) { return SKIP; };
    }
    ~Start() override = default;

    const entity::EntityPtr operator()(const entity::EntityPtr entity) override
    {
      return entity::EntityPtr();
    }
  };

  bool m_started {false};
  TransformPtr m_start;
  PipelineContextPtr m_context;
};
}  // namespace pipeline
}  // namespace mtconnect
