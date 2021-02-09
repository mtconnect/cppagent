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

#include "observation/observation.hpp"
#include "transform.hpp"

namespace mtconnect
{
  class Agent;
  namespace pipeline
  {
    class PeriodFilter : public Transform
    {
    public:
      struct State : TransformState
      {
        std::unordered_map<std::string, Timestamp> m_lastTimeOffset;
      };

      PeriodFilter(PipelineContextPtr context);
      ~PeriodFilter() override = default;

      const entity::EntityPtr operator()(const entity::EntityPtr entity) override
      {
        using namespace std;
        using namespace observation;
        using namespace entity;

        if (auto o = std::dynamic_pointer_cast<Observation>(entity);
            o && o->getDataItem()->hasMinimumPeriod())
        {
          std::lock_guard<TransformState> guard(*m_state);

          auto di = o->getDataItem();
          auto &id = di->getId();
          auto minDuration = chrono::duration<double>(di->getFilterPeriod());
          
          if (o->isUnavailable())
          {
            m_state->m_lastTimeOffset.erase(id);
            return next(entity);
          }
          
          auto ts = o->getTimestamp();
          if (filterPeriod(id, ts, minDuration))
            return EntityPtr();
        }
        return next(entity);
      }
      
    protected:
      bool filterPeriod(const std::string &id, Timestamp &ts,
                        const std::chrono::duration<double> md)
      {
        using namespace std;

        auto last = m_state->m_lastTimeOffset.find(id);
        if (last != m_state->m_lastTimeOffset.end())
        {
          auto lv = last->second;
          if (ts < (lv + md))
          {
            return true;
          }
          last->second = ts;
        }
        else
        {
          m_state->m_lastTimeOffset[id] = ts;
        }

        return false;
      }
      
    protected:
      std::shared_ptr<State> m_state;
      PipelineContract *m_contract;
    };
  }  // namespace pipeline
}  // namespace mtconnect
