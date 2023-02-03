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

#include <nlohmann/json.hpp>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"

using json = nlohmann::json;

namespace mtconnect {
  namespace entity {
    /// @brief Serializes entities as JSON text
    class AGENT_LIB_API JsonPrinter
    {
    public:
      /// @brief Create a json printer
      /// @param version the supported MTConnect serialization version
      /// - Version 1 has a repreated objects in arrays for collections of objects
      /// - Version 2 combines arrays of objects by type
      JsonPrinter(uint32_t version) : m_version(version) {};

      /// @brief create a json object from an entity
      ///
      /// Cover method for `printEntity()`
      ///
      /// @param entity the entity
      /// @return json object
      json print(const EntityPtr entity) const
      {
        return json::object({{entity->getName(), printEntity(entity)}});
      }
      /// @brief Convert properties of an entity into a json object
      /// @param entity the entity
      /// @return json object
      json printEntity(const EntityPtr entity) const;

    protected:
      void printEntityList1(json &obj, const EntityList &list) const;
      void printEntityList2(json &obj, const EntityList &list) const;

    protected:
      uint32_t m_version;
    };
  }  // namespace entity
}  // namespace mtconnect
