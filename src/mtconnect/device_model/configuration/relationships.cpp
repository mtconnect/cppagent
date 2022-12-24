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

#include "relationships.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model {
    namespace configuration {
      FactoryPtr Relationships::getFactory()
      {
        static FactoryPtr relationships;
        if (!relationships)
        {
          auto relationship = make_shared<Factory>(Requirements {
              Requirement("id", true), Requirement("name", false),
              Requirement("type", ControlledVocab {"PARENT", "CHILD", "PEER"}, true),
              Requirement("criticality", ControlledVocab {"CRITICAL", "NON_CRITICAL"}, false)});

          auto deviceRelationship = make_shared<Factory>(*relationship);

          deviceRelationship->addRequirements(Requirements {
              Requirement("deviceUuidRef", true),
              Requirement("role", ControlledVocab {"SYSTEM", "AUXILIARY"}, false),
              Requirement("href", false),
              Requirement("xlink:type", false),
          });

          auto componentRelationship = make_shared<Factory>(*relationship);

          componentRelationship->addRequirements(Requirements {Requirement("idRef", true)});

          relationships = make_shared<Factory>(
              Requirements {Requirement("ComponentRelationship", ENTITY, componentRelationship, 0,
                                        Requirement::Infinite),
                            Requirement("DeviceRelationship", ENTITY, deviceRelationship, 0,
                                        Requirement::Infinite)});

          relationships->registerMatchers();

          relationships->setMinListSize(1);
        }
        return relationships;
      }
    }  // namespace configuration
  }    // namespace device_model
}  // namespace mtconnect
