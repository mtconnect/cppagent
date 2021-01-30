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
#include "agent.hpp"

namespace mtconnect
{
  namespace source
  {
    class DeliverObservation : public Transform
    {
    public:
      
      const EntityPtr operator()(const EntityPtr entity) override
      {
        using namespace observation;
        if (auto &o = std::dynamic_pointer_cast<Observation>(entity); m_agent && o)
        {
          m_agent->addToBuffer(o);
        }
        else
        {
          throw EntityError("Unexpected entity type, cannot convert to observation in DeliverObservation");
        }
        return entity;	
      }
      
      void bindTo(TransformPtr trans)
      {
        // Event, Sample, Timeseries, DataSetEvent, Message, Alarm,
        // AssetEvent, ThreeSpaceSmple, Condition, AssetEvent
        using namespace observation;
        trans->bind<Event, Sample, Timeseries, DataSetEvent, Message, Alarm,
                    AssetEvent, ThreeSpaceSmple, Condition, AssetEvent>(this->getptr());
      }

      Agent *m_agent { nullptr };
      
    protected:
    };

    class DeliverAsset : public Transform
    {
    public:
      
      const EntityPtr operator()(const EntityPtr entity) override
      {
        if (auto &a = std::dynamic_pointer_cast<Asset>(entity); m_agent && a)
        {
          // m_agent->addAsset(a);
        }
        else
        {
          throw EntityError("Unexpected entity type, cannot convert to asset in DeliverObservation");
        }

        return entity;
      }
      
      void bindTo(TransformPtr trans)
      {
        trans->bind<Asset, CuttingToolArchetype, CuttingTool,
                    FileAsset, FileArchetypeAsset >(this->getptr());
      }

      Agent *m_agent { nullptr };
      
    protected:
    };
  }
}  // namespace mtconnect
