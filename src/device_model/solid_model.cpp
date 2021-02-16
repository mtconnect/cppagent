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
#include "solid_model.hpp"

#include <sstream>

using namespace std;

namespace mtconnect
{
  using namespace entity;

  FactoryPtr SolidModel::getFactory()
  {
    auto translation = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto rotation = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto transformation = make_shared<Factory>(Requirements{
        Requirement("Translation", ENTITY, translation, false),
        Requirement("Rotation", ENTITY, rotation, false)
    });
 
    transformation->registerMatchers();

    auto scale = make_shared<Factory>(Requirements{
        Requirement("VALUE", VECTOR, 3, true),
    });

    auto solidModel = make_shared<Factory>(Requirements{
        Requirement("id", true),
        Requirement("solidModelIdRef", false),
        Requirement("href", false),
        Requirement("itemRef", false),
        Requirement("mediaType", ControlledVocab {"STEP", "STL", "GDML", "OBJ", "COLLADA", "IGES", "3DS", "ACIS", "X_T"}, true),
        Requirement("coordinateSystemIdRef", false),
        Requirement("Transformation", ENTITY, transformation, false),
        Requirement("Scale", ENTITY, scale, false)
    });

    solidModel->registerMatchers();

    auto root = SolidModel::getRoot();

    root->addRequirements(
        Requirements{Requirement("SolidModel", ENTITY, solidModel)});

    return root;
  }

  FactoryPtr SolidModel::getRoot()
  {
    static auto root = make_shared<Factory>();
    return root;
  }

}  // namespace mtconnect