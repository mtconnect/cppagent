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

#include "filter.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/factory.hpp"

namespace mtconnect::device_model::data_item {
  /// @brief Constraint for a data item
  class AGENT_LIB_API Constraints : public entity::Entity
  {
  public:
    static entity::FactoryPtr getFactory()
    {
      using namespace mtconnect::entity;
      static FactoryPtr factory;
      if (!factory)
      {
        auto limit = std::make_shared<Factory>(Requirements {{"VALUE", ValueType::DOUBLE, true}});
        auto value = std::make_shared<Factory>(Requirements {{"VALUE", true}});
        auto filter = Filter::getFactory()->factoryFor("Filter")->deepCopy();
        filter->getRequirement("type")->setMultiplicity(0, 1);
        factory = std::make_shared<Factory>(
            Requirements {{"Minimum", ValueType::ENTITY, limit, 0, 1},
                          {"Maximum", ValueType::ENTITY, limit, 0, 1},
                          {"Nominal", ValueType::ENTITY, limit, 0, 1},
                          {"Value", ValueType::ENTITY, value, 0, Requirement::Infinite},
                          {"Filter", ValueType::ENTITY, filter, 0, Requirement::Infinite}});
      }
      return factory;
    }
  };
}  // namespace mtconnect::device_model::data_item
