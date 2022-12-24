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

#include <memory>
#include <mutex>
#include <unordered_map>

#include "pipeline_contract.hpp"

namespace mtconnect {
  namespace pipeline {
    struct TransformState
    {
      // For mutex locking
      auto lock() { return m_mutex.lock(); }
      auto unlock() { return m_mutex.unlock(); }
      auto try_lock() { return m_mutex.try_lock(); }

      std::mutex m_mutex;
      virtual ~TransformState() {}
    };
    using TransformStatePtr = std::shared_ptr<TransformState>;

    class PipelineContext : public std::enable_shared_from_this<PipelineContext>
    {
    public:
      auto getptr() { return shared_from_this(); }

      template <typename T>
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
  }  // namespace pipeline
}  // namespace mtconnect
