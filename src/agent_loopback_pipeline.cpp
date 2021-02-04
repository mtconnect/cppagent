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

#include "agent_loopback_pipeline.hpp"
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
  using namespace pipeline;
  void AgentLoopbackPipeline::build(const ConfigOptions &options)
  {
    clear();
    TransformPtr next = m_start;    
    
    if (IsOptionSet(m_options, "UpcaseDataItemValue"))
      next = next->bind(make_shared<UpcaseValue>());

    // Filter dups, by delta, and by period
    next = next->bind(make_shared<DuplicateFilter>(m_context));
    next = next->bind(make_shared<DeltaFilter>(m_context));
    next = next->bind(make_shared<PeriodFilter>(m_context));

    // Convert values
    if (IsOptionSet(m_options, "ConversionRequired"))
      next = next->bind(make_shared<ConvertSample>());

    // Deliver
    next->bind(make_shared<DeliverObservation>(m_context));    
  }  
}
