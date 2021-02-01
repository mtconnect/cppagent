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
#include "observation/observation.hpp"
#include "assets/asset.hpp"

namespace mtconnect
{
  namespace pipeline
  {
    class DeliverObservation : public Transform
    {
    public:
      using Deliver = std::function<void(observation::ObservationPtr)>;
      DeliverObservation(PipelineContract *contract)
      : Transform("DeliverAsset"), m_contract(contract)
      {
        m_guard = TypeGuard<observation::Observation>(RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;
      
    protected:
      PipelineContract *m_contract;
    };

    class DeliverAsset : public Transform
    {
    public:
      using Deliver = std::function<void(AssetPtr)>;
      DeliverAsset(PipelineContract *contract)
      : Transform("DeliverAsset"), m_contract(contract)
      {
        m_guard = TypeGuard<Asset>(RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;
      
    protected:
      PipelineContract *m_contract;
    };
    
    class DeliverConnectionStatus : public Transform
    {
      using Deliver = std::function<void(entity::EntityPtr)>;
      DeliverConnectionStatus(PipelineContract *contract)
      : Transform("DeliverConnectionStatus"), m_contract(contract)
      {
        m_guard = EntityNameGuard("ConnectionStatus", RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;
      
    protected:
      PipelineContract *m_contract;
    };
    
    class DeliverAssetCommand : public Transform
    {
      using Deliver = std::function<void(entity::EntityPtr)>;
      DeliverAssetCommand(PipelineContract *contract)
      : Transform("AssetCommand"), m_contract(contract)
      {
        m_guard = EntityNameGuard("ConnectionStatus", RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;
      
    protected:
      PipelineContract *m_contract;
    };

    class DeliverProcessCommand : public Transform
    {
      using Deliver = std::function<void(entity::EntityPtr)>;
      DeliverProcessCommand(PipelineContract *contract)
      : Transform("DeliverProcessCommand"), m_contract(contract)
      {
        m_guard = EntityNameGuard("ProcessCommand", RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;
      
    protected:
      PipelineContract *m_contract;
    };
  }
}  // namespace mtconnect
