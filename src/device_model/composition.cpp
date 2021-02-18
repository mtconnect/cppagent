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

#include "composition.hpp"
#include "description.hpp"
#include "component_configuration.hpp"

#include "entity.hpp"

#include <sstream>

using namespace std;

namespace mtconnect
{
  using namespace entity;

  FactoryPtr Composition::getFactory()
  {
    auto transformation = make_shared<Factory>(Requirements{
        Requirement("Translation", VECTOR, 3, false), Requirement("Rotation", VECTOR, 3, false)});

    auto composition = make_shared<Factory>(Requirements{
        Requirement("id", true),
        Requirement("uuid", false),
        Requirement("name", false),
        Requirement("type", true),
        Requirement("Description", ENTITY, Description::getFactory(), false),
        Requirement("Configuration", ENTITY_LIST, ComponentConfiguration::getRoot(), false)});

    auto compositions = make_shared<Factory>(Requirements{
        Requirement("Composition", ENTITY, composition, 1, Requirement::Infinite)});

    return compositions;
  }

}  // namespace mtconnect