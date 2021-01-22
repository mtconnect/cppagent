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
#include "geometry.hpp"
#include "globals.hpp"

#include <map>
#include <utility>
#include <vector>

namespace mtconnect
{
  class SolidModel : public GeometricConfiguration
  {
  public:
    SolidModel(const SolidModel &s) = default;
    SolidModel() = default;
    ~SolidModel() override = default;

    const std::string &klass() const override
    {
      const static std::string &klass("SolidModel");
      return klass;
    }
    bool hasScale() const override { return true; }
    const std::map<std::string, bool> &properties() const override
    {
      const static std::map<std::string, bool> properties = {
          {"id", true},        {"solidModelIdRef", false},       {"itemRef", false},
          {"mediaType", true}, {"coordinateSystemIdRef", false}, {"href", false}};
      ;
      return properties;
    }
  };
}  // namespace mtconnect
