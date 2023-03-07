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

#include "specifications.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model {
    namespace configuration {
      FactoryPtr Specifications::getFactory()
      {
        static FactoryPtr specifications;
        if (!specifications)
        {
          auto abstractSpecification = make_shared<Factory>(Requirements {
              Requirement("id", false),
              Requirement("type", true),
              Requirement("originator", ControlledVocab {"MANUFACTURER", "USER"}, false),
              Requirement("subType", false),
              Requirement("name", false),
              Requirement("dataItemIdRef", false),
              Requirement("compositionIdRef", false),
              Requirement("coordinateSystemIdRef", false),
              Requirement("units", false),
          });

          auto controlLimits = make_shared<Factory>(Requirements {
              Requirement("UpperLimit", DOUBLE, false), Requirement("UpperWarning", DOUBLE, false),
              Requirement("Nominal", DOUBLE, false), Requirement("LowerWarning", DOUBLE, false),
              Requirement("LowerLimit", DOUBLE, false)});

          auto alarmLimits = make_shared<Factory>(Requirements {
              Requirement("UpperLimit", DOUBLE, false), Requirement("UpperWarning", DOUBLE, false),
              Requirement("LowerWarning", DOUBLE, false),
              Requirement("LowerLimit", DOUBLE, false)});

          auto specificationLimits = make_shared<Factory>(Requirements {
              Requirement("UpperLimit", DOUBLE, false), Requirement("Nominal", DOUBLE, false),
              Requirement("LowerLimit", DOUBLE, false)});

          auto specification = make_shared<Factory>(*abstractSpecification);

          specification->addRequirements(
              {Requirement("Maximum", DOUBLE, false), Requirement("Minimum", DOUBLE, false),
               Requirement("Nominal", DOUBLE, false), Requirement("UpperLimit", DOUBLE, false),
               Requirement("UpperWarning", DOUBLE, false), Requirement("Nominal", DOUBLE, false),
               Requirement("LowerWarning", DOUBLE, false),
               Requirement("LowerLimit", DOUBLE, false)});

          auto processSpecification = make_shared<Factory>(*abstractSpecification);

          processSpecification->addRequirements(
              {Requirement("ControlLimits", ENTITY, controlLimits, false),
               Requirement("AlarmLimits", ENTITY, alarmLimits, false),
               Requirement("SpecificationLimits", ENTITY, specificationLimits, false)});

          specifications = make_shared<Factory>(Requirements {
              Requirement("ProcessSpecification", ENTITY, processSpecification, 0,
                          Requirement::Infinite),
              Requirement("Specification", ENTITY, specification, 0, Requirement::Infinite)});

          specifications->registerMatchers();

          specifications->setMinListSize(1);
        }
        return specifications;
      }
    }  // namespace configuration
  }    // namespace device_model
}  // namespace mtconnect
