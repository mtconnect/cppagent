//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "entity.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;


namespace mtconnect
{
  namespace entity 
  {
    class JsonPrinter 
    {
    public:
      JsonPrinter() = default;

      json print(const EntityPtr entity) const;
      json GetValue(const Value &value) const;
    };
  }
}
