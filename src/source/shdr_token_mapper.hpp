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
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "transform.hpp"

#include <chrono>
#include <regex>

namespace mtconnect
{
  class Device;
  class DataItem;

  namespace source
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
      const EntityPtr operator()(const EntityPtr entity) override;

      // Takes a tokenized set of fields and maps them to timestamp and data items
      EntityPtr mapTokensToDataItem(const Timestamp &timestamp, TokenList::const_iterator &token,
                                    const TokenList::const_iterator &end, ErrorList &errors);
      EntityPtr mapTokesnToAsset(const Timestamp &timestamp, TokenList::const_iterator &token,
                                 const TokenList::const_iterator &end, ErrorList &errors);

      using GetDevice = std::function<const Device *(const std::string &id)>;
      using GetDataItem =
          std::function<const DataItem *(const Device *device, const std::string &id)>;

      // Functions to handle get information
      GetDevice m_getDevice;
      GetDataItem m_getDataItem;

      // Logging Context
      std::set<std::string> m_logOnce;
    };

  }  // namespace source
}  // namespace mtconnect
