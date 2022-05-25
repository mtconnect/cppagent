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

#include "coordinate_systems.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model {
    namespace configuration {
      FactoryPtr CoordinateSystems::getFactory()
      {
        static FactoryPtr coordinateSystems;
        if (!coordinateSystems)
        {
          auto transformation =
              make_shared<Factory>(Requirements {Requirement("Translation", VECTOR, 3, false),
                                                 Requirement("Rotation", VECTOR, 3, false)});

          auto coordinateSystem = make_shared<Factory>(Requirements {
              Requirement("id", true), Requirement("name", false), Requirement("nativeName", false),
              Requirement("parentIdRef", false),
              Requirement(
                  "type",
                  ControlledVocab {"WORLD", "BASE", "OBJECT", "TASK", "MECHANICAL_INTERFACE",
                                   "TOOL", "MOBILE_PLATFORM", "MACHINE", "CAMERA"},
                  true),
              Requirement("Origin", VECTOR, 3, false),
              Requirement("Transformation", ENTITY, transformation, false)});

          coordinateSystems = make_shared<Factory>(Requirements {
              Requirement("CoordinateSystem", ENTITY, coordinateSystem, 1, Requirement::Infinite)});
        }

        return coordinateSystems;
      }
    }  // namespace configuration
  }    // namespace device_model
}  // namespace mtconnect
