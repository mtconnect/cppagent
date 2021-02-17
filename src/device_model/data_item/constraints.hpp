//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "entity.hpp"

namespace mtconnect
{
  namespace device_model
  {
    namespace data_item
    {
      class Constraints : public entity::Entity
      {
      public:
        static entity::FactoryPtr getFactory()
        {
          using namespace mtconnect::entity;
          static FactoryPtr factory;
          if (!factory)
          {
            auto value = std::make_shared<Factory>(Requirements{{"VALUE", true}});
            value->setList(true);
            factory = std::make_shared<Factory>(Requirements{{"Minimum", DOUBLE, false},
              {"Maximum", DOUBLE, false},
              {"Nominal", DOUBLE, false}});
            factory->registerFactory("Value", value);
          }
          return factory;
        }
      };
    }  // namespace data_item
  }    // namespace device_model
}  // namespace mtconnect
