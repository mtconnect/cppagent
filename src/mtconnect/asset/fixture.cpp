//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "fixture.hpp"

using namespace std;

namespace mtconnect::asset {
  using namespace entity;

  FactoryPtr Fixture::getFactory()
  {
    static FactoryPtr factory;
    if (!factory)
    {
      factory = make_shared<Factory>(*PhysicalAsset::getFactory());
      factory->addRequirements(Requirements {{"FixtureId", ValueType::STRING, false},
                                             {"FixtureNumber", ValueType::INTEGER, false},
                                             {"ClampingMethod", ValueType::STRING, false},
                                             {"MountingMethod", ValueType::STRING, false}});

      factory->setOrder({"ManufactureDate", "CalibrationDate", "InspectionDate",
                         "NextInspectionDate", "Measurements", "FixtureId", "FixtureNumber",
                         "ClampingMethod", "MountingMethod"});
    }

    return factory;
  }

  void Fixture::registerAsset()
  {
    static bool once {true};
    if (once)
    {
      Asset::registerAssetType("Fixture", getFactory());
      once = false;
    }
  }

}  // namespace mtconnect::asset
