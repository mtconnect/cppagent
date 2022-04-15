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

#include <chrono>
#include <regex>

#include "entity/entity.hpp"
#include "observation/observation.hpp"
#include "pipeline/transform.hpp"

namespace mtconnect::adapter::agent {
  using namespace mtconnect::entity;
  using namespace mtconnect::pipeline;
  class MTConnectStreamTransform : public Transform
  {
  public:
    MTConnectStreamTransform(const ShdrTokenMapper &) = default;
    MTConnectStreamTransform(PipelineContextPtr context,
                    const std::optional<std::string> &device = std::nullopt,
                    int version = 1)
      : Transform("ShdrTokenMapper"),
        m_contract(context->m_contract.get()),
        m_defaultDevice(device), m_shdrVersion(version)
    {
      m_guard = TypeGuard<Timestamped>(RUN);
    }

    
  };
}
