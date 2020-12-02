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
#include <optional>
#include <variant>

namespace mtconnect
{

  struct Origin
  {
    double m_x, m_y, m_z;    
  };

  struct Translation
  {
    double m_x, m_y, m_z;    
  };

  struct Rotation
  {
    double m_roll, m_pitch, m_yaw;    
  };      
  
  struct Transformation
  {
    Transformation() = default;
    Transformation(const Transformation &) = default;
    ~Transformation() = default;
    
    std::optional<Translation> m_translation;
    std::optional<Rotation> m_rotation;    
  };

  struct Scale 
  {
    double m_scaleX, m_scaleY, m_scaleZ;
  };

  struct Geometry
  {
    Geometry() = default;
    Geometry(const Geometry &) = default;
    ~Geometry() = default;
    
    std::variant<Origin, Transformation> m_location;
    std::optional<Scale> m_scale;
  };
}  // namespace mtconnect
