//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/asio/bind_executor.hpp>
#include <boost/bind/bind.hpp>
#include <boost/bind/placeholders.hpp>

#include <chrono>

#include "mtconnect/agent.hpp"
#include "mtconnect/asset/cutting_tool.hpp"
#include "mtconnect/asset/file_asset.hpp"
#include "mtconnect/logging.hpp"

using namespace std::literals::chrono_literals;

namespace mtconnect {
  using namespace observation;
  using namespace entity;

  namespace pipeline {
    EntityPtr DeliverObservation::operator()(entity::EntityPtr &&entity)
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

    void ComputeMetrics::start()
    {
      m_timer.cancel();
      m_first = true;
      m_stopped = false;

      compute(boost::system::error_code());
    }

    void ComputeMetrics::compute(boost::system::error_code ec)

    {
      NAMED_SCOPE("pipeline.deliver");

      if (!ec && !m_stopped)
      {
        using namespace std;
        using namespace chrono;
        using namespace chrono_literals;

        if (!m_dataItem)
          return;

        auto di = m_contract->findDataItem("Agent", *m_dataItem);
        if (di == nullptr)
        {
          LOG(warning) << "Could not find data item: " << *m_dataItem << ", exiting metrics";
          return;
        }

        auto now = std::chrono::steady_clock::now();
        if (m_first)
        {
          m_last = 0;
          m_lastAvg = 0.0;
          m_lastTime = now;
          m_first = false;
        }
        else
        {
          std::chrono::duration<double> dt = now - m_lastTime;
          m_lastTime = now;

          auto count = *m_count;
          auto delta = count - m_last;

          double avg = delta + exp(-(dt.count() / 60.0)) * (m_lastAvg - delta);
          LOG(debug) << *m_dataItem << " - Average for last 1 minutes: " << (avg / dt.count());
          LOG(debug) << *m_dataItem
                     << " - Delta for last 10 seconds: " << (double(delta) / dt.count());

          m_last = count;
          if (avg != m_lastAvg)
          {
            ErrorList errors;
            auto obs = Observation::make(
                di, Properties {{"VALUE", double(delta) / 10.0}, {"duration", 10.0}},
                system_clock::now(), errors);
            m_contract->deliverObservation(obs);
            m_lastAvg = avg;
          }
        }

        using std::placeholders::_1;
        m_timer.expires_from_now(10s);
        m_timer.async_wait(
            boost::asio::bind_executor(m_strand, boost::bind(&ComputeMetrics::compute, ptr(), _1)));
      }
    }

    EntityPtr DeliverAsset::operator()(entity::EntityPtr &&entity)
    {
      auto a = std::dynamic_pointer_cast<asset::Asset>(entity);
      if (!a)
      {
        throw EntityError("Unexpected entity type, cannot convert to asset in DeliverAsset");
      }

      m_contract->deliverAsset(a);
      (*m_count)++;

      return entity;
    }

    EntityPtr DeliverDevice::operator()(entity::EntityPtr &&entity)
    {
      auto d = std::dynamic_pointer_cast<device_model::Device>(entity);
      if (!d)
      {
        throw EntityError("Unexpected entity type, cannot convert to asset in DeliverAsset");
      }

      m_contract->deliverDevice(d);

      return entity;
    }

    entity::EntityPtr DeliverConnectionStatus::operator()(entity::EntityPtr &&entity)
    {
      m_contract->deliverConnectStatus(entity, m_devices, m_autoAvailable);
      return entity;
    }

    entity::EntityPtr DeliverAssetCommand::operator()(entity::EntityPtr &&entity)
    {
      m_contract->deliverAssetCommand(entity);
      return entity;
    }

    entity::EntityPtr DeliverCommand::operator()(entity::EntityPtr &&entity)
    {
      if (m_defaultDevice)
        entity->setProperty("device", *m_defaultDevice);
      m_contract->deliverCommand(entity);
      return entity;
    }

  }  // namespace pipeline
}  // namespace mtconnect
