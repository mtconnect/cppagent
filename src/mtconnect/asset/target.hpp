//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <map>
#include <utility>
#include <vector>

#include "asset.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/factory.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::asset {
  /// @brief A target of a process or a task
  class AGENT_LIB_API Target : public entity::Entity
  {
  public:
    static entity::FactoryPtr getFactory();
    static entity::FactoryPtr getDeviceTargetsFactory();
    static entity::FactoryPtr getTargetsFactory();
    static entity::FactoryPtr getAllTargetsFactory();
    static entity::FactoryPtr getRequirementTargetsFactory();
  };

  /// @brief A group of possible targets
  class AGENT_LIB_API TargetGroup : public Target
  {
  public:
    static entity::FactoryPtr getFactory();
  };

  /// @brief A device target where the `targetId` is the device UUID
  class AGENT_LIB_API TargetDevice : public Target
  {
  public:
    static entity::FactoryPtr getFactory();
  };

  /// @brief A reference to a target group
  class AGENT_LIB_API TargetRef : public Target
  {
  public:
    static entity::FactoryPtr getFactory();
  };

  /// @brief A requirement for a target
  class AGENT_LIB_API TargetRequirement : public Target
  {
  public:
    static entity::FactoryPtr getFactory();
  };

}  // namespace mtconnect::asset
