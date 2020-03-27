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

#include <utility>
#include <vector>

namespace mtconnect
{
  struct Specification
  {
    Specification(const Specification &s) = default;
    Specification() = default;

    // Attributes
    std::string m_type;
    std::string m_subType;
    std::string m_units;
    std::string m_name;
    std::string m_dataItemIdRef;
    std::string m_compositionIdRef;
    std::string m_coordinateSystemIdRef;

    // Elements
    std::string m_maximum;
    std::string m_minimum;
    std::string m_nominal;
  };

  class Specifications : public ComponentConfiguration
  {
   public:
    Specifications() = default;
    virtual ~Specifications() = default;

    const std::list<std::unique_ptr<Specification>> &getSpecifications() const
    {
      return m_specifications;
    }
    void addSpecification(Specification *s)
    {
      m_specifications.emplace_back(std::move(s));
    }
    void addSpecification(std::unique_ptr<Specification> &s)
    {
      m_specifications.emplace_back(std::move(s));
    }

   protected:
    std::list<std::unique_ptr<Specification>> m_specifications;
  };
}  // namespace mtconnect
