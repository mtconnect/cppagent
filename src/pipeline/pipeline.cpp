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
      m_handler->m_processData = [](const std::string &){};
      m_handler->m_protocolCommand = [](const std::string &){};
      m_handler->m_connecting = [](const Adapter &adapter){};
      m_handler->m_connected = [](const Adapter &adapter){};
      m_handler->m_disconnected = [](const Adapter &adapter){};
    }

    void AdapterPipeline::build()
    {
      TransformPtr next;
      next = m_start = make_shared<ShdrTokenizer>();
      m_handler->m_processData = [this](const std::string &data) {
        auto entity = make_shared<Entity>("Data", Properties{{"VALUE", data}});
        run(entity);
      };
      auto mapper = make_shared<ShdrTokenMapper>();
      mapper->m_getDevice = [this](const std::string &id){
        return m_agent->findDeviceByUUIDorName(id);
      };
      mapper->m_getDataItem = [](const Device *device, const std::string &id){
        return device->getDeviceDataItem(id);
      };
      next = mapper->bindTo(next);
      
      // Optional type based transforms
      if (auto o = GetOption<bool>(m_options, "IgnoreTimestamps"); o && *o)
        next = make_shared<IgnoreTimestamp>()->bindTo(next);
      else
        next = make_shared<ExtractTimestamp>()->bindTo(next);
      if (auto o = GetOption<bool>(m_options, "UpcaseDataItemValue"); o && *o)
        next = make_shared<UpcaseValue>()->bindTo(next);
      if (auto o = GetOption<bool>(m_options, "FilterDuplicates"); o && *o)
        next = make_shared<DuplicateFilter>()->bindTo(next);
      next = make_shared<RateFilter>()->bindTo(next);
      
      // Scan DataItems for rate filters...
      
      // Convert values
      // TODO: Convert samples
      
      // Deliver
      next = make_shared<DeliverObservation>(m_agent)->bindTo(next);
    }

  }
}

 
