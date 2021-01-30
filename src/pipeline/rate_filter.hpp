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

#pragma once

#include "transform.hpp"

namespace mtconnect
{
  namespace pipeline
  {
    class RateFilter : public Transform
    {
    public:
      ~RateFilter() override = default;

      bool filterMinimumDelta(const std::string &id, double value, double fv)
      {
        auto last = m_lastSampleValue.find(id);
        if (last != m_lastSampleValue.end())
        {
          double lv = last->second;
          if (value > (lv - fv) && value < (lv + fv))
          {
            return true;
          }
          last->second = value;
        }
        else
        {
          m_lastSampleValue[id] = value;
        }
        
        return false;
      }
      
      bool filterPeriod(const std::string &id, Timestamp &value,
                        const std::chrono::duration<double> md)
      {
        using namespace std;
        auto last = m_lastTimeOffset.find(id);
        if (last != m_lastTimeOffset.end())
        {
          auto lv = last->second;
          if (value < (lv + md))
          {
            return true;
          }
          last->second = value;
        }
        else
        {
          m_lastTimeOffset[id] = value;
        }
        
        return false;
      }
            
      const EntityPtr operator()(const EntityPtr entity) override
      {
        using namespace std;
        using namespace observation;
        if (auto o = std::dynamic_pointer_cast<Observation>(entity);
            o)
        {
          auto di = o->getDataItem();
          auto &id = di->getId();
          if (o->isUnavailable())
          {
            m_lastSampleValue.erase(id);
            m_lastTimeOffset.erase(id);
            return next(entity);
          }
          
          if (di->isSample())
          {
            if (auto md = m_minimumDelta.find(id); md !=  m_minimumDelta.end())
            {
              double value = o->getValue<double>();
              if (filterMinimumDelta(di->getId(), value, md->second))
                return EntityPtr();
            }
          }
          
          if (auto md = m_minimumDuration.find(id); md != m_minimumDuration.end())
          {
            auto value = o->getTimestamp();
            if (filterPeriod(di->getId(), value, md->second))
              return EntityPtr();
          }
        }
        
        return next(entity);
      }
      
      TransformPtr bindTo(TransformPtr trans)
      {
        // Event, Sample, Timeseries, DataSetEvent, Message, Alarm,
        // AssetEvent, ThreeSpaceSmple, Condition, AssetEvent
        using namespace observation;
        trans->bind<Event, Sample, Message, Alarm>(this->getptr());
        return getptr();
      }

      void addMinimumDelta(const std::string &id, double d)
      {
        m_minimumDelta[id] = d;
      }
      void addMinimumDelta(const DataItem *di, double d)
      {
        m_minimumDelta[di->getId()] = d;
      }
      void addMinimumDuration(const std::string &id, const std::chrono::duration<double> &d)
      {
        m_minimumDuration[id] = d;
      }
      void addMinimumDuration(const DataItem *di, const std::chrono::duration<double> &d)
      {
        m_minimumDuration[di->getId()] = d;
      }

    protected:
      std::unordered_map<std::string, double> m_minimumDelta;
      std::unordered_map<std::string, std::chrono::duration<double>> m_minimumDuration;
      std::unordered_map<std::string, double> m_lastSampleValue;
      std::unordered_map<std::string, Timestamp> m_lastTimeOffset;
    };
  }
}  // namespace mtconnect
