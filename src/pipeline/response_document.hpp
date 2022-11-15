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

#include "entity/entity.hpp"
#include "pipeline/pipeline_context.hpp"
#include "printer/xml_helper.hpp"
#include "utilities.hpp"

namespace mtconnect::pipeline {
  using namespace mtconnect;

  struct ResponseDocument
  {
    struct Error
    {
      std::string m_code;
      std::string m_message;
    };
    using Errors = std::list<Error>;

    static bool parse(const std::string_view &content, ResponseDocument &doc,
                      pipeline::PipelineContextPtr context,
                      const std::optional<std::string> &device = std::nullopt);

    // Parsed data
    SequenceNumber_t m_next;
    uint64_t m_instanceId;
    entity::EntityList m_entities;
    entity::EntityList m_assetEvents;
    Errors m_errors;
  };

}  // namespace mtconnect::pipeline
