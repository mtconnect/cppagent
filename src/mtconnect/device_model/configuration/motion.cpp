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

#include "motion.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model {
    namespace configuration {
      FactoryPtr Motion::getFactory()
      {
        static auto transformation =
            make_shared<Factory>(Requirements {Requirement("Translation", VECTOR, 3, false),
                                               Requirement("Rotation", VECTOR, 3, false)});

        static auto motion = make_shared<Factory>(Requirements {
            Requirement("id", true), Requirement("parentIdRef", false),
            Requirement("coordinateSystemIdRef", true),
            Requirement("type", ControlledVocab {"REVOLUTE", "CONTINUOUS", "PRISMATIC", "FIXED"},
                        true),
            Requirement("actuation", ControlledVocab {"DIRECT", "VIRTUAL", "NONE"}, true),
            Requirement("Description", false), Requirement("Axis", VECTOR, 3, true),
            Requirement("Origin", VECTOR, 3, false),
            Requirement("Transformation", ENTITY, transformation, false)});

        return motion;
      }
    }  // namespace configuration
  }    // namespace device_model
}  // namespace mtconnect
