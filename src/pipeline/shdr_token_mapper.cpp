//
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

#include "shdr_token_mapper.hpp"

#include "asset/asset.hpp"
#include "device_model/device.hpp"
#include "entity/factory.hpp"
#include "entity/xml_parser.hpp"
#include "logging.hpp"
#include "observation/observation.hpp"
#include "upcase_value.hpp"

using namespace std;

namespace mtconnect {
  using namespace observation;
  namespace pipeline {
    inline bool unavailable(const string &str)
    {
      const static string unavailable("UNAVAILABLE");
      return equal(str.cbegin(), str.cend(), unavailable.cbegin(), unavailable.cend(),
                   [](const char a, const char b) { return toupper(a) == b; });
    }

    inline static std::pair<std::string, std::optional<std::string>> splitKey(
        const std::string &key)
    {
      auto c = key.find(':');
      if (c != string::npos)
        return {key.substr(c + 1, string::npos), key.substr(0, c)};
      else
        return {key, nullopt};
    }

    inline optional<double> getDuration(std::string &timestamp)
    {
      optional<double> duration;

      auto pos = timestamp.find('@');
      if (pos != string::npos)
      {
        auto read = pos + 1;
        ;
        duration = std::stod(timestamp, &read);
        if (read == pos + 1)
          duration.reset();
        timestamp = timestamp.erase(pos);
      }

      return duration;
    }

    // --------------------------------------
    // Mapping to data items
    static entity::Requirements s_condition {{"level", true},
                                             {"nativeCode", false},
                                             {"nativeSeverity", false},
                                             {"qualifier", false},
                                             {"VALUE", false}};
    static entity::Requirements s_alarm {{"code", true},
                                         {"nativeCode", false},
                                         {"severity", false},
                                         {"state", true},
                                         {"VALUE", false}};
    static entity::Requirements s_timeseries {{"sampleCount", entity::INTEGER, true},
                                              {"sampleRate", entity::DOUBLE, true},
                                              {"VALUE", entity::VECTOR, true}};
    static entity::Requirements s_message {{"nativeCode", false}, {"VALUE", false}};
    static entity::Requirements s_threeSpaceSample {{"VALUE", entity::VECTOR, false}};
    static entity::Requirements s_sample {{"VALUE", entity::DOUBLE, false}};
    static entity::Requirements s_assetEvent {{"assetType", false}, {"VALUE", false}};
    static entity::Requirements s_event {{"VALUE", false}};
    static entity::Requirements s_dataSet {{"VALUE", entity::DATA_SET, false}};

    static inline size_t firtNonWsColon(const string &token)
    {
      auto len = token.size();
      for (size_t i = 0; i < len; i++)
      {
        if (token[i] == ':')
          return i;
        else if (!isspace(token[i]))
          return string::npos;
      }

      return string::npos;
    }

    static inline std::string extractResetTrigger(const DataItemPtr dataItem, const string &token,
                                                  Properties &properties)
    {
      size_t pos;
      // Check for reset triggered
      auto hasResetTriggered = dataItem->hasProperty("ResetTrigger");
      if (hasResetTriggered || dataItem->isTable() || dataItem->isDataSet())
      {
        string trig, value;
        if (!dataItem->isDataSet() && (pos = token.find(':')) != string::npos)
        {
          trig = token.substr(pos + 1);
          value = token.substr(0, pos);
        }
        else if (dataItem->isDataSet() && (pos = firtNonWsColon(token)) != string::npos)
        {
          auto ef = token.find_first_of(" \t", pos);
          trig = token.substr(1, ef - 1);
          if (ef != string::npos)
            value = token.substr(ef + 1);
        }
        else
        {
          return token;
        }

        if (!trig.empty())
          properties.insert_or_assign("resetTriggered", upcase(trig));
        return value;
      }
      else
      {
        return token;
      }
    }

    inline ObservationPtr zipProperties(const DataItemPtr dataItem, const Timestamp &timestamp,
                                        const entity::Requirements &reqs,
                                        TokenList::const_iterator &token,
                                        const TokenList::const_iterator &end, ErrorList &errors)
    {
      NAMED_SCOPE("zipProperties");
      Properties props;
      for (auto req = reqs.begin(); token != end && req != reqs.end(); token++, req++)
      {
        const string &tok = *token;

        if (req->getName() == "VALUE" || req->getName() == "level")
        {
          if (unavailable(tok))
          {
            continue;
          }
        }
        else if (tok.empty())
        {
          continue;
        }

        entity::Value value {extractResetTrigger(dataItem, tok, props)};

        try
        {
          req->convertType(value, dataItem->isTable());
          props.insert_or_assign(req->getName(), value);
        }
        catch (entity::PropertyError &e)
        {
          LOG(warning) << "Cannot convert value for data item id '" << dataItem->getId()
                       << "': " << *token << " - " << e.what();
        }
      }

      return Observation::make(dataItem, props, timestamp, errors);
    }

    EntityPtr ShdrTokenMapper::mapTokensToDataItem(const Timestamp &timestamp,
                                                   const std::optional<std::string> &source,
                                                   TokenList::const_iterator &token,
                                                   const TokenList::const_iterator &end,
                                                   ErrorList &errors)
    {
      NAMED_SCOPE("DataItemMapper.ShdrTokenMapper.mapTokensToDataItem");
      auto dataItemKey = splitKey(*token++);
      string device = dataItemKey.second.value_or(m_defaultDevice.value_or(""));
      auto dataItem = m_contract->findDataItem(device, dataItemKey.first);

      if (dataItem == nullptr)
      {
        // resync to next item
        if (m_logOnce.count(dataItemKey.first) > 0)
          LOG(trace) << "Could not find data item: " << dataItemKey.first;
        else
        {
          LOG(info) << "Could not find data item: " << dataItemKey.first;
          m_logOnce.insert(dataItemKey.first);
        }

        // Skip following tolken if we are in legacy mode
        if (m_shdrVersion < 2 && token != end)
          token++;

        return nullptr;
      }

      entity::Requirements *reqs {nullptr};

      // Extract the remaining tokens
      if (dataItem->isSample())
      {
        if (dataItem->isTimeSeries())
          reqs = &s_timeseries;
        else if (dataItem->isThreeSpace())
          reqs = &s_threeSpaceSample;
        else
          reqs = &s_sample;
      }
      else if (dataItem->isEvent())
      {
        if (dataItem->isMessage())
          reqs = &s_message;
        else if (dataItem->isAlarm())
          reqs = &s_alarm;
        else if (dataItem->isDataSet() || dataItem->isTable())
          reqs = &s_dataSet;
        else if (dataItem->isAssetChanged() || dataItem->isAssetRemoved())
          reqs = &s_assetEvent;
        else
          reqs = &s_event;
      }
      else if (dataItem->isCondition())
      {
        reqs = &s_condition;
      }

      if (reqs != nullptr)
      {
        auto obs = zipProperties(dataItem, timestamp, *reqs, token, end, errors);
        if (dataItem->getConstantValue())
          return nullptr;
        if (obs && source)
          dataItem->setDataSource(*source);
        return obs;
      }
      else
      {
        LOG(warning) << "Cannot find requirements for " << dataItemKey.first;
        throw entity::PropertyError("Unresolved data item requirements");
      }

      return nullptr;
    }

    EntityPtr ShdrTokenMapper::mapTokensToAsset(const Timestamp &timestamp,
                                                const std::optional<std::string> &source,
                                                TokenList::const_iterator &token,
                                                const TokenList::const_iterator &end,
                                                ErrorList &errors)
    {
      using namespace mtconnect::asset;
      EntityPtr res;
      auto command = *token++;
      if (command == "@ASSET@")
      {
        auto assetId = *token++;
        auto type = *token++;
        auto body = *token++;

        XmlParser parser;
        res = parser.parse(Asset::getRoot(), body, "2.0", errors);
        if (auto asset = dynamic_pointer_cast<Asset>(res))
        {
          asset->setAssetId(assetId);
          asset->setProperty("timestamp", timestamp);
          if (m_defaultDevice)
          {
            DevicePtr dev = m_contract->findDevice(*m_defaultDevice);
            if (dev != nullptr)
              asset->setProperty("deviceUuid", *dev->getUuid());
          }
        }
        else
          res = EntityPtr();

        if (!errors.empty())
        {
          LOG(warning) << "Could not parse asset: " << body;
          for (auto &e : errors)
          {
            LOG(warning) << "    Message: " << e->what();
          }
        }
      }
      else
      {
        auto ac = make_shared<AssetCommand>("AssetCommand", Properties {});
        ac->m_timestamp = timestamp;
        if (command == "@REMOVE_ALL_ASSETS@")
        {
          ac->setName("AssetCommand");
          ac->setValue("RemoveAll"s);
          if (token != end)
          {
            if (!token->empty())
              ac->setProperty("type", *token);
            token++;
          }
          if (m_defaultDevice)
            ac->setProperty("device", *m_defaultDevice);
        }
        else if (command == "@REMOVE_ASSET@")
        {
          ac->setValue("RemoveAsset"s);
          ac->setProperty("assetId", *token++);
          if (m_defaultDevice)
            ac->setProperty("device", *m_defaultDevice);
        }
        else
        {
          throw EntityError("Unkown asset command " + command);
        }
        res = ac;
      }
      return res;
    }

    const EntityPtr ShdrTokenMapper::operator()(const EntityPtr entity)
    {
      NAMED_SCOPE("DataItemMapper.ShdrTokenMapper.operator");
      if (auto timestamped = std::dynamic_pointer_cast<Timestamped>(entity))
      {
        // Don't copy the tokens.
        auto res = std::make_shared<Observations>(*timestamped, TokenList {});
        EntityList entities;

        auto &tokens = timestamped->m_tokens;
        auto token = tokens.cbegin();
        auto end = tokens.end();

        while (token != end)
        {
          auto start = token;
          EntityPtr out;
          ErrorList errors;
          try
          {
            auto source = entity->maybeGet<string>("source");
            entity::ErrorList errors;
            if ((*token)[0] == '@')
            {
              out = mapTokensToAsset(timestamped->m_timestamp, source, token, end, errors);
            }
            else
            {
              out = mapTokensToDataItem(timestamped->m_timestamp, source, token, end, errors);
              if (out && timestamped->m_duration)
                out->setProperty("duration", *timestamped->m_duration);
            }

            if (out && errors.empty())
            {
              auto fwd = next(out);
              if (fwd)
                entities.emplace_back(fwd);
            }

            // For legacy token handling, stop if we have
            // consumed more than two tokens.
            if (m_shdrVersion < 2)
            {
              auto distance = std::distance(start, token);
              if (distance > 2)
                break;
            }
          }
          catch (entity::EntityError &e)
          {
            LOG(error) << "Could not create observation: " << e.what();
          }
          for (auto &e : errors)
          {
            LOG(warning) << "Error while parsing tokens: " << e->what();
            for (auto it = start; it != token; it++)
              LOG(warning) << "    token: " << *token;
          }
        }

        res->setValue(entities);
        return next(res);
      }
      else
      {
        throw EntityError("Cannot map non-timestamped token stream");
      }

      return nullptr;
    }
  }  // namespace pipeline
}  // namespace mtconnect
