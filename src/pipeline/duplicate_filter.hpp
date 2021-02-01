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
    class DuplicateFilter : public Transform
    {
    public:
      struct State : TransformState
      {
        std::unordered_map<std::string, entity::Value> m_values;
      };
      
      DuplicateFilter(const DuplicateFilter &) = default;
      DuplicateFilter(std::shared_ptr<State> state)
      : Transform("DuplicateFilter"), m_state(state)
      {
        using namespace observation;
        m_guard = ExactTypeGuard<Event, Sample, ThreeSpaceSample, Message>(RUN) ||
                  TypeGuard<Observation>(SKIP);
      }
      ~DuplicateFilter() override = default;
      
      const EntityPtr operator()(const EntityPtr entity) override
      {
        using namespace observation;
        if (auto o = std::dynamic_pointer_cast<Observation>(entity);
            o)
        {
          auto &values = m_state->m_values;
          
          auto di = o->getDataItem();
          if (!di->allowDups())
          {
            auto old = values.find(di->getId());
            if (old != values.end() && old->second == o->getValue())
              return EntityPtr();
            
            if (old == values.end())
              values[di->getId()] = o->getValue();
            else if (old->second != o->getValue())
              old->second = o->getValue();
          }
        }
        
        return next(entity);
      }
      
    protected:
      std::shared_ptr<State> m_state;
    };

  }
  
}  // namespace mtconnect
