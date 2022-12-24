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

#include <future>

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

  namespace source::adapter {
    class Adapter;
  }  // namespace source::adapter
  namespace pipeline {
    class Pipeline
    {
    public:
      using Splice = std::function<void(Pipeline *)>;

      Pipeline(PipelineContextPtr context, boost::asio::io_context::strand &st)
        : m_start(std::make_shared<Start>()), m_context(context), m_strand(st)
      {}
      virtual ~Pipeline() { m_start->stop(); }
      virtual void build(const ConfigOptions &options) = 0;
      bool started() const { return m_started; }
      boost::asio::io_context::strand &getStrand() { return m_strand; }

      void applySplices()
      {
        for (auto &splice : m_splices)
        {
          splice(this);
        }
      }

      void clear()
      {
        using namespace std::chrono_literals;
        if (m_start->getNext().size() > 0)
        {
          if (m_strand.context().stopped())
          {
            clearTransforms();
          }
          else
          {
            std::promise<void> p;
            auto f = p.get_future();
            m_strand.dispatch([this, &p]() {
              clearTransforms();
              p.set_value();
            });

            while (f.wait_for(1ms) != std::future_status::ready)
            {
              m_strand.context().run_for(10ms);
            }
            f.get();
          }
        }
      }

      virtual void start()
      {
        if (m_start)
        {
          m_start->start(m_strand);
          m_started = true;
        }
      }

      bool spliceBefore(const std::string &target, TransformPtr transform, bool reapplied = false)
      {
        Transform::ListOfTransforms xforms;
        m_start->find(target, xforms);
        if (xforms.empty())
          return false;

        transform->unlink();
        for (auto &pair : xforms)
        {
          pair.first->spliceBefore(pair.second, transform);
        }

        if (!reapplied)
        {
          m_splices.emplace_back(
              [target, transform](Pipeline *pipe) { pipe->spliceBefore(target, transform, true); });
        }

        return true;
      }
      bool spliceAfter(const std::string &target, TransformPtr transform, bool reapplied = false)
      {
        Transform::ListOfTransforms xforms;
        m_start->find(target, xforms);
        if (xforms.empty())
          return false;

        transform->unlink();
        for (auto &pair : xforms)
        {
          pair.second->spliceAfter(transform);
        }

        if (!reapplied)
        {
          m_splices.emplace_back(
              [target, transform](Pipeline *pipe) { pipe->spliceAfter(target, transform, true); });
        }

        return true;
      }
      bool firstAfter(const std::string &target, TransformPtr transform, bool reapplied = false)
      {
        Transform::ListOfTransforms xforms;
        m_start->find(target, xforms);
        if (xforms.empty())
          return false;

        for (auto &pair : xforms)
        {
          pair.second->firstAfter(transform);
        }

        if (!reapplied)
        {
          m_splices.emplace_back(
              [target, transform](Pipeline *pipe) { pipe->firstAfter(target, transform, true); });
        }
        return true;
      }

      bool lastAfter(const std::string &target, TransformPtr transform, bool reapplied = false)
      {
        Transform::ListOfTransforms xforms;
        m_start->find(target, xforms);
        if (xforms.empty())
          return false;

        for (auto &pair : xforms)
        {
          pair.second->bind(transform);
        }

        if (!reapplied)
        {
          m_splices.emplace_back(
              [target, transform](Pipeline *pipe) { pipe->lastAfter(target, transform, true); });
        }
        return true;
      }

      bool replace(const std::string &target, TransformPtr transform, bool reapplied = false)
      {
        Transform::ListOfTransforms xforms;
        m_start->find(target, xforms);
        if (xforms.empty())
          return false;

        transform->unlink();
        for (auto &pair : xforms)
        {
          pair.first->replace(pair.second, transform);
        }

        if (!reapplied)
        {
          m_splices.emplace_back(
              [target, transform](Pipeline *pipe) { pipe->replace(target, transform, true); });
        }

        return true;
      }

      bool remove(const std::string &target)
      {
        Transform::ListOfTransforms xforms;
        m_start->find(target, xforms);
        if (xforms.empty())
          return false;

        for (auto &pair : xforms)
        {
          pair.first->remove(pair.second);
        }

        m_splices.emplace_back([target](Pipeline *pipe) { pipe->remove(target); });

        return true;
      }

      const entity::EntityPtr run(const entity::EntityPtr entity) { return m_start->next(entity); }

      TransformPtr bind(TransformPtr transform)
      {
        m_start->bind(transform);
        return transform;
      }

      bool hasContext() const { return bool(m_context); }
      bool hasContract() const { return bool(m_context) && bool(m_context->m_contract); }
      PipelineContextPtr getContext() { return m_context; }
      const auto &getContract() { return m_context->m_contract; }

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

      void clearTransforms()
      {
        m_start->stop();
        m_started = false;
        m_start->clear();
        m_start = std::make_shared<Start>();
      }

      bool m_started {false};
      TransformPtr m_start;
      PipelineContextPtr m_context;
      boost::asio::io_context::strand m_strand;
      std::list<Splice> m_splices;
    };
  }  // namespace pipeline
}  // namespace mtconnect
