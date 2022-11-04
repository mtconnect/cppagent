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

#include "asset/asset.hpp"
#include "observation/observation.hpp"
#include "transform.hpp"

namespace mtconnect {
  namespace pipeline {
    class ConvertSample : public Transform
    {
    public:
      ConvertSample() : Transform("ConvertSample")
      {
        using namespace observation;
        m_guard = TypeGuard<Sample>(RUN) || TypeGuard<Observation>(SKIP);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override
      {
        using namespace observation;
        using namespace entity;
        auto sample = std::dynamic_pointer_cast<Sample>(entity);
        if (sample && !sample->isOrphan() && !sample->isUnavailable())
        {
          auto &converter = sample->getDataItem()->getConverter();
          if (converter)
          {
            auto ns = sample->copy();
            converter->convertValue(ns->getValue());
            return next(ns);
          }
        }
        return next(entity);
      }
    };
  }  // namespace pipeline
}  // namespace mtconnect
