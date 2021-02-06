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
    static dlib::logger g_logger("agent_device");

    const EntityPtr DeliverObservation::operator()(const EntityPtr entity)
    {
      using namespace observation;
      auto o = std::dynamic_pointer_cast<Observation>(entity);
      if (!o)
      {
        throw EntityError(
            "Unexpected entity type, cannot convert to observation in DeliverObservation");
      }

      m_contract->deliverObservation(o);
      (*m_count)++;

      return entity;
    }
    
    void ComputeMetrics::operator()()

    {
      using namespace std;
      using namespace chrono;
      using namespace chrono_literals;
      
      if (!m_dataItem)
        return;
      
      auto di = m_contract->findDataItem("Agent", *m_dataItem);
      if (di == nullptr)
        return;
            
      size_t last{0};
      double lastAvg{0.0};
      while (m_running)
      {
        auto count = *m_count;
        auto delta = count - last;
        
        double avg = delta + exp(-(10.0/60.0)) * (lastAvg - delta);
        g_logger << dlib::LDEBUG << *m_dataItem << " - Agerage for last 1 minutes: " << (avg/10.0);
        g_logger << dlib::LDEBUG << *m_dataItem << " - Delta for last 10 seconds: " << (double(delta)/10.0);

        last = count;
        if (avg != lastAvg)
        {
          ErrorList errors;
          auto obs = Observation::make(di, Properties{{"VALUE", double(delta)/10.0 }},
                                       system_clock::now(), errors);
          m_contract->deliverObservation(obs);
          lastAvg = avg;
        }
        this_thread::sleep_for(10s);
      }
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

    const entity::EntityPtr DeliverConnectionStatus::operator()(const entity::EntityPtr entity)
    {
      m_contract->deliverConnectStatus(entity, m_devices, m_autoAvailable);
      return entity;
    }

    const entity::EntityPtr DeliverAssetCommand::operator()(const entity::EntityPtr entity)
    {
      m_contract->deliverAssetCommand(entity);
      return entity;
    }

    const entity::EntityPtr DeliverCommand::operator()(const entity::EntityPtr entity)
    {
      if (m_defaultDevice)
        entity->setProperty("device", *m_defaultDevice);
      m_contract->deliverCommand(entity);
      return entity;
    }

  }  // namespace pipeline
}  // namespace mtconnect
