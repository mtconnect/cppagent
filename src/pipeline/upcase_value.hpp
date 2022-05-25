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

#include <chrono>
#include <regex>

#include "entity/entity.hpp"
#include "observation/observation.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "transform.hpp"

namespace mtconnect {
  class Device;

  namespace pipeline {
    inline static std::string &upcase(std::string &s)
    {
      std::transform(s.begin(), s.end(), s.begin(),
                     [](unsigned char c) -> unsigned char { return std::toupper(c); });
      return s;
    }

    class UpcaseValue : public Transform
    {
    public:
      UpcaseValue(const UpcaseValue &) = default;
      UpcaseValue() : Transform("UpcaseValue")
      {
        using namespace observation;
        m_guard = ExactTypeGuard<Event>(RUN) || TypeGuard<Observation>(SKIP);
      }

      const EntityPtr operator()(const EntityPtr entity) override
      {
        using namespace observation;
        auto event = std::dynamic_pointer_cast<Event>(entity);
        if (!entity)
          throw EntityError("Unexpected Entity type in UpcaseValue: ", entity->getName());
        auto nos = std::make_shared<Event>(*event.get());

        upcase(std::get<std::string>(nos->getValue()));
        return next(nos);
      }
    };

  }  // namespace pipeline
}  // namespace mtconnect
