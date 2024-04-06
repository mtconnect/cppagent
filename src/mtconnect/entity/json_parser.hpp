//
// Copyright Copyright 2009-2024, AMT � The Association For Manufacturing Technology (�AMT�)
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

#include <map>
#include <utility>
#include <vector>

#include "factory.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/utilities.hpp"
#include "requirement.hpp"

namespace mtconnect {
  namespace entity {
    /// @brief Parser to convert a JSON document to an entity
    class AGENT_LIB_API JsonParser
    {
    public:
      /// @brief Create a json parser
      /// @param version the supported MTConnect serialization version
      /// - Version 1 has a repreated objects in arrays for collections of objects
      /// - Version 2 combines arrays of objects by type
      JsonParser(uint32_t version = 1) : m_version(version) {}
      ~JsonParser() = default;

      /// @brief Parse a JSON document to an entity
      /// @param factory The top level factory for parsing
      /// @param document The json document
      /// @param version the version to parse
      /// @param errors Errors that occurred creating the entities
      /// @return an entity shared pointer if successful
      EntityPtr parse(FactoryPtr factory, const std::string &document, const std::string &version,
                      ErrorList &errors);

    protected:
      uint32_t m_version;
    };
  }  // namespace entity
}  // namespace mtconnect
