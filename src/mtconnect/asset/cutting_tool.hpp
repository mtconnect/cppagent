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

#include <map>
#include <utility>
#include <vector>

#include "asset.hpp"
#include "mtconnect/asset/physical_asset.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  namespace asset {
    /// @brief Cutting Tool Architype Asset
    class AGENT_LIB_API CuttingToolArchetype : public Asset
    {
    public:
      static entity::FactoryPtr getFactory();
      static void registerAsset();
    };

    /// @brief Cutting Tool Asset
    class AGENT_LIB_API CuttingTool : public Asset
    {
    public:
      static entity::FactoryPtr getFactory();
      static void registerAsset();
    };
  }  // namespace asset
}  // namespace mtconnect
