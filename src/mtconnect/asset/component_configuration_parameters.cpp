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

#include "component_configuration_parameters.hpp"

namespace mtconnect::asset {
  using namespace entity;
  using namespace std;

  FactoryPtr ComponentConfigurationParameters::getFactory()
  {
    static auto parameter = make_shared<Factory>(Requirements {
        Requirement("identifier", true),
        Requirement("name", true),
        Requirement("minimum", ValueType::DOUBLE, false),
        Requirement("maximum", ValueType::DOUBLE, false),
        Requirement("nominal", ValueType::DOUBLE, false),
        Requirement("units", false),
        Requirement("VALUE", true),
    });

    static auto parameterSet = make_shared<Factory>(
        Requirements {Requirement("name", false),
                      Requirement("Parameter", ValueType::ENTITY, parameter, 1, Requirement::Infinite)});

    static auto factory = make_shared<Factory>(*Asset::getFactory());

    static bool first {true};
    if (first)
    {
      factory->addRequirements(Requirements {
          Requirement("ParameterSet", ValueType::ENTITY_LIST, parameterSet, 1, Requirement::Infinite)});

      auto root = Asset::getRoot();
      root->registerFactory("ComponentConfigurationParameters", factory);
      first = false;
    }

    return factory;
  }

  void ComponentConfigurationParameters::registerAsset()
  {
    static bool once {true};
    if (once)
    {
      Asset::registerAssetType("ComponentConfigurationParameters", getFactory());
      once = false;
    }
  }

}  // namespace mtconnect::asset
