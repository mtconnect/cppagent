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

#include "mtconnect/config.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  /// @brief Filter duplicates
  class AGENT_LIB_API DuplicateFilter : public Transform
  {
  public:
    /// @brief Shared states to check for duplicates
    struct State : TransformState
    {
      std::unordered_map<std::string, entity::Value> m_values;
    };

    DuplicateFilter(const DuplicateFilter &) = default;
    /// @brief Create a duplicate filter with shared state from the context
    /// @param context the context
    DuplicateFilter(PipelineContextPtr context) : Transform("DuplicateFilter"), m_context(context)
    {
      using namespace observation;
      static constexpr auto lambda = [](const Observation &o) {
        return !o.isOrphan() && !o.getDataItem()->isDiscrete() && !o.getDataItem()->isDataSet();
      };
      m_guard = LambdaGuard<Observation, TypeGuard<Observation>>(lambda, RUN) ||
                TypeGuard<Observation>(SKIP);
    }
    ~DuplicateFilter() override = default;

    const entity::EntityPtr operator()(const entity::EntityPtr entity) override
    {
      using namespace observation;

      auto o = std::dynamic_pointer_cast<Observation>(entity);
      if (o->isOrphan())
        return entity::EntityPtr();

      if (m_context->m_contract->isDuplicate(o))
        return entity::EntityPtr();
      else
        return next(entity);
    }

  protected:
    PipelineContextPtr m_context;
  };
}  // namespace mtconnect::pipeline
