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

#include <string>
#include <unordered_map>

#include "../utilities.hpp"

namespace mtconnect {

  /// @brief MTConnect validation containers
  namespace validation {

    /// @brief Observation validation containers
    namespace observations {

      /// @brief Validation type for observations
      using Validation =
          std::unordered_map<std::string,
                             std::unordered_map<std::string, std::pair<int32_t, int32_t>>>;

      /// @brief Global Validations for Event Observation's Controlled Vocabularies
      ///
      /// The map is as follows:
      /// * Event name -->
      ///   * Map of valid values. Empty map if not controlled.
      ///     * Map has is a pair of value to
      ///       * 0 if not deprecated
      ///       * SCHEMA_VERSION if deprecated
      extern Validation ControlledVocabularies;
    }  // namespace observations
  }    // namespace validation
}  // namespace mtconnect
