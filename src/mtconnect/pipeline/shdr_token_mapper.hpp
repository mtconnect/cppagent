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

#include <chrono>
#include <regex>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  using namespace entity;
  /// @brief A list of observations for testing
  class AGENT_LIB_API Observations : public Timestamped
  {
  public:
    using Timestamped::Timestamped;
  };

  /// @brief Map a token list to data items or asset types
  class AGENT_LIB_API ShdrTokenMapper : public Transform
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
    EntityPtr operator()(entity::EntityPtr &&entity) override;

    /// @brief Takes a tokenized set of fields and maps them data items
    /// @param[in] timestamp the timestamp from prior extraction
    /// @param[in] source the optional source
    /// @param[in] token a token itertor
    /// @param[in] end the sentinal end token
    /// @param[in,out] errors
    /// @return returns an observation list
    EntityPtr mapTokensToDataItem(const Timestamp &timestamp,
                                  const std::optional<std::string> &source,
                                  TokenList::const_iterator &token,
                                  const TokenList::const_iterator &end, ErrorList &errors);
    /// @brief Takes a tokenized set of fields and maps them to assets
    /// @param timestamp the timestamp
    /// @param source the optional source
    /// @param[in] token a token itertor
    /// @param[in] end the sentinal end token
    /// @param[in,out] errors
    /// @return An asset
    EntityPtr mapTokensToAsset(const Timestamp &timestamp, const std::optional<std::string> &source,
                               TokenList::const_iterator &token,
                               const TokenList::const_iterator &end, ErrorList &errors);

  protected:
    // Logging Context
    std::set<std::string> m_logOnce;
    PipelineContract *m_contract;
    std::optional<std::string> m_defaultDevice;
    std::unordered_map<std::string, WeakDataItemPtr> m_dataItemMap;
    int m_shdrVersion {1};
  };
}  // namespace mtconnect::pipeline
