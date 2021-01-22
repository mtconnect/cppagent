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
#include "description.hpp"
#include "globals.hpp"

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace mtconnect
{
  class Composition
  {
  public:
    Composition() = default;
    Composition(const Composition &) = default;
    ~Composition() = default;

    const std::map<std::string, bool> &properties() const
    {
      const static std::map<std::string, bool> properties = {
          {"id", true}, {"uuid", false}, {"name", false}, {"type", true}};
      ;
      return properties;
    }

    const std::optional<Description> &getDescription() const { return m_description; }

    void setDescription(Description &d) { m_description = d; }

    const auto &getConfiguration() const { return m_configuration; }

    void addConfiguration(std::unique_ptr<ComponentConfiguration> &config)
    {
      m_configuration.emplace_back(std::move(config));
    }

    std::map<std::string, std::string> m_attributes;

  protected:
    std::list<std::unique_ptr<ComponentConfiguration>> m_configuration;
    std::optional<Description> m_description;
  };
}  // namespace mtconnect
