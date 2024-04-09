//
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

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/pipeline/pipeline_context.hpp"
#include "mtconnect/printer/xml_helper.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::pipeline {
  using namespace mtconnect;

  /// @brief Utility class for parsing XML response document
  struct AGENT_LIB_API ResponseDocument
  {
    enum EntityType
    {
      UNKNOWN,
      DEVICE,
      OBSERVATION,
      ASSET,
      ERRORS
    };

    /// @brief An error document response from the agent
    struct Error
    {
      std::string m_code;
      std::string m_message;
    };
    using Errors = std::list<Error>;

    /// @brief parse the content of the XML document
    /// @param[in] content XML document
    /// @param[out] doc the created response document
    /// @param[in] context pipeline context
    /// @param[in] device optional device uuid
    /// @return `true` if successful
    static bool parse(const std::string_view &content, ResponseDocument &doc,
                      pipeline::PipelineContextPtr context,
                      const std::optional<std::string> &device = std::nullopt,
                      const std::optional<std::string> &uuid = std::nullopt);

    // Parsed data
    SequenceNumber_t m_next;           ///< Next sequence number
    uint64_t m_instanceId;             ///< Agent instance id
    int32_t m_agentVersion = 0;        ///< Agent version
    entity::EntityList m_entities;     ///< List of entities
    entity::EntityList m_assetEvents;  ///< List of asset events
    Errors m_errors;                   ///< List of Errors
    EntityType m_enityType {UNKNOWN};  ///< Entity Type in Collection
  };
}  // namespace mtconnect::pipeline
