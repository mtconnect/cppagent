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
#include "pipeline/convert_sample.hpp"
#include "pipeline/deliver.hpp"
#include "pipeline/delta_filter.hpp"
#include "pipeline/duplicate_filter.hpp"
#include "pipeline/period_filter.hpp"
#include "pipeline/shdr_token_mapper.hpp"
#include "pipeline/shdr_tokenizer.hpp"
#include "pipeline/timestamp_extractor.hpp"

using namespace std;

namespace mtconnect
{
  using namespace observation;
  using namespace entity;
  using namespace entity;
  using namespace pipeline;

  namespace adapter
  {
    AdapterPipeline::AdapterPipeline(const ConfigOptions &options, PipelineContextPtr context)
      : Pipeline(options, context)
    {
    }

    std::unique_ptr<adapter::Handler> AdapterPipeline::makeHandler()
    {
      auto handler = make_unique<adapter::Handler>();

      // Build the pipeline for an adapter
      handler->m_connecting = [this](const Adapter &adapter) {
        auto entity = make_shared<Entity>(
            "ConnectionStatus", Properties{{"VALUE", "CONNECTING"}, {"id", adapter.getIdentity()}});
        run(entity);
      };
      handler->m_connected = [this](const Adapter &adapter) {
        auto entity = make_shared<Entity>(
            "ConnectionStatus", Properties{{"VALUE", "CONNECTED"}, {"id", adapter.getIdentity()}});
        run(entity);
      };
      handler->m_disconnected = [this](const Adapter &adapter) {
        auto entity = make_shared<Entity>(
            "ConnectionStatus",
            Properties{{"VALUE", "DISCONNECTED"}, {"id", adapter.getIdentity()}});
        run(entity);
      };
      handler->m_processData = [this](const std::string &data) {
        auto entity = make_shared<Entity>("Data", Properties{{"VALUE", data}});
        run(entity);
      };
      handler->m_protocolCommand = [this](const std::string &data) {
        auto entity = make_shared<Entity>("ProtocolCommand", Properties{{"VALUE", data}});
        run(entity);
      };

      return handler;
    }

    void AdapterPipeline::build()
    {
      clear();
      TransformPtr next = bind(make_shared<ShdrTokenizer>());
      bind(make_shared<DeliverConnectionStatus>(m_context));
      bind(make_shared<DeliverCommand>(m_context));

      // Optional type based transforms
      if (IsOptionSet(m_options, "IgnoreTimestamps"))
        next = next->bind(make_shared<IgnoreTimestamp>());
      else
      {
        auto extract = make_shared<ExtractTimestamp>(IsOptionSet(m_options, "RelativeTime"));
        next = next->bind(extract);
      }

      // Token mapping to data items and assets
      auto mapper = make_shared<ShdrTokenMapper>(m_context);
      next = next->bind(mapper);

      // Handle the observations and send to nowhere
      mapper->bind(make_shared<NullTransform>(TypeGuard<Observations>(RUN)));

      // Go directly to asset delivery
      mapper->bind(make_shared<DeliverAsset>(m_context));
      mapper->bind(make_shared<DeliverAssetCommand>(m_context));

      // Uppercase Events
      if (IsOptionSet(m_options, "UpcaseDataItemValue"))
        next = next->bind(make_shared<UpcaseValue>());

      // Filter dups, by delta, and by period
      if (IsOptionSet(m_options, "FilterDuplicates"))
      {
        next = next->bind(make_shared<DuplicateFilter>(m_context));
      }
      next = next->bind(make_shared<DeltaFilter>(m_context));
      next = next->bind(make_shared<PeriodFilter>(m_context));

      // Convert values
      if (IsOptionSet(m_options, "ConversionRequired"))
        next = next->bind(make_shared<ConvertSample>());

      // Deliver
      next->bind(make_shared<DeliverObservation>(m_context));
    }
  }  // namespace adapter
}  // namespace mtconnect
