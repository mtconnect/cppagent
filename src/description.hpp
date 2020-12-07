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

#include "component_configuration.hpp"
#include "globals.hpp"

#include <list>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace mtconnect
{
  class Description
  {
   public:
    Description() = default;
    Description(const Description &another) = default;
    Description(std::string body) : m_body(std::move(body)) {}
    ~Description() = default;

    const std::map<std::string, bool> &properties() const
    {
      const static std::map<std::string, bool> properties = {
          {"manufacturer", false}, {"model", false}, {"serialNumber", false}, {"station", false}};
      ;
      return properties;
    }

    std::string m_body;
    std::map<std::string, std::string> m_attributes;

   protected:
  };
}  // namespace mtconnect
