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

#include "entity/entity.hpp"
#include "entity/factory.hpp"
#include "filter.hpp"

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      class Constraints : public entity::Entity
      {
      public:
        static entity::FactoryPtr getFactory()
        {
          using namespace mtconnect::entity;
          static FactoryPtr factory;
          if (!factory)
          {
            auto limit = std::make_shared<Factory>(Requirements {{"VALUE", DOUBLE, true}});
            auto value = std::make_shared<Factory>(Requirements {{"VALUE", true}});
            auto filter = Filter::getFactory()->factoryFor("Filter")->deepCopy();
            filter->getRequirement("type")->setMultiplicity(0, 1);
            factory = std::make_shared<Factory>(
                Requirements {{"Minimum", ENTITY, limit, 0, 1},
                              {"Maximum", ENTITY, limit, 0, 1},
                              {"Nominal", ENTITY, limit, 0, 1},
                              {"Value", ENTITY, value, 0, Requirement::Infinite},
                              {"Filter", ENTITY, filter, 0, Requirement::Infinite}});
          }
          return factory;
        }
      };
    }  // namespace data_item
  }    // namespace device_model
}  // namespace mtconnect
