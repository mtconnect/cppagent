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

#include "transform.hpp"

namespace mtconnect {
  namespace pipeline {
    class DuplicateFilter : public Transform
    {
    public:
      struct State : TransformState
      {
        std::unordered_map<std::string, entity::Value> m_values;
      };

      DuplicateFilter(const DuplicateFilter &) = default;
      DuplicateFilter(PipelineContextPtr context)
        : Transform("DuplicateFilter"), m_state(context->getSharedState<State>(m_name))
      {
        using namespace observation;
        static constexpr auto lambda = [](const Observation &o) {
          return !o.isOrphan() && !o.getDataItem()->isDiscrete();
        };
        m_guard =
            LambdaGuard<Observation, ExactTypeGuard<Event, Sample, ThreeSpaceSample, Message>>(
                lambda, RUN) ||
            TypeGuard<Observation>(SKIP);
      }
      ~DuplicateFilter() override = default;

      const entity::EntityPtr operator()(const entity::EntityPtr entity) override
      {
        using namespace observation;
        std::lock_guard<TransformState> guard(*m_state);

        auto o = std::dynamic_pointer_cast<Observation>(entity);
        if (o->isOrphan())
          return entity::EntityPtr();

        auto di = o->getDataItem();
        auto &id = di->getId();

        auto &values = m_state->m_values;
        auto old = values.find(id);
        if (old != values.end() && old->second == o->getValue())
          return entity::EntityPtr();

        if (old == values.end())
          values[id] = o->getValue();
        else
          old->second = o->getValue();

        return next(entity);
      }

    protected:
      std::shared_ptr<State> m_state;
    };

  }  // namespace pipeline

}  // namespace mtconnect
