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

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      class Filter : public entity::Entity
      {
      public:
        static entity::FactoryPtr getFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;
          static auto filter = make_shared<Factory>(Requirements {
              {"type", ControlledVocab {"PERIOD", "MINIMUM_DELTA"}}, {"VALUE", DOUBLE, true}});
          static auto filters = make_shared<Factory>(
              Requirements {{"Filter", ENTITY, filter, 1, Requirement::Infinite}});
          return filters;
        }
      };
    }  // namespace data_item
  }    // namespace device_model
}  // namespace mtconnect
