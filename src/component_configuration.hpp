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

#include "globals.hpp"
#include "geometry.hpp"

#include <map>
#include <string>
#include <utility>

namespace mtconnect
{
  class ComponentConfiguration
  {
   public:
    ComponentConfiguration() = default;
    virtual ~ComponentConfiguration() = default;
  };

  class ExtendedComponentConfiguration : public ComponentConfiguration
  {
   public:
    ExtendedComponentConfiguration(const std::string &content) : m_content(content)
    {
    }
    ~ExtendedComponentConfiguration() override = default;

    const std::string &getContent() const
    {
      return m_content;
    }

   protected:
    std::string m_content;
  };
  
  class GeometricConfiguration : public ComponentConfiguration
  {
  public:
    GeometricConfiguration() = default;
    virtual ~GeometricConfiguration() = default;
    
    virtual const std::map<std::string, bool> &properties() const = 0;
    virtual const std::string &klass() const = 0;
    virtual bool hasScale() const { return false; }
    virtual bool hasAxis() const { return false; }

    std::map<std::string, std::string> m_attributes;
    std::optional<Geometry> m_geometry;
  };
}  // namespace mtconnect
