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

#include "shdr_pipeline.hpp"

#include "mtconnect/agent.hpp"
#include "mtconnect/configuration/agent_config.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/pipeline/convert_sample.hpp"
#include "mtconnect/pipeline/deliver.hpp"
#include "mtconnect/pipeline/delta_filter.hpp"
#include "mtconnect/pipeline/duplicate_filter.hpp"
#include "mtconnect/pipeline/period_filter.hpp"
#include "mtconnect/pipeline/shdr_token_mapper.hpp"
#include "mtconnect/pipeline/shdr_tokenizer.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "mtconnect/pipeline/upcase_value.hpp"
#include "shdr_adapter.hpp"

using namespace std;
using namespace std::literals;

namespace mtconnect {
  using namespace observation;
  using namespace entity;
  using namespace pipeline;

  namespace source::adapter::shdr {
    void ShdrPipeline::build(const ConfigOptions &options)
    {
      AdapterPipeline::build(options);
      buildDeviceList();

      buildCommandAndStatusDelivery();

      TransformPtr next = bind(make_shared<ShdrTokenizer>());

      // Optional type based transforms
      if (IsOptionSet(m_options, configuration::IgnoreTimestamps))
        next = next->bind(make_shared<IgnoreTimestamp>());
      else
      {
        auto extract =
            make_shared<ExtractTimestamp>(IsOptionSet(m_options, configuration::RelativeTime));
        next = next->bind(extract);
      }

      // Token mapping to data items and asset
      auto mapper = make_shared<ShdrTokenMapper>(
          m_context, m_device.value_or(""),
          GetOption<int>(m_options, configuration::ShdrVersion).value_or(1));

      buildAssetDelivery(mapper);
      mapper->bind(make_shared<NullTransform>(TypeGuard<Observations>(RUN)));

      next = next->bind(mapper);

      // Handle the observations and send to nowhere
      buildObservationDelivery(next);
      applySplices();
    }
  }  // namespace source::adapter::shdr
}  // namespace mtconnect
