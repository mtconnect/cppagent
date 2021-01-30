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
  namespace source
  {
    class RateFilter : public Transform
    {
    public:
      
      bool filterMinimumDelta(const DataItem *di, double value)
      {
        auto last = m_lastSampleValue.find(di->getId());
        double fv = di->getFilterValue();
        if (last != m_lastSampleValue.end())
        {
          double lv = last->second;
          if (value > (lv - fv) && value < (lv + fv))
          {
            return false;
          }
          last->second = value;
        }
        else
        {
          m_lastSampleValue[di->getId()] = value;
        }
        
        return true;
      }
      
      bool filterPeriod(const DataItem *di, Timestamp &value)
      {
        using namespace std;
        auto last = m_lastTimeOffset.find(di->getId());
        auto fv = chrono::duration<double>(di->getFilterPeriod());
        if (last != m_lastTimeOffset.end())
        {
          auto lv = last->second;
          if (value < (lv + (fv)))
          {
            return false;
          }
          last->second = value;
        }
        else
        {
          m_lastTimeOffset[di->getId()] = value;
        }
        
        return true;
      }
            
      const EntityPtr operator()(const EntityPtr entity) override
      {
        using namespace std;
        using namespace observation;
        if (auto o = std::dynamic_pointer_cast<Observation>(entity);
            o)
        {
          auto di = o->getDataItem();
          if (o->isUnavailable())
          {
            m_lastSampleValue.erase(di->getId());
            m_lastTimeOffset.erase(di->getId());
            return next(entity);
          }
          
          if (di->isSample() && di->hasMinimumDelta())
          {
            double value = o->getValue<double>();
            if (!filterMinimumDelta(di, value))
              return EntityPtr();
          }
                    
          if (di->hasMinimumPeriod())
          {
            auto value = o->getTimestamp();
            if (!filterPeriod(di, value))
              return EntityPtr();
          }
        }
        
        return next(entity);
      }
      
      void bindTo(TransformPtr trans)
      {
        // Event, Sample, Timeseries, DataSetEvent, Message, Alarm,
        // AssetEvent, ThreeSpaceSmple, Condition, AssetEvent
        using namespace observation;
        trans->bind<Event, Sample, Message, Alarm>(this->getptr());
      }
      
    protected:
      std::unordered_map<std::string, double> m_lastSampleValue;
      std::unordered_map<std::string, Timestamp> m_lastTimeOffset;
    };
  }
}  // namespace mtconnect
