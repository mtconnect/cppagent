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
  class DataItem;

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

    inline static std::string &upcase(std::string &s)
    {
      std::transform(s.begin(), s.end(), s.begin(),
                     [](unsigned char c) -> unsigned char { return std::toupper(c); });
      return s;
    }

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

      TransformPtr bindTo(TransformPtr trans)
      {
        trans->bind<Timestamped>(this->getptr());
        return getptr();
      }

    protected:
      // Logging Context
      std::set<std::string> m_logOnce;
    };

    class UpcaseValue : public Transform
    {
    public:
      const EntityPtr operator()(const EntityPtr entity) override
      {
        using namespace observation;
        using T = std::decay_t<decltype(*entity.get())>;
        if (std::is_same<T, Event>())
        {
          auto event = std::dynamic_pointer_cast<Event>(entity);
          auto nos = std::make_shared<Event>(*event.get());
          upcase(std::get<std::string>(nos->getValue()));
          return next(nos);
        }
        else
        {
          return next(entity);
        }
      }
      
      TransformPtr bindTo(TransformPtr trans)
      {
        // Event, Sample, Timeseries, DataSetEvent, Message, Alarm,
        // AssetEvent, ThreeSpaceSmple, Condition, AssetEvent
        using namespace observation;
        trans->bind<Event>(this->getptr());
        return getptr();
      }

    };

  }  // namespace source
}  // namespace mtconnect
