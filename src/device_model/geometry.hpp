//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include "utilities.hpp"

#include <optional>
#include <utility>
#include <variant>

namespace mtconnect
{
  struct Origin
  {
    Origin() = default;
    Origin(const Origin &) = default;
    Origin(double x, double y, double z) : m_x(x), m_y(y), m_z(z) {}

    double m_x, m_y, m_z;
  };

  struct Axis
  {
    Axis() = default;
    Axis(const Axis &) = default;
    Axis(double x, double y, double z) : m_x(x), m_y(y), m_z(z) {}

    double m_x, m_y, m_z;
  };

  struct Translation
  {
    Translation() = default;
    Translation(const Translation &) = default;
    Translation(double x, double y, double z) : m_x(x), m_y(y), m_z(z) {}

    double m_x, m_y, m_z;
  };

  struct Rotation
  {
    Rotation() = default;
    Rotation(const Rotation &) = default;
    Rotation(double r, double p, double y) : m_roll(r), m_pitch(p), m_yaw(y) {}

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
    Scale() = default;
    Scale(const Scale &) = default;
    Scale(double x, double y, double z) : m_scaleX(x), m_scaleY(y), m_scaleZ(z) {}

    double m_scaleX, m_scaleY, m_scaleZ;
  };

  struct Geometry
  {
    Geometry() = default;
    Geometry(const Geometry &) = default;
    ~Geometry() = default;

    std::variant<std::monostate, Origin, Transformation> m_location;
    std::optional<Scale> m_scale;
    std::optional<Axis> m_axis;
  };
}  // namespace mtconnect
