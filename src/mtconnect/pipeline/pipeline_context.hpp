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

#include <memory>
#include <mutex>
#include <unordered_map>

#include "mtconnect/config.hpp"
#include "pipeline_contract.hpp"

namespace mtconnect::pipeline {
  /// @brief Base class for all shared state used by the `PipelineContext`
  struct TransformState
  {
    /// @brief Forwards to `std::mutex` `lock()`
    /// @return void
    auto lock() { return m_mutex.lock(); }
    /// @brief Forwards to `std::mutex` `unlock()`
    /// @return void
    auto unlock() { return m_mutex.unlock(); }
    /// @brief Forwards to `std::mutex` `try_lock()`
    /// @return `true` if sucessful
    auto try_lock() { return m_mutex.try_lock(); }

    /// @brief The mutex to lock for synchronized access
    std::mutex m_mutex;
    virtual ~TransformState() {}
  };
  using TransformStatePtr = std::shared_ptr<TransformState>;

  /// @brief Manages shared state across multiple pipelines
  ///
  /// Used for cases like duplicate detection and shared counters.
  class AGENT_LIB_API PipelineContext : public std::enable_shared_from_this<PipelineContext>
  {
  public:
    auto getptr() { return shared_from_this(); }

    /// @brief Retrieves the shared state for a given name.
    /// @tparam T the type of the shared state. Must be a subclass of TransformState or a lockable
    /// object.
    /// @param[in] name the name of the shared state
    /// @return a shared pointer to the shared state.
    template <typename T>
    std::shared_ptr<T> getSharedState(const std::string &name)
    {
      auto &state = m_sharedState[name];
      if (!state)
        state = std::make_shared<T>();
      return std::dynamic_pointer_cast<T>(state);
    }

    /// @brief A pipeline contract that can be used by the shared state.
    std::unique_ptr<PipelineContract> m_contract;

  protected:
    using SharedState = std::unordered_map<std::string, TransformStatePtr>;
    SharedState m_sharedState;
  };

  /// @brief Alias for a shared pointer to the pipeline context
  using PipelineContextPtr = std::shared_ptr<PipelineContext>;
}  // namespace mtconnect::pipeline
