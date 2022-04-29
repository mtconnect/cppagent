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
    using namespace entity;
    class ShdrTokenMapper : public Transform
    {
    public:
      ShdrTokenMapper(const ShdrTokenMapper &) = default;
      ShdrTokenMapper(PipelineContextPtr context,
                      const std::optional<std::string> &device = std::nullopt, int version = 1)
        : Transform("ShdrTokenMapper"),
          m_contract(context->m_contract.get()),
          m_defaultDevice(device),
          m_shdrVersion(version)
      {
        m_guard = TypeGuard<Timestamped>(RUN);
      }
      const EntityPtr operator()(const EntityPtr entity) override;

      // Takes a tokenized set of fields and maps them to timestamp and data items
      EntityPtr mapTokensToDataItem(const Timestamp &timestamp,
                                    const std::optional<std::string> &source,
                                    TokenList::const_iterator &token,
                                    const TokenList::const_iterator &end, ErrorList &errors);
      EntityPtr mapTokensToAsset(const Timestamp &timestamp,
                                 const std::optional<std::string> &source,
                                 TokenList::const_iterator &token,
                                 const TokenList::const_iterator &end, ErrorList &errors);

    protected:
      // Logging Context
      std::set<std::string> m_logOnce;
      PipelineContract *m_contract;
      std::optional<std::string> m_defaultDevice;
      int m_shdrVersion {1};
    };
  }  // namespace pipeline
}  // namespace mtconnect
