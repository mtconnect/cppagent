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
#include "specifications.hpp"

#include <sstream>

using namespace std;

namespace mtconnect
{
  using namespace entity;

  FactoryPtr Specifications::getFactory()
  {
    auto maximum = make_shared<Factory>(Requirements{Requirement("VALUE", DOUBLE, true)});
    auto minimum = make_shared<Factory>(Requirements{Requirement("VALUE", DOUBLE, true)});
    auto nominal = make_shared<Factory>(Requirements{Requirement("VALUE", DOUBLE, true)});
    auto upperLimit = make_shared<Factory>(Requirements{Requirement("VALUE", DOUBLE, true)});
    auto upperWarning = make_shared<Factory>(Requirements{Requirement("VALUE", DOUBLE, true)});
    auto lowerWarning = make_shared<Factory>(Requirements{Requirement("VALUE", DOUBLE, true)});
    auto lowerLimit = make_shared<Factory>(Requirements{Requirement("VALUE", DOUBLE, true)});
    
    auto abstractSpecification = make_shared<Factory>(Requirements{
        Requirement("id", true),
        Requirement("type", true),
        Requirement("originator", ControlledVocab{"MANUFACTURER", "USER"}, false),
        Requirement("subType", false),
        Requirement("name", false),
        Requirement("dataItemIdRef", false),
        Requirement("compositionIdRef", false),
        Requirement("coordinateSystemIdRef", false),
        Requirement("units", false),
    });

    auto controlLimits = make_shared<Factory>(Requirements{
        Requirement("UpperLimit", ENTITY, upperLimit, false),
        Requirement("UpperWarning", ENTITY, upperWarning, false),
        Requirement("Nominal", ENTITY, nominal, false),
        Requirement("LowerWarning", ENTITY, lowerWarning, false),
        Requirement("LowerLimit", ENTITY, lowerLimit, false)
    });

    auto alarmLimits = make_shared<Factory>(Requirements{
        Requirement("UpperLimit", ENTITY, upperLimit, false),
        Requirement("UpperWarning", ENTITY, upperWarning, false),
        Requirement("LowerWarning", ENTITY, lowerWarning, false),
        Requirement("LowerLimit", ENTITY, lowerLimit, false)
    });

    auto specificationLimits = make_shared<Factory>(Requirements{
        Requirement("UpperLimit", ENTITY, upperLimit, false),
        Requirement("Nominal", ENTITY, nominal, false),
        Requirement("LowerLimit", ENTITY, lowerLimit, false)
    });

        
    auto specification = make_shared<Factory>(*abstractSpecification);

    specification->addRequirements({
        Requirement("Maximum", ENTITY, maximum, false),
        Requirement("Minimum", ENTITY, minimum, false),
        Requirement("Nominal", ENTITY, nominal, false),
        Requirement("UpperLimit", ENTITY, upperLimit, false),
        Requirement("UpperWarning", ENTITY, upperWarning, false),
        Requirement("LowerWarning", ENTITY, lowerWarning, false),
        Requirement("LowerLimit", ENTITY, lowerLimit, false)});

    auto processSpecification = make_shared<Factory>(*abstractSpecification);
    
    processSpecification->addRequirements({
        Requirement("ControlLimits", ENTITY, controlLimits, false),
        Requirement("AlarmLimits", ENTITY, alarmLimits, false),
        Requirement("SpecificationLimits", ENTITY, specificationLimits, false)
        });

    auto specifications = make_shared<Factory>(Requirements{
        Requirement("ProcessSpecification", ENTITY, processSpecification, 0,
                    Requirement::Infinite),
        Requirement("Specification", ENTITY, specification, 0, Requirement::Infinite)});

    specifications->registerMatchers();

    specifications->setMinListSize(1);

    auto root = Specifications::getRoot();

    root->addRequirements(Requirements{Requirement("Specifications", ENTITY_LIST, specifications, false)});

    return root;
  }

  FactoryPtr Specifications::getRoot()
  {
    static auto root = make_shared<Factory>();

    return root;
  }

}  // namespace mtconnect