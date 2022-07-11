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

#pragma once

#include <boost/asio/steady_timer.hpp>

#include <chrono>

#include "asset/asset.hpp"
#include "observation/observation.hpp"
#include "transform.hpp"

namespace mtconnect {
  namespace pipeline {
    struct ComputeMetrics : std::enable_shared_from_this<ComputeMetrics>
    {
      ComputeMetrics(boost::asio::io_context::strand &st, PipelineContract *contract,
                     const std::optional<std::string> &dataItem, std::shared_ptr<size_t> &count)
        : m_count(count),
          m_contract(contract),
          m_dataItem(dataItem),
          m_strand(st),
          m_timer(st.context())
      {}

      std::shared_ptr<ComputeMetrics> ptr() { return shared_from_this(); }

      void compute(boost::system::error_code ec);

      void stop()
      {
        m_stopped = true;
        m_timer.cancel();
      }

      void start();

      std::shared_ptr<size_t> m_count;
      PipelineContract *m_contract {nullptr};
      std::optional<std::string> m_dataItem;
      std::chrono::time_point<std::chrono::steady_clock> m_lastTime;

      boost::asio::io_context::strand &m_strand;
      boost::asio::steady_timer m_timer;
      bool m_first {true};
      bool m_stopped {false};
      size_t m_last {0};
      double m_lastAvg {0.0};
    };

    class MeteredTransform : public Transform
    {
    public:
      MeteredTransform(const std::string &name, PipelineContextPtr context,
                       const std::optional<std::string> &metricsDataItem = std::nullopt)
        : Transform(name),
          m_contract(context->m_contract.get()),
          m_count(std::make_shared<size_t>(0)),
          m_dataItem(metricsDataItem)
      {}

      ~MeteredTransform() override
      {
        if (m_metrics)
          m_metrics->stop();
      }

      void stop() override
      {
        if (m_metrics)
          m_metrics->stop();

        Transform::stop();
      }

      void start(boost::asio::io_context::strand &st) override
      {
        if (m_dataItem)
        {
          m_metrics = std::make_shared<ComputeMetrics>(st, m_contract, m_dataItem, m_count);
          m_metrics->start();
        }
        Transform::start(st);
      }

    protected:
      friend struct ComputeMetrics;

      PipelineContract *m_contract;
      std::shared_ptr<size_t> m_count;
      std::shared_ptr<ComputeMetrics> m_metrics;
      std::optional<std::string> m_dataItem;
    };

    class DeliverObservation : public MeteredTransform
    {
    public:
      using Deliver = std::function<void(observation::ObservationPtr)>;
      DeliverObservation(PipelineContextPtr context,
                         const std::optional<std::string> &metricDataItem = std::nullopt)
        : MeteredTransform("DeliverObservation", context, metricDataItem)
      {
        m_guard = TypeGuard<observation::Observation>(RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;
    };

    class DeliverAsset : public MeteredTransform
    {
    public:
      using Deliver = std::function<void(asset::AssetPtr)>;
      DeliverAsset(PipelineContextPtr context,
                   const std::optional<std::string> &metricsDataItem = std::nullopt)
        : MeteredTransform("DeliverAsset", context, metricsDataItem)
      {
        m_guard = TypeGuard<asset::Asset>(RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;
    };

    class DeliverConnectionStatus : public Transform
    {
    public:
      using Deliver = std::function<void(entity::EntityPtr)>;
      DeliverConnectionStatus(PipelineContextPtr context, const StringList &devices,
                              bool autoAvailable)
        : Transform("DeliverConnectionStatus"),
          m_contract(context->m_contract.get()),
          m_devices(devices),
          m_autoAvailable(autoAvailable)
      {
        m_guard = EntityNameGuard("ConnectionStatus", RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;

    protected:
      PipelineContract *m_contract;
      std::list<std::string> m_devices;
      bool m_autoAvailable;
    };

    class DeliverAssetCommand : public Transform
    {
    public:
      using Deliver = std::function<void(entity::EntityPtr)>;
      DeliverAssetCommand(PipelineContextPtr context)
        : Transform("DeliverAssetCommand"), m_contract(context->m_contract.get())
      {
        m_guard = EntityNameGuard("AssetCommand", RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;

    protected:
      PipelineContract *m_contract;
    };

    class DeliverCommand : public Transform
    {
    public:
      using Deliver = std::function<void(entity::EntityPtr)>;
      DeliverCommand(PipelineContextPtr context, const std::optional<std::string> &device)
        : Transform("DeliverCommand"),
          m_contract(context->m_contract.get()),
          m_defaultDevice(device)
      {
        m_guard = EntityNameGuard("Command", RUN);
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override;

    protected:
      PipelineContract *m_contract;
      std::optional<std::string> m_defaultDevice;
    };
  }  // namespace pipeline
}  // namespace mtconnect
