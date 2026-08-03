//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "part.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace asset {
    FactoryPtr PartArchetype::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        auto ext = make_shared<Factory>();
        ext->registerFactory(regex(".+"), ext);
        ext->setAny(true);
        ext->setList(true);

        auto customer = make_shared<Factory>(Requirements {
            {"customerId", true},
            {"name", false},
            {"Address", false},
            {"Description", false},
        });

        auto customers = make_shared<Factory>(Requirements {
            {"Customer", ValueType::ENTITY, customer, 1, entity::Requirement::Infinite}});

        factory = make_shared<Factory>(*Asset::getFactory());
        factory->addRequirements({
            {"revision", true},
            {"family", false},
            {"drawing", false},
            {"Customers", ValueType::ENTITY_LIST, customers, false},
        });
        factory->registerFactory(regex(".+"), ext);
        factory->setAny(true);
      }
      return factory;
    }

    void PartArchetype::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("PartArchetype", getFactory());
        once = false;
      }
    }

    FactoryPtr Part::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        auto ext = make_shared<Factory>();
        ext->registerFactory(regex(".+"), ext);
        ext->setAny(true);
        ext->setList(true);

        auto identifier = make_shared<Factory>(
            Requirements {{"type", ControlledVocab {"UNIQUE_IDENTIFIER", "GROUP_IDENTIFIER"}, true},
                          {"stepIdRef", false},
                          {"timestamp", ValueType::TIMESTAMP, true},
                          {"VALUE", true}});

        auto identifiers = make_shared<Factory>(Requirements {
            {"Identifier", ValueType::ENTITY, identifier, 1, entity::Requirement::Infinite}});

        factory = make_shared<Factory>(*Asset::getFactory());
        factory->addRequirements({{"revision", true},
                                  {"family", false},
                                  {"drawing", false},
                                  {"PartIdentifiers", ValueType::ENTITY_LIST, identifiers, false}});
        factory->registerFactory(regex(".+"), ext);
        factory->setAny(true);
      }
      return factory;
    }

    void Part::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("Part", getFactory());
        once = false;
      }
    }
  }  // namespace asset
}  // namespace mtconnect
