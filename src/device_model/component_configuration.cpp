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

#include "component_configuration.hpp"
#include "sensor_configuration.hpp"
#include "relationships.hpp"
#include "solid_model.hpp"
#include "motion.hpp"
#include "coordinate_systems.hpp"
#include "specifications.hpp"
#include "entity.hpp"

#include <sstream>

using namespace std;

namespace mtconnect
{
  using namespace entity;

  FactoryPtr ComponentConfiguration::getRoot()
  {
    auto configuration = make_shared<Factory>();

    configuration->addRequirements({
        Requirement("SensorConfiguration", ENTITY, SensorConfiguration::getFactory(), 0, Requirement::Infinite),
        Requirement("Relationships", ENTITY_LIST, Relationships::getFactory(), false),
        Requirement("SolidModel", ENTITY, SolidModel::getFactory(), 0, Requirement::Infinite),
        Requirement("Motion", ENTITY, Motion::getFactory(), 0, Requirement::Infinite),
        Requirement("CoordinateSystems", ENTITY_LIST, CoordinateSystems::getFactory(), false),
        Requirement("Specifications", ENTITY_LIST, Specifications::getFactory(), false)
        });
    
    configuration->setMinListSize(1);
      
    return configuration;
  }

}  // namespace mtconnect