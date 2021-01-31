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

#include "pipeline.hpp"
#include "agent.hpp"
#include "config.hpp"
#include "shdr_tokenizer.hpp"
#include "shdr_token_mapper.hpp"
#include "duplicate_filter.hpp"
#include "rate_filter.hpp"
#include "timestamp_extractor.hpp"
#include "convert_sample.hpp"
#include "deliver.hpp"

using namespace std;

namespace mtconnect
{
  using namespace adapter;
  using namespace observation;
  using namespace entity;
  
  namespace pipeline
  {
    AdapterPipeline::AdapterPipeline(const ConfigOptions &options, Agent *agent,
                                     Adapter *adapter)
    : Pipeline(options, agent), m_adapter(adapter),
        m_handler(make_unique<adapter::Handler>())
    {
      // Build the pipeline for an adapter
      m_handler->m_connecting = [](const Adapter &adapter){};
      m_handler->m_connected = [](const Adapter &adapter){};
      m_handler->m_disconnected = [](const Adapter &adapter){};
      
      build();
    }

    void AdapterPipeline::build()
    {
      m_handler->m_processData = [this](const std::string &data) {
        auto entity = make_shared<Entity>("Data", Properties{{"VALUE", data}});
        run(entity);
      };
      m_adapter->setHandler(m_handler);

      TransformPtr next = m_start = make_shared<ShdrTokenizer>();

      // Optional type based transforms
      if (IsOptionSet(m_options, "IgnoreTimestamps"))
        next = next->bind(make_shared<IgnoreTimestamp>());
      else
      {
        auto extract = make_shared<ExtractTimestamp>();
        if (IsOptionSet(m_options, "RelativeTime"))
          extract->m_relativeTime = true;
        next = next->bind(extract);
      }

      // Map to data items
      //m_handler->m_protocolCommand...
      
      // Token mapping to data items and assets
      auto mapper = make_shared<ShdrTokenMapper>();
      next = next->bind(mapper);
      mapper->m_getDevice = [this](const std::string &id){
        return m_agent->findDeviceByUUIDorName(id);
      };
      mapper->m_getDataItem = [](const Device *device, const std::string &id){
        return device->getDeviceDataItem(id);
      };
      
      // Handle the observations and send to nowhere
      mapper->bind(make_shared<NullTransform>(TypeGuard<Observations>()));

      // Go directly to asset delivery
      auto asset = mapper->bind(make_shared<DeliverAsset>());
            
      // Branched flow
      if (IsOptionSet(m_options, "UpcaseDataItemValue"))
        next = next->bind(make_shared<UpcaseValue>());
      
      if (IsOptionSet(m_options, "FilterDuplicates"))
        next = next->bind(make_shared<DuplicateFilter>());
      auto rate = make_shared<RateFilter>(m_agent);      
      next = next->bind(rate);

      // Convert values
      if (IsOptionSet(m_options, "ConversionRequired"))
        next = next->bind(make_shared<ConvertSample>());
      
      // Deliver
      next->bind(make_shared<DeliverObservation>([this](ObservationPtr obs){
        m_agent->addToBuffer(obs);
      }));
    }

  }
}

 
