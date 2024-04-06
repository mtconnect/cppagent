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
#include "mtconnect/observation/observation.hpp"
#include "transform.hpp"

namespace mtconnect {
  class Agent;
  namespace pipeline {
    /// @brief Provide MTConnect DataItem delta filter behavior
    class AGENT_LIB_API DeltaFilter : public Transform
    {
    public:
      /// @brief shared values associated with data items
      struct State : TransformState
      {
        std::unordered_map<std::string, double> m_lastSampleValue;
      };

      /// @brief Construct a delta filter
      /// @param[in] context the context for shared state
      DeltaFilter(PipelineContextPtr context)
        : Transform("DeltaFilter"),
          m_state(context->getSharedState<State>(m_name)),
          m_contract(context->m_contract.get())
      {
        using namespace observation;
        constexpr static auto lambda = [](const Sample &s) {
          return bool(!s.isOrphan() && s.getDataItem()->getMinimumDelta());
        };
        m_guard = LambdaGuard<Sample, ExactTypeGuard<Sample>>(lambda, RUN) ||
                  TypeGuard<Observation>(SKIP);
      }

      ~DeltaFilter() override = default;

      entity::EntityPtr operator()(entity::EntityPtr &&entity) override
      {
        using namespace std;
        using namespace observation;
        using namespace entity;

        std::lock_guard<TransformState> guard(*m_state);

        auto o = std::dynamic_pointer_cast<Observation>(entity);
        if (o->isOrphan())
          return EntityPtr();
        auto di = o->getDataItem();
        auto &id = di->getId();

        if (o->isUnavailable())
        {
          m_state->m_lastSampleValue.erase(id);
          return next(std::move(entity));
        }

        auto filter = *di->getMinimumDelta();
        double value = o->getValue<double>();
        if (filterMinimumDelta(id, value, filter))
          return EntityPtr();

        return next(std::move(entity));
      }

    protected:
      bool filterMinimumDelta(const std::string &id, const double value, const double fv)
      {
        auto last = m_state->m_lastSampleValue.find(id);
        if (last != m_state->m_lastSampleValue.end())
        {
          double lv = last->second;
          if (value > (lv - fv) && value < (lv + fv))
          {
            return true;
          }
          last->second = value;
        }
        else
        {
          m_state->m_lastSampleValue[id] = value;
        }

        return false;
      }

    protected:
      std::shared_ptr<State> m_state;
      PipelineContract *m_contract;
    };
  }  // namespace pipeline
}  // namespace mtconnect
