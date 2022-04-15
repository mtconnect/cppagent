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

#pragma once

#include "xml_helper.hpp"
#include "utilities.hpp"
#include "entity/entity.hpp"
#include "pipeline/pipeline_context.hpp"

namespace mtconnect::adapter::agent {
  using namespace mtconnect;
  using namespace adapter;
  
  struct NextSequence : public pipeline::TransformState
  {
    SequenceNumber_t m_next;
  };
  

  struct ResponseDocument
  {
    static bool parse(const std::string_view &content,
                      ResponseDocument &doc,
                      pipeline::PipelineContextPtr context);

    // Parsed data
    SequenceNumber_t m_next;
    entity::EntityList m_entities;
    entity::EntityList m_assetEvents;
  };
  

}
