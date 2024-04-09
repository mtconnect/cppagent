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
#include "mtconnect/entity/entity.hpp"

namespace mtconnect::device_model::data_item {
  /// @brief Data item source
  class AGENT_LIB_API Source : public entity::Entity
  {
  public:
    static entity::FactoryPtr getFactory()
    {
      using namespace mtconnect::entity;
      static auto source = std::make_shared<Factory>(Requirements {{"componentId", false},
                                                                   {"compositionId", false},
                                                                   {"dataItemId", false},
                                                                   {"VALUE", false}});
      return source;
    }
  };
}  // namespace mtconnect::device_model::data_item
