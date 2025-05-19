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

#include <boost/algorithm/string.hpp>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "topic_mapper.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  /// @brief JsonMapper does nothing, it is a placeholder for a json interpreter
  class AGENT_LIB_API JsonMapper : public Transform
  {
  public:
    JsonMapper(const JsonMapper &) = default;
    JsonMapper(PipelineContextPtr context) : Transform("JsonMapper"), m_context(context)
    {
      m_guard = TypeGuard<JsonMessage>(RUN);
    }

    /// @brief Use rapidjson to parse the json content. If there is an error, output the text and
    /// log the error.
    EntityPtr operator()(entity::EntityPtr &&entity) override;

  protected:
    PipelineContextPtr m_context;
  };

}  // namespace mtconnect::pipeline
