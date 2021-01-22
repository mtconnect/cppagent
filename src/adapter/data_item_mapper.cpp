//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "data_item_mapper.hpp"

#include "shdr_parser.hpp"

using namespace std;

namespace mtconnect
{
  namespace adapter
  {
    static dlib::logger g_logger("DataItemMapper");

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

    inline static string &upcase(string &s)
    {
      std::transform(s.begin(), s.end(), s.begin(),
                     [](unsigned char c) -> unsigned char { return std::toupper(c); });
      return s;
    }

    inline static bool unavailable(const string &str)
    {
      const static string unavailable("UNAVAILABLE");
      return equal(str.cbegin(), str.cend(), unavailable.cbegin(), unavailable.cend(),
                   [](const char a, const char b) { return toupper(a) == b; });
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
    static entity::Requirements s_sample{{"VALUE", entity::DOUBLE, false}};
    static entity::Requirements s_event{{"VALUE", false}};
    static entity::Requirements s_dataSet{{"VALUE", entity::DATA_SET, false}};

    static inline std::string extractResetTrigger(const DataItem *dataItem,
                                                  TokenList::const_iterator &token,
                                                  entity::Properties &properties)
    {
      size_t pos;
      // Check for reset triggered
      if ((dataItem->hasResetTrigger() || dataItem->isTable() || dataItem->isDataSet()) &&
          (pos = token->find(':')) != string::npos)
      {
        string trig, value;
        if (dataItem->isSample())
        {
          trig = token->substr(pos + 1);
          value = token->substr(0, pos);
        }
        else
        {
          auto ef = token->find_first_of(" \t", pos);
          trig = token->substr(1, ef - 1);
          value = token->substr(ef + 1);
        }
        properties.insert_or_assign("resetTriggered", upcase(trig));
        return value;
      }
      else
      {
        return *token;
      }
    }

    inline void zipProperties(ShdrObservation &obs, const entity::Requirements &reqs,
                              TokenList::const_iterator &token,
                              const TokenList::const_iterator &end, bool upc = false)
    {
      auto req = reqs.begin();
      auto &observation = get<DataItemObservation>(obs.m_observed);
      while (token != end && req != reqs.end())
      {
        // TODO: Handle unavailable and resetTriggered
        if (unavailable(*token) && (req->getName() == "VALUE" || req->getName() == "level"))
        {
          observation.m_unavailable = true;
          obs.m_properties.insert_or_assign(req->getName(), "UNAVAILABLE");
          token++;
          req++;
          continue;
        }

        entity::Value value = extractResetTrigger(observation.m_dataItem, token, obs.m_properties);

        if (upc && req->getType() == entity::STRING && !observation.m_dataItem->isTable() &&
            !observation.m_dataItem->isDataSet())
        {
          upcase(get<string>(value));
        }

        try
        {
          if (req->convertType(value, observation.m_dataItem->isTable()))
          {
            obs.m_properties.insert_or_assign(req->getName(), value);
          }
          else
          {
            g_logger << dlib::LWARN << "Cannot convert value for: " << *token;
          }
        }
        catch (entity::PropertyError &e)
        {
          g_logger << dlib::LWARN << "Cannot convert value for: " << *token << " - " << e.what();
          throw;
        }

        token++;
        req++;
      }
    }

    void MapTokensToDataItem(ShdrObservation &obs, TokenList::const_iterator &token,
                             const TokenList::const_iterator &end, Context &context)
    {
      obs.m_observed.emplace<DataItemObservation>();
      auto &observation = get<DataItemObservation>(obs.m_observed);

      auto dataItemKey = splitKey(*token++);
      obs.m_device = context.m_getDevice(dataItemKey.second.value_or(""));
      observation.m_dataItem = context.m_getDataItem(obs.m_device, dataItemKey.first);

      if (observation.m_dataItem == nullptr)
      {
        // resync to next item
        if (context.m_logOnce.count(dataItemKey.first) > 0)
          g_logger << LTRACE << "(" << obs.m_device->getName()
                   << ") Could not find data item: " << dataItemKey.first;
        else
        {
          g_logger << LWARN << "(" << obs.m_device->getName()
                   << ") Could not find data item: " << dataItemKey.first;
          context.m_logOnce.insert(dataItemKey.first);
        }

        throw entity::PropertyError("Cannot find data item");
        return;
      }

      entity::Requirements *reqs{nullptr};

      // Extract the remaining tokens
      if (observation.m_dataItem->isSample())
      {
        if (observation.m_dataItem->isTimeSeries())
          reqs = &s_timeseries;
        else
          reqs = &s_sample;
      }
      else if (observation.m_dataItem->isEvent())
      {
        if (observation.m_dataItem->isMessage())
          reqs = &s_message;
        else if (observation.m_dataItem->isAlarm())
          reqs = &s_alarm;
        else if (observation.m_dataItem->isDataSet() || observation.m_dataItem->isTable())
          reqs = &s_dataSet;
        else
          reqs = &s_event;
      }
      else if (observation.m_dataItem->isCondition())
      {
        reqs = &s_condition;
      }

      // TODO: Check for UNAVAILABLE
      if (reqs != nullptr)
      {
        zipProperties(obs, *reqs, token, end, context.m_upcaseValue);
        const char *field = observation.m_dataItem->isCondition() ? "level" : "VALUE";
        if (obs.m_properties.count(field) == 0)
        {
          observation.m_unavailable = true;
          obs.m_properties[field] = "UNAVAILABLE";
        }
      }
      else
      {
        g_logger << LWARN << "Cannot find requirements for " << dataItemKey.first;
        throw entity::PropertyError("Unresolved data item requirements");
      }
    }
  }  // namespace adapter
}  // namespace mtconnect
