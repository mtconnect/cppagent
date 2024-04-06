//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <atomic>
#include <chrono>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  /// @brief Utility class to periodically compute metrics
  struct ComputeMetrics : std::enable_shared_from_this<ComputeMetrics>
  {
    /// @brief Construct a metrics stats computer
    /// @param st the strand for async operations
    /// @param contract contract to use
    /// @param dataItem the data item to post the metrics
    /// @param count a shared count
    ComputeMetrics(boost::asio::io_context::strand &st, PipelineContract *contract,
                   const std::optional<std::string> &dataItem,
                   std::shared_ptr<std::atomic_size_t> &count)
      : m_count(count),
        m_contract(contract),
        m_dataItem(dataItem),
        m_strand(st),
        m_timer(st.context())
    {}

    std::shared_ptr<ComputeMetrics> ptr() { return shared_from_this(); }

    /// @brief the compute method that get called periodically
    /// @param ec an error code from the timer
    void compute(boost::system::error_code ec);

    /// @brief stop the timer and cancel computation
    void stop()
    {
      m_stopped = true;
      m_timer.cancel();
    }

    /// @brief start computation
    void start();

    std::shared_ptr<std::atomic_size_t> m_count;
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

  /// @brief A transform that computes metrics using `ComputeMetrics`
  class AGENT_LIB_API MeteredTransform : public Transform
  {
  public:
    /// @brief Construct an abstract metered transform
    /// @param name the name of the transform from the subclass
    /// @param context the pipeline context
    /// @param metricsDataItem the data item used for an observation when the metrics are updated
    MeteredTransform(const std::string &name, PipelineContextPtr context,
                     const std::optional<std::string> &metricsDataItem = std::nullopt)
      : Transform(name),
        m_contract(context->m_contract.get()),
        m_count(std::make_shared<std::atomic_size_t>(0)),
        m_dataItem(metricsDataItem)
    {}

    ~MeteredTransform() override
    {
      if (m_metrics)
        m_metrics->stop();
    }

    /// @brief Stop the metrics
    void stop() override
    {
      if (m_metrics)
        m_metrics->stop();

      Transform::stop();
    }

    /// @brief start the metrics
    /// @param st the context to post observations
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
    std::shared_ptr<std::atomic_size_t> m_count;
    std::shared_ptr<ComputeMetrics> m_metrics;
    std::optional<std::string> m_dataItem;
  };

  /// @brief A transform to deliver and meter observation delivery
  class AGENT_LIB_API DeliverObservation : public MeteredTransform
  {
  public:
    using Deliver = std::function<void(observation::ObservationPtr)>;
    DeliverObservation(PipelineContextPtr context,
                       const std::optional<std::string> &metricDataItem = std::nullopt)
      : MeteredTransform("DeliverObservation", context, metricDataItem)
    {
      m_guard = TypeGuard<observation::Observation>(RUN);
    }
    entity::EntityPtr operator()(entity::EntityPtr &&entity) override;
  };

  /// @brief A transform to deliver and meter asset delivery
  class AGENT_LIB_API DeliverAsset : public MeteredTransform
  {
  public:
    using Deliver = std::function<void(asset::AssetPtr)>;
    DeliverAsset(PipelineContextPtr context,
                 const std::optional<std::string> &metricsDataItem = std::nullopt)
      : MeteredTransform("DeliverAsset", context, metricsDataItem)
    {
      m_guard = TypeGuard<asset::Asset>(RUN);
    }
    entity::EntityPtr operator()(entity::EntityPtr &&entity) override;
  };

  /// @brief A transform to deliver a device
  class AGENT_LIB_API DeliverDevices : public Transform
  {
  public:
    using Deliver = std::function<void(asset::AssetPtr)>;
    DeliverDevices(PipelineContextPtr context)
      : Transform("DeliverDevices"), m_contract(context->m_contract.get())
    {
      m_guard = EntityNameGuard("Devices", RUN);
    }
    entity::EntityPtr operator()(entity::EntityPtr &&entity) override;

  protected:
    PipelineContract *m_contract;
  };

  /// @brief A transform to deliver a device
  class AGENT_LIB_API DeliverDevice : public Transform
  {
  public:
    using Deliver = std::function<void(asset::AssetPtr)>;
    DeliverDevice(PipelineContextPtr context)
      : Transform("DeliverDevice"), m_contract(context->m_contract.get())
    {
      m_guard = TypeGuard<device_model::Device>(RUN);
    }
    entity::EntityPtr operator()(entity::EntityPtr &&entity) override;

  protected:
    PipelineContract *m_contract;
  };

  /// @brief deliver the connection status of an adapter
  class AGENT_LIB_API DeliverConnectionStatus : public Transform
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
    entity::EntityPtr operator()(entity::EntityPtr &&entity) override;

  protected:
    PipelineContract *m_contract;
    std::list<std::string> m_devices;
    bool m_autoAvailable;
  };

  /// @brief Deliver an asset command
  class AGENT_LIB_API DeliverAssetCommand : public Transform
  {
  public:
    using Deliver = std::function<void(entity::EntityPtr)>;
    DeliverAssetCommand(PipelineContextPtr context)
      : Transform("DeliverAssetCommand"), m_contract(context->m_contract.get())
    {
      m_guard = EntityNameGuard("AssetCommand", RUN);
    }
    entity::EntityPtr operator()(entity::EntityPtr &&entity) override;

  protected:
    PipelineContract *m_contract;
  };

  /// @brief Deliver an adapter command
  class AGENT_LIB_API DeliverCommand : public Transform
  {
  public:
    using Deliver = std::function<void(entity::EntityPtr)>;
    DeliverCommand(PipelineContextPtr context, const std::optional<std::string> &device)
      : Transform("DeliverCommand"), m_contract(context->m_contract.get()), m_defaultDevice(device)
    {
      m_guard = EntityNameGuard("Command", RUN);
    }
    entity::EntityPtr operator()(entity::EntityPtr &&entity) override;

  protected:
    PipelineContract *m_contract;
    std::optional<std::string> m_defaultDevice;
  };
}  // namespace mtconnect::pipeline
