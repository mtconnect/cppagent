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
#include "mtconnect/entity/factory.hpp"

/// @brief Configuration namespace
namespace mtconnect::device_model::configuration {
  /// @brief Configuration Entity
  struct Configuration
  {
    static entity::FactoryPtr getFactory();
    static entity::FactoryPtr getRoot();
  };

  // TODO: Need to support extended configuration
}  // namespace mtconnect::device_model::configuration
