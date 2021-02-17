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

#include "entity/entity.hpp"
#include "observation/observation.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "transform.hpp"

#include <chrono>
#include <regex>

namespace mtconnect
{
  class Device;

  namespace pipeline
  {
    class AssetCommand : public Timestamped
    {
    public:
      using Timestamped::Timestamped;
    };

    class Observations : public Timestamped
    {
    public:
      using Timestamped::Timestamped;
    };

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
      int m_shdrVersion{1};
    };

    inline static std::string &upcase(std::string &s)
    {
      std::transform(s.begin(), s.end(), s.begin(),
                     [](unsigned char c) -> unsigned char { return std::toupper(c); });
      return s;
    }

    class UpcaseValue : public Transform
    {
    public:
      UpcaseValue(const UpcaseValue &) = default;
      UpcaseValue() : Transform("UpcaseValue")
      {
        using namespace observation;
        m_guard = ExactTypeGuard<Event>(RUN) || TypeGuard<Observation>(SKIP);
      }

      const EntityPtr operator()(const EntityPtr entity) override
      {
        using namespace observation;
        auto event = std::dynamic_pointer_cast<Event>(entity);
        if (!entity)
          throw EntityError("Unexpected Entity type in UpcaseValue: ", entity->getName());
        auto nos = std::make_shared<Event>(*event.get());

        upcase(std::get<std::string>(nos->getValue()));
        return next(nos);
      }
    };

  }  // namespace pipeline
}  // namespace mtconnect
