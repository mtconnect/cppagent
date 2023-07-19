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

#include "mtconnect/source/adapter/adapter_pipeline.hpp"

#include "mtconnect/agent.hpp"
#include "mtconnect/configuration/agent_config.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/pipeline/convert_sample.hpp"
#include "mtconnect/pipeline/deliver.hpp"
#include "mtconnect/pipeline/delta_filter.hpp"
#include "mtconnect/pipeline/duplicate_filter.hpp"
#include "mtconnect/pipeline/period_filter.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "mtconnect/pipeline/topic_mapper.hpp"
#include "mtconnect/pipeline/upcase_value.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using namespace std;
using namespace std::literals;

namespace mtconnect {
  using namespace observation;
  using namespace entity;
  using namespace pipeline;

  namespace source::adapter {
    std::unique_ptr<Handler> AdapterPipeline::makeHandler()
    {
      auto handler = make_unique<Handler>();

      // Build the pipeline for an adapter
      handler->m_connecting = [this](const std::string &id) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties {{"VALUE", "CONNECTING"s}, {"source", id}});
        run(std::move(entity));
      };
      handler->m_connected = [this](const std::string &id) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties {{"VALUE", "CONNECTED"s}, {"source", id}});
        run(std::move(entity));
      };
      handler->m_disconnected = [this](const std::string &id) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties {{"VALUE", "DISCONNECTED"s}, {"source", id}});
        run(std::move(entity));
      };
      handler->m_processData = [this](const std::string &data, const std::string &source) {
        auto entity = make_shared<Entity>("Data", Properties {{"VALUE", data}, {"source", source}});
        run(std::move(entity));
      };
      handler->m_processMessage = [this](const std::string &topic, const std::string &data,
                                         const std::string &source) {
        auto entity = make_shared<Entity>(
            "Message", Properties {{"VALUE", data}, {"topic", topic}, {"source", source}});
        run(std::move(entity));
      };
      handler->m_command = [this](const std::string &command, const std::string &value,
                                  const std::string &source) {
        auto entity = make_shared<Entity>(
            "Command", Properties {{"command", command}, {"VALUE", value}, {"source", source}});
        run(std::move(entity));
      };

      return handler;
    }

    void AdapterPipeline::build(const ConfigOptions &options)
    {
      clear();
      m_options = options;

      m_identity = GetOption<string>(m_options, configuration::AdapterIdentity).value_or("unknown");
    }

    void AdapterPipeline::buildDeviceList()
    {
      m_devices =
          GetOption<StringList>(m_options, configuration::AdditionalDevices).value_or(StringList());
      m_device = GetOption<string>(m_options, configuration::Device);
      if (m_device)
      {
        m_devices.emplace_front(*m_device);
        auto dp = m_context->m_contract->findDevice(*m_device);
        if (dp)
        {
          dp->setOptions(m_options);
        }
      }
    }

    void AdapterPipeline::buildCommandAndStatusDelivery(pipeline::TransformPtr next)
    {
      if (next == nullptr)
        next = m_start;

      next->bind(make_shared<DeliverConnectionStatus>(
          m_context, m_devices, IsOptionSet(m_options, configuration::AutoAvailable)));
      next->bind(make_shared<DeliverCommand>(m_context, m_device));
    }

    void AdapterPipeline::buildAssetDelivery(pipeline::TransformPtr next)
    {
      // Go directly to asset delivery
      std::optional<string> assetMetrics;
      assetMetrics = m_identity + "_asset_update_rate";

      next->bind(make_shared<DeliverAsset>(m_context, assetMetrics));
      next->bind(make_shared<DeliverAssetCommand>(m_context));
    }

    void AdapterPipeline::buildDeviceDelivery(pipeline::TransformPtr next)
    {
      next->bind(make_shared<DeliverDevice>(m_context));
    }

    void AdapterPipeline::buildObservationDelivery(pipeline::TransformPtr next)
    {
      // Uppercase Events
      if (IsOptionSet(m_options, configuration::UpcaseDataItemValue))
        next = next->bind(make_shared<UpcaseValue>());

      // Convert values
      if (IsOptionSet(m_options, configuration::ConversionRequired))
        next = next->bind(make_shared<ConvertSample>());

      // Filter dups, by delta, and by period
      next = next->bind(make_shared<DuplicateFilter>(m_context));
      next = next->bind(make_shared<DeltaFilter>(m_context));
      next = next->bind(make_shared<PeriodFilter>(m_context, m_strand));

      // Deliver
      std::optional<string> obsMetrics;
      obsMetrics = m_identity + "_observation_update_rate";
      next->bind(make_shared<DeliverObservation>(m_context, obsMetrics));
    }
  }  // namespace source::adapter
}  // namespace mtconnect
