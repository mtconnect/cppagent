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

#include "asset/raw_material.hpp"

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
                                             {"ManufacturingDate", TIMESTAMP, false},
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
          {"HasMaterial", BOOL, false},
          {"Form",
           {"BAR", "BLOCK", "CASTING", "FILAMENT", "GAS", "GEL", "LIQUID", "POWDER", "SHEET"},
           true},
          {"ManufacturingDate", TIMESTAMP, false},
          {"FirstUseDate", TIMESTAMP, false},
          {"LastUseDate", TIMESTAMP, false},
          {"InitialVolume", DOUBLE, false},
          {"InitialDimension", DOUBLE, false},
          {"InitialQuantity", INTEGER, false},
          {"CurrentVolume", DOUBLE, false},
          {"CurrentDimension", DOUBLE, false},
          {"CurrentQuantity", INTEGER, false},
          {"Material", ENTITY, material, false}});
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
