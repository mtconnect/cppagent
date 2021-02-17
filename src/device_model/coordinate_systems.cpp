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

#include "entity.hpp"
#include "coordinate_systems.hpp"

#include <sstream>

using namespace std;

namespace mtconnect
{
  using namespace entity;

  FactoryPtr CoordinateSystems::getFactory()
  {
    auto origin = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto translation = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto rotation = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto transformation = make_shared<Factory>(Requirements{
        Requirement("Translation", ENTITY, translation, false),
        Requirement("Rotation", ENTITY, rotation, false)});

    auto coordinateSystem = make_shared<Factory>(Requirements{
        Requirement("id", true),
        Requirement("name", false),
        Requirement("nativeName", false),
        Requirement("parentIdRef", false),
        Requirement(
            "type",
            ControlledVocab {"WORLD", "BASE", "OBJECT", "TASK", "MECHANICAL_INTERFACE", "TOOL", "MOBILE_PLATFORM", "MACHINE", "CAMERA"},
            true),
        Requirement("Origin", ENTITY, origin, false),
        Requirement("Transformation", ENTITY, transformation, false)});

    auto coordinateSystems = make_shared<Factory>(Requirements{
        Requirement("CoordinateSystem", ENTITY, coordinateSystem, 1, Requirement::Infinite)});

    auto root = CoordinateSystems::getRoot();

    root->addRequirements(
        Requirements{Requirement("CoordinateSystems", ENTITY_LIST, coordinateSystems, false)});

    return root;
  }

  FactoryPtr CoordinateSystems::getRoot()
  {
    static auto root = make_shared<Factory>();

    return root;
  }

}  // namespace mtconnect