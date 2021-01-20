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

#include "shdr_parser.hpp"
#include "data_item_mapper.hpp"

using namespace std;

namespace mtconnect
{
  namespace adapter
  {
    static dlib::logger g_logger("ShdrParser");
    
    inline static std::pair<std::string,std::optional<std::string>> splitKey(const std::string &key)
    {
      auto c = key.find(':');
      if (c != string::npos)
        return { key.substr(c + 1, string::npos), key.substr(0, c - 1) };
      else
        return {key, nullopt};
    }
    
    inline optional<double> getDuration(std::string &timestamp)
    {
      optional<double> duration;

      auto pos = timestamp.find('@');
      if (pos != string::npos)
      {
        auto read = pos + 1;;
        duration = std::stod(timestamp, &read);
        if (read == pos + 1)
          duration.reset();
        timestamp = timestamp.erase(pos);
      }
      
      return duration;
    }
    
    // --------------------------------------
    // Mapping to data items
    
    entity::Requirements DataItemMapper::m_condition {
      { "level", true },
      { "nativeCode", false },
      { "nativeSeverity", false },
      { "qualifier", false },
      { "value", false }
    };
    entity::Requirements DataItemMapper::m_timeseries {
      { "count", entity::INTEGER, true },
      { "frequency", entity::DOUBLE, true },
      { "value", entity::VECTOR, true }
    };
    entity::Requirements DataItemMapper::m_message {
      { "nativeCode", false },
      { "value", false }
    };
    entity::Requirements DataItemMapper::m_asset {
      { "assetId", false },
      { "type", false }
    };
    entity::Requirements DataItemMapper::m_sample {
      { "value", entity::DOUBLE, false },
    };
    entity::Requirements DataItemMapper::m_event {
      { "value", false },
    };
    
    void DataItemMapper::mapTokensToAsset(ShdrObservation &obs,
                                       TokenList::const_iterator &token,
                                       const TokenList::const_iterator &end)
    {
      
    }
    
    inline void zipProperties(entity::Properties &properties,
                              const entity::Requirements &reqs,
                              TokenList::const_iterator &token,
                              const TokenList::const_iterator &end)
    {
      auto req = reqs.begin();
      while (token != end && req != reqs.end())
      {
        entity::Value value(*token);
        try
        {
          if (req->convertType(value))
          {
            properties.insert_or_assign(*token, value);
          }
          else
          {
            g_logger << dlib::LWARN << "Cannot convert value for: " << *token;
          }
        }
        catch (entity::PropertyError &e)
        {
          g_logger << dlib::LWARN << "Cannot convert value for: " << *token
                    << " - " << e.what();
        }
        
        token++;
        req++;
      }
    }
    
    void DataItemMapper::mapTokensToDataItems(ShdrObservation &obs,
                                         TokenList::const_iterator &token,
                                         const TokenList::const_iterator &end,
                                         Context &context)
    {
      // Asset maping
      if ((*token)[0] == '@')
      {
        mapTokensToAsset(obs, token, end);
        return;
      }

      ShdrDataItemObservation observation{obs};
      
      while (token != end)
      {
        string deviceId;
        auto dataItemKey = splitKey(*token++);
        observation.m_device = context.m_getDevice(dataItemKey.second.value_or(""));
        observation.m_dataItem = context.m_getDataItem(observation.m_device, dataItemKey.first);
        
        if (observation.m_dataItem == nullptr)
        {
          // resync to next item
          if (context.m_logOnce.count(dataItemKey.first) > 0)
            g_logger << LTRACE << "(" << observation.m_device->getName()
                     << ") Could not find data item: " << dataItemKey.first;
          else
          {
            g_logger << LWARN << "(" << observation.m_device->getName()
                     << ") Could not find data item: " << dataItemKey.first;
            context.m_logOnce.insert(dataItemKey.first);
          }

          continue;
        }
        
        entity::Requirements *reqs { nullptr };
        
        // Extract the remaining tokens
        if (observation.m_dataItem->getCategory() == DataItem::SAMPLE)
        {
          if (observation.m_dataItem->getRepresentation() == DataItem::TIME_SERIES)
          {
            reqs = &m_timeseries;
          }
          else
          {
            reqs = &m_sample;
          }
        }
        else if (observation.m_dataItem->getCategory() == DataItem::EVENT)
        {
          if (observation.m_dataItem->getType() == "MESSAGE")
          {
            reqs = &m_message;
          }
          else
          {
            reqs = &m_event;
          }
        }
        else if (observation.m_dataItem->getCategory() == DataItem::CONDITION)
        {
          reqs = &m_condition;
        }
        
        if (reqs != nullptr)
          zipProperties(observation.m_properties, *reqs, token, end);
        else
          g_logger << LWARN << "Cannot find requirements for " << dataItemKey.first;
      }
    }
  }
}
