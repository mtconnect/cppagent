//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/asset/physical_asset.hpp"

using namespace std;

namespace mtconnect::asset {
  using namespace entity;

  FactoryPtr PhysicalAsset::getMeasurementsFactory()
  {
    static FactoryPtr measurements;

    if (!measurements)
    {
      static auto measurement = make_shared<Factory>(Requirements(
          {Requirement("significantDigits", ValueType::INTEGER, false), Requirement("units", false),
           Requirement("nativeUnits", false), Requirement("code", false),
           Requirement("maximum", ValueType::DOUBLE, false),
           Requirement("minimum", ValueType::DOUBLE, false),
           Requirement("nominal", ValueType::DOUBLE, false),
           Requirement("VALUE", ValueType::DOUBLE, false)}));

      measurements = make_shared<Factory>(Requirements(
          {Requirement("Measurement", ValueType::ENTITY, measurement, 1, Requirement::Infinite)}));
      measurements->registerMatchers();
      measurements->registerFactory(regex(".+"), measurement);
    }
    return measurements;
  }

  FactoryPtr PhysicalAsset::getFactory()
  {
    static FactoryPtr factory;
    if (!factory)
    {
      static auto measurements = getMeasurementsFactory()->deepCopy();

      factory = make_shared<Factory>(*Asset::getFactory());
      factory->addRequirements(
          Requirements {{"ManufactureDate", ValueType::TIMESTAMP, false},
                        {"CalibrationDate", ValueType::TIMESTAMP, false},
                        {"InspectionDate", ValueType::TIMESTAMP, false},
                        {"NextInspectionDate", ValueType::TIMESTAMP, false},
                        {"Measurements", ValueType::ENTITY_LIST, measurements, false}});

      auto meas = measurements->factoryFor("Measurement");
      meas->getRequirement("VALUE")->makeRequired();

      factory->setOrder({"ManufactureDate", "CalibrationDate", "InspectionDate",
                         "NextInspectionDate", "Measurements"});
    }

    return factory;
  }

  void PhysicalAsset::registerAsset()
  {
    static bool once {true};
    if (once)
    {
      Asset::registerAssetType("PhysicalAsset", getFactory());
      once = false;
    }
  }

}  // namespace mtconnect::asset
