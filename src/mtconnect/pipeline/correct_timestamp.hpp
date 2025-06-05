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

#include "mtconnect/config.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  /// @brief Filter duplicates
  class AGENT_LIB_API CorrectTimestamp : public Transform
  {
  protected:
    struct State : TransformState
    {
      std::unordered_map<std::string, Timestamp> m_timestamps;
    };

  public:
    CorrectTimestamp(const CorrectTimestamp &) = default;
    /// @brief Create a duplicate filter with shared state from the context
    /// @param context the context
    CorrectTimestamp(PipelineContextPtr context)
      : Transform("ValidateTimestamp"),
        m_context(context),
        m_state(context->getSharedState<State>(m_name))
    {
      m_guard = TypeGuard<observation::Observation>(RUN);
    }
    ~CorrectTimestamp() override = default;

    /// @brief check if the entity is a duplicate
    /// @param[in] entity the entity to check
    /// @return the result of the transform if not a duplicate or an empty entity
    entity::EntityPtr operator()(entity::EntityPtr &&entity) override
    {
      using namespace observation;

      auto obs = std::dynamic_pointer_cast<Observation>(entity);
      if (obs->isOrphan())
        return entity::EntityPtr();

      auto di = obs->getDataItem();
      auto &id = di->getId();
      auto ts = obs->getTimestamp();

      std::lock_guard<TransformState> guard(*m_state);

      auto last = m_state->m_timestamps.find(id);
      if (last != m_state->m_timestamps.end())
      {
        if (ts < last->second)
        {
          LOG(debug) << "Observation for data item " << id << " has timestamp " << format(ts)
                     << " thats is before " << format(last->second);

          // Set the timestamp to now.
          ts = std::chrono::system_clock::now();
          obs->setTimestamp(ts);
        }
      }

      m_state->m_timestamps.insert_or_assign(id, ts);

      return next(std::move(obs));
    }

  protected:
    PipelineContextPtr m_context;
    std::shared_ptr<State> m_state;
  };
}  // namespace mtconnect::pipeline
