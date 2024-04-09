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

#include <future>

#include "mtconnect/config.hpp"
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

  /// @brief mtconnect pipelines and transformations
  ///
  /// Contains all classes pertaining to pipeline transformations
  namespace pipeline {
    /// @brief Abstract Pipeline class
    ///
    /// Must be subclassed and the `build()` method must be provided
    class AGENT_LIB_API Pipeline
    {
    public:
      /// @brief A splice function type for resplicing the pipeline after it is rebuilt
      using Splice = std::function<void(Pipeline *)>;

      /// @brief Pipeline constructor
      /// @param context The pipeline context
      /// @param st boost asio strand for for setting timers and running async operations
      /// @note All pipelines run in a single strand (thread) and therefor all operations are
      ///       thread-safe in one pipeline.

      Pipeline(PipelineContextPtr context, boost::asio::io_context::strand &st)
        : m_start(std::make_shared<Start>()), m_context(context), m_strand(st)
      {}
      /// @brief Destructor stops the pipeline
      virtual ~Pipeline() { m_start->stop(); }
      /// @brief Build the pipeline
      /// @param options A set of configuration options
      virtual void build(const ConfigOptions &options) = 0;
      /// @brief Has the pipeline started?
      /// @return `true` if started
      bool started() const { return m_started; }
      /// @brief Get a reference to the strand
      /// @return the strand
      boost::asio::io_context::strand &getStrand() { return m_strand; }

      /// @brief Apply the splices after rebuilding
      void applySplices()
      {
        for (auto &splice : m_splices)
        {
          splice(this);
        }
      }

      /// @brief remove all transforms from the pipeline
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

      /// @brief start all the transforms that require asynchronous operations
      virtual void start()
      {
        if (m_start)
        {
          m_start->start(m_strand);
          m_started = true;
        }
      }

      /// @brief Find all transforms that match the target
      /// @param[in] target the named transforms to find
      /// @return a list of all matching transforms
      Transform::ListOfTransforms find(const std::string &target)
      {
        Transform::ListOfTransforms xforms;
        m_start->find(target, xforms);
        return xforms;
      }

      /// @brief add a transform before the target.
      /// @param[in] target the named transforms to insert this transform before
      /// @param[in] transform the transform to add before
      /// @param[in] reapplied `true` if the pipeline is being rebuilt
      /// @returns `true` if the target is found and spliced
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

      /// @brief add a transform after the target.
      /// @param[in] target the named transforms to insert this transform after
      /// @param[in] transform the transform to add before
      /// @param[in] reapplied `true` if the pipeline is being rebuilt
      /// @returns `true` if the target is found and spliced
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

      /// @brief splices the transform as the first option in targets next list.
      /// @param[in] target the named transforms to insert this transform after
      /// @param[in] transform the transform to add before
      /// @param[in] reapplied `true` if the pipeline is being rebuilt
      /// @returns `true` if the target is found and spliced
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

      /// @brief splices the transform as the last option in targets next list.
      /// @param[in] target the named transforms to insert this transform after
      /// @param[in] transform the transform to add before
      /// @param[in] reapplied `true` if the pipeline is being rebuilt
      /// @returns `true` if the target is found and spliced
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

      /// @brief replaces each occurence of target with transform.
      /// @param[in] target the named transforms to replace
      /// @param[in] transform the transform to add before
      /// @param[in] reapplied `true` if the pipeline is being rebuilt
      /// @returns `true` if the target is found and spliced
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

      /// @brief removes the named transform.
      /// @param[in] target the named transforms to replace
      /// @returns `true` if the target is found and spliced
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

      /// @brief Sends the entity through the pipeline
      /// @param[in] entity the entity to send through the pipeline
      /// @return the entity returned from the transform
      entity::EntityPtr run(entity::EntityPtr &&entity) { return m_start->next(std::move(entity)); }

      /// @brief Bind the transform to the start
      /// @param[in] transform the transform to bind
      /// @return returns `transform`
      TransformPtr bind(TransformPtr transform)
      {
        m_start->bind(transform);
        return transform;
      }

      /// @brief check if the pipeline has a pipeline context
      /// @returns `true` if there is a context
      bool hasContext() const { return bool(m_context); }

      /// @brief check if the pipeline has a pipeline contract
      /// @returns `true` if there is a contract
      bool hasContract() const { return bool(m_context) && bool(m_context->m_contract); }
      /// @brief gets the pipeline context
      /// @returns a shared pointer to the pipeline context
      PipelineContextPtr getContext() { return m_context; }
      /// @brief gets the pipeline contract
      /// @returns the pipeline contract
      const auto &getContract() { return m_context->m_contract; }

    protected:
      class AGENT_LIB_API Start : public Transform
      {
      public:
        Start() : Transform("Start")
        {
          m_guard = [](const entity::Entity *entity) { return SKIP; };
        }
        ~Start() override = default;

        entity::EntityPtr operator()(entity::EntityPtr &&entity) override
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
