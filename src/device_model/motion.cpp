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
#include "motion.hpp"

#include <sstream>

using namespace std;

namespace mtconnect
{
  using namespace entity;

  FactoryPtr Motion::getFactory()
  {
    auto translation = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto rotation = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto transformation =
        make_shared<Factory>(Requirements{Requirement("Translation", ENTITY, translation, false),
                                          Requirement("Rotation", ENTITY, rotation, false)});

    transformation->registerMatchers();

    auto description = make_shared<Factory>(Requirements{
        Requirement("VALUE", true),
    });

    auto axis = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto origin = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto motion = make_shared<Factory>(Requirements{
        Requirement("id", true),
        Requirement("parentIdRef", false),
        Requirement("coordinateSystemIdRef", true),
        Requirement(
            "type",
            ControlledVocab{"REVOLUTE", "CONTINUOUS", "PRISMATIC", "FIXED"},
            true),
        Requirement(
            "actuation",
            ControlledVocab{"DIRECT", "VIRTUAL", "NONE"},
            true),
        Requirement("Description", ENTITY, description, false),
        Requirement("Axis", ENTITY, axis, true),
        Requirement("Origin", ENTITY, origin, false),
        Requirement("Transformation", ENTITY, transformation, false)
        });

    auto root = Motion::getRoot();

    root->addRequirements(Requirements{Requirement("Motion", ENTITY, motion)});

    return root;
  }

  FactoryPtr Motion::getRoot()
  {
    static auto root = make_shared<Factory>();
    return root;
  }

}  // namespace mtconnect