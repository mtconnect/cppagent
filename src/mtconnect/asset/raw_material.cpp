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

#include "mtconnect/asset/raw_material.hpp"

using namespace std;

namespace mtconnect::asset {
  using namespace entity;
  FactoryPtr RawMaterial::getFactory()
  {
    static FactoryPtr factory;
    if (!factory)
    {
      static auto material =
          make_shared<Factory>(Requirements {{"id", false},
                                             {"name", false},
                                             {"type", true},
                                             {"Lot", false},
                                             {"Manufacturer", false},
                                             {"ManufacturingDate", ValueType::TIMESTAMP, false},
                                             {"ManufacturingCode", false},
                                             {"MaterialCode", false}});

      material->setOrder(
          {"Lot", "Manufacturer", "ManufacturingDate", "ManufacturingCode", "MaterialCode"});

      factory = make_shared<Factory>(*Asset::getFactory());
      factory->addRequirements(Requirements {
          {"name", false},
          {"containerType", false},
          {"processKind", false},
          {"serialNumber", false},
          {"HasMaterial", ValueType::BOOL, false},
          {"Form",
           {"BAR", "BLOCK", "CASTING", "FILAMENT", "GAS", "GEL", "LIQUID", "POWDER", "SHEET"},
           true},
          {"ManufacturingDate", ValueType::TIMESTAMP, false},
          {"FirstUseDate", ValueType::TIMESTAMP, false},
          {"LastUseDate", ValueType::TIMESTAMP, false},
          {"InitialVolume", ValueType::DOUBLE, false},
          {"InitialDimension", ValueType::DOUBLE, false},
          {"InitialQuantity", ValueType::INTEGER, false},
          {"CurrentVolume", ValueType::DOUBLE, false},
          {"CurrentDimension", ValueType::DOUBLE, false},
          {"CurrentQuantity", ValueType::INTEGER, false},
          {"Material", ValueType::ENTITY, material, false}});
      factory->setOrder({"HasMaterial", "Form", "ManufacturingDate", "FirstUseDate", "LastUseDate",
                         "InitialVolume", "InitialDimension", "InitialQuantity", "CurrentVolume",
                         "CurrentDimension", "CurrentQuantity", "Material"});
    }

    return factory;
  }

  void RawMaterial::registerAsset()
  {
    static bool once {true};
    if (once)
    {
      Asset::registerAssetType("RawMaterial", getFactory());
      once = false;
    }
  }

}  // namespace mtconnect::asset
