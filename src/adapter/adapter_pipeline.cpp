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

#include "adapter_pipeline.hpp"

#include "agent.hpp"
#include "config.hpp"
#include "config_options.hpp"
#include "pipeline/convert_sample.hpp"
#include "pipeline/deliver.hpp"
#include "pipeline/delta_filter.hpp"
#include "pipeline/duplicate_filter.hpp"
#include "pipeline/period_filter.hpp"
#include "pipeline/shdr_token_mapper.hpp"
#include "pipeline/shdr_tokenizer.hpp"
#include "pipeline/timestamp_extractor.hpp"

using namespace std;
using namespace std::literals;

namespace mtconnect
{
  using namespace observation;
  using namespace entity;
  using namespace entity;
  using namespace pipeline;

  namespace adapter
  {
    std::unique_ptr<adapter::Handler> AdapterPipeline::makeHandler()
    {
      auto handler = make_unique<adapter::Handler>();

      // Build the pipeline for an adapter
      handler->m_connecting = [this](const std::string &id) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties{{"VALUE", "CONNECTING"s}, {"source", id}});
        run(entity);
      };
      handler->m_connected = [this](const std::string &id) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties{{"VALUE", "CONNECTED"s}, {"source", id}});
        run(entity);
      };
      handler->m_disconnected = [this](const std::string &id) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties{{"VALUE", "DISCONNECTED"s}, {"source", id}});
        run(entity);
      };
      handler->m_processData = [this](const std::string &data, const std::string &source) {
        auto entity = make_shared<Entity>("Data", Properties{{"VALUE", data}, {"source", source}});
        run(entity);
      };
      handler->m_command = [this](const std::string &data, const std::string &source) {
        auto entity =
            make_shared<Entity>("Command", Properties{{"VALUE", data}, {"source", source}});
        run(entity);
      };

      return handler;
    }

    void AdapterPipeline::build(const ConfigOptions &options)
    {
      clear();
      m_options = options;

      TransformPtr next = bind(make_shared<ShdrTokenizer>());
      auto identity = GetOption<string>(options, configuration::AdapterIdentity);

      StringList devices;
      auto list = GetOption<StringList>(options, configuration::AdditionalDevices);
      if (list)
        devices = *list;
      auto device = GetOption<string>(options, configuration::Device);
      if (device)
      {
        devices.emplace_front(*device);
        auto dp = m_context->m_contract->findDevice(*device);
        if (dp)
        {
          dp->setOptions(options);
        }
      }

      bind(make_shared<DeliverConnectionStatus>(m_context, devices,
                                                IsOptionSet(options, configuration::AutoAvailable)));
      bind(make_shared<DeliverCommand>(m_context, device));

      // Optional type based transforms
      if (IsOptionSet(m_options, configuration::IgnoreTimestamps))
        next = next->bind(make_shared<IgnoreTimestamp>());
      else
      {
        auto extract = make_shared<ExtractTimestamp>(IsOptionSet(m_options, configuration::RelativeTime));
        next = next->bind(extract);
      }

      // Token mapping to data items and assets
      auto mapper = make_shared<ShdrTokenMapper>(
          m_context, GetOption<string>(m_options, configuration::Device).value_or(""),
                     GetOption<int>(m_options, configuration::ShdrVersion).value_or(1));
      next = next->bind(mapper);

      // Handle the observations and send to nowhere
      mapper->bind(make_shared<NullTransform>(TypeGuard<Observations>(RUN)));

      // Go directly to asset delivery
      std::optional<string> assetMetrics;
      if (identity)
        assetMetrics = *identity + "_asset_update_rate";

      mapper->bind(make_shared<DeliverAsset>(m_context, assetMetrics));
      mapper->bind(make_shared<DeliverAssetCommand>(m_context));

      // Uppercase Events
      if (IsOptionSet(m_options, configuration::UpcaseDataItemValue))
        next = next->bind(make_shared<UpcaseValue>());

      // Filter dups, by delta, and by period
      next = next->bind(make_shared<DuplicateFilter>(m_context));
      next = next->bind(make_shared<DeltaFilter>(m_context));
      next = next->bind(make_shared<PeriodFilter>(m_context));

      // Convert values
      if (IsOptionSet(m_options, configuration::ConversionRequired))
        next = next->bind(make_shared<ConvertSample>());

      // Deliver
      std::optional<string> obsMetrics;
      if (identity)
        obsMetrics = *identity + "_observation_update_rate";
      next->bind(make_shared<DeliverObservation>(m_context, obsMetrics));
    }
  }  // namespace adapter
}  // namespace mtconnect
