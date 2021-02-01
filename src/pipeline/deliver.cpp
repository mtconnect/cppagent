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

#include "deliver.hpp"
#include "agent.hpp"
#include "assets/cutting_tool.hpp"
#include "assets/file_asset.hpp"

namespace mtconnect
{
  using namespace observation;
  using namespace entity;
  
  namespace pipeline
  {
    const EntityPtr DeliverObservation::operator()(const EntityPtr entity)
    {
      using namespace observation;
      auto o = std::dynamic_pointer_cast<Observation>(entity);
      if (!o)
      {
        throw EntityError("Unexpected entity type, cannot convert to observation in DeliverObservation");
      }

      m_contract->deliverObservation(o);
      
      return entity;
    }

    const EntityPtr DeliverAsset::operator()(const EntityPtr entity)
    {
      auto a = std::dynamic_pointer_cast<Asset>(entity);
      if (!a)
      {
        throw EntityError("Unexpected entity type, cannot convert to asset in DeliverObservation");
      }
      
      m_contract->deliverAsset(a);

      return entity;
    }    
  }
}

 
