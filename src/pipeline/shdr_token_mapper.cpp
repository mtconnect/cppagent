//
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

#include "shdr_token_mapper.hpp"

#include "assets/asset.hpp"
#include "device_model/device.hpp"
#include "entity/factory.hpp"
#include "entity/xml_parser.hpp"
#include "observation/observation.hpp"

using namespace std;

namespace mtconnect
{
  using namespace observation;
  namespace pipeline
  {
    static dlib::logger g_logger("DataItemMapper");

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
        return {key.substr(c + 1, string::npos), key.substr(0, c - 1)};
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
    static entity::Requirements s_condition{{"level", true},
                                            {"nativeCode", false},
                                            {"nativeSeverity", false},
                                            {"qualifier", false},
                                            {"VALUE", false}};
    static entity::Requirements s_alarm{{"code", true},
                                        {"nativeCode", false},
                                        {"severity", false},
                                        {"state", true},
                                        {"VALUE", false}};
    static entity::Requirements s_timeseries{{"sampleCount", entity::INTEGER, true},
                                             {"sampleRate", entity::DOUBLE, true},
                                             {"VALUE", entity::VECTOR, true}};
    static entity::Requirements s_message{{"nativeCode", false}, {"VALUE", false}};
    static entity::Requirements s_threeSpaceSample{{"VALUE", entity::VECTOR, false}};
    static entity::Requirements s_sample{{"VALUE", entity::DOUBLE, false}};
    static entity::Requirements s_assetEvent{{"assetType", false}, {"VALUE", false}};
    static entity::Requirements s_event{{"VALUE", false}};
    static entity::Requirements s_dataSet{{"VALUE", entity::DATA_SET, false}};

    static inline std::string extractResetTrigger(const DataItem *dataItem, const string &token,
                                                  Properties &properties)
    {
      size_t pos;
      // Check for reset triggered
      if ((dataItem->hasResetTrigger() || dataItem->isTable() || dataItem->isDataSet()) &&
          (pos = token.find(':')) != string::npos)
      {
        string trig, value;
        if (dataItem->isSample())
        {
          trig = token.substr(pos + 1);
          value = token.substr(0, pos);
        }
        else
        {
          auto ef = token.find_first_of(" \t", pos);
          trig = token.substr(1, ef - 1);
          if (ef != string::npos)
            value = token.substr(ef + 1);
        }
        properties.insert_or_assign("resetTriggered", upcase(trig));
        return value;
      }
      else
      {
        return token;
      }
    }

    inline ObservationPtr zipProperties(const DataItem *dataItem, const Timestamp &timestamp,
                                        const entity::Requirements &reqs,
                                        TokenList::const_iterator &token,
                                        const TokenList::const_iterator &end, ErrorList &errors)
    {
      bool unavail{false};
      Properties props;
      for (auto req = reqs.begin(); token != end && req != reqs.end(); token++, req++)
      {
        const string &tok = *token;

        if (tok.empty())
        {
          continue;
        }
        if (unavailable(tok) && (req->getName() == "VALUE" || req->getName() == "level"))
        {
          unavail = true;
          continue;
        }

        entity::Value value{extractResetTrigger(dataItem, tok, props)};

        try
        {
          req->convertType(value, dataItem->isTable());
          props.insert_or_assign(req->getName(), value);
        }
        catch (entity::PropertyError &e)
        {
          g_logger << dlib::LWARN << "Cannot convert value for: " << *token << " - " << e.what();
          throw;
        }
      }

      return Observation::make(dataItem, props, timestamp, errors);
    }

    EntityPtr ShdrTokenMapper::mapTokensToDataItem(const Timestamp &timestamp,
                                                   TokenList::const_iterator &token,
                                                   const TokenList::const_iterator &end,
                                                   ErrorList &errors)
    {
      auto dataItemKey = splitKey(*token++);
      auto dataItem = m_contract->findDataItem(dataItemKey.second.value_or(""), dataItemKey.first);

      if (dataItem == nullptr)
      {
        // resync to next item
        if (m_logOnce.count(dataItemKey.first) > 0)
          g_logger << dlib::LTRACE << "Could not find data item: " << dataItemKey.first;
        else
        {
          g_logger << dlib::LTRACE << "Could not find data item: " << dataItemKey.first;
          m_logOnce.insert(dataItemKey.first);
        }

        return nullptr;
      }

      entity::Requirements *reqs{nullptr};

      // Extract the remaining tokens
      if (dataItem->isSample())
      {
        if (dataItem->isTimeSeries())
          reqs = &s_timeseries;
        else if (dataItem->is3D())
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

      // TODO: Check for UNAVAILABLE
      if (reqs != nullptr)
      {
        return zipProperties(dataItem, timestamp, *reqs, token, end, errors);
      }
      else
      {
        g_logger << dlib::LWARN << "Cannot find requirements for " << dataItemKey.first;
        throw entity::PropertyError("Unresolved data item requirements");
      }

      return nullptr;
    }

    EntityPtr ShdrTokenMapper::mapTokensToAsset(const Timestamp &timestamp,
                                                TokenList::const_iterator &token,
                                                const TokenList::const_iterator &end,
                                                ErrorList &errors)
    {
      EntityPtr res;
      auto command = *token++;
      if (command == "@ASSET@")
      {
        auto assetId = *token++;
        auto type = *token++;
        auto body = *token++;

        XmlParser parser;
        res = parser.parse(Asset::getRoot(), body, "1.7", errors);
        res->setProperty("timestamp", timestamp);
      }
      else
      {
        auto ac = make_shared<AssetCommand>("", Properties{});
        ac->m_timestamp = timestamp;
        if (command == "@REMOVE_ALL_ASSETS@")
        {
          ac->setName("RemoveAll");
          if (token != end)
            ac->setProperty("type", *token++);
        }
        else if (command == "@REMOVE_ASSET@")
        {
          ac->setName("RemoveAsset");
          ac->setProperty("assetId", *token++);
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
      if (auto timestamped = std::dynamic_pointer_cast<Timestamped>(entity))
      {
        // Don't copy the tokens.
        auto res = std::make_shared<Observations>(*timestamped, TokenList{});
        EntityList entities;

        auto tokens = timestamped->m_tokens;
        auto token = tokens.cbegin();
        auto end = tokens.end();

        while (token != end)
        {
          auto start = token;
          EntityPtr out;
          ErrorList errors;
          try
          {
            entity::ErrorList errors;
            if ((*token)[0] == '@')
            {
              out = mapTokensToAsset(timestamped->m_timestamp, token, end, errors);
            }
            else
            {
              out = mapTokensToDataItem(timestamped->m_timestamp, token, end, errors);
              if (out && timestamped->m_duration)
                out->setProperty("duration", *timestamped->m_duration);
            }

            if (out && errors.empty())
            {
              auto fwd = next(out);
              if (fwd)
                entities.emplace_back(fwd);
            }
          }
          catch (entity::EntityError &e)
          {
            g_logger << dlib::LERROR << "Could not create observation: " << e.what();
          }
          for (auto &e : errors)
          {
            g_logger << dlib::LWARN << "Error while parsing tokens: " << e->what();
            for (auto it = start; it != token; it++)
              g_logger << dlib::LWARN << "    token: " << *token;
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
