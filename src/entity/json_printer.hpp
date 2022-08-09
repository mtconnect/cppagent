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

#include "entity/entity.hpp"

using json = nlohmann::json;

namespace mtconnect {
  namespace entity {
    class JsonPrinter
    {
    public:
      JsonPrinter(uint32_t version) : m_version(version) {};

      json print(const EntityPtr entity) const
      {
        return json::object({{entity->getName(), printEntity(entity)}});
      }
      json printEntity(const EntityPtr entity) const;

    protected:
      void printEntityList1(json &obj, const EntityList &list) const;
      void printEntityList2(json &obj, const EntityList &list) const;

    protected:
      uint32_t m_version;
    };
  }  // namespace entity
}  // namespace mtconnect
