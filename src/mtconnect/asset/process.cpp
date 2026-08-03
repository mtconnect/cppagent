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

#include "process.hpp"

#include "target.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace asset {
    FactoryPtr ProcessArchetype::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        auto activity =
            make_shared<Factory>(Requirements {{"sequence", ValueType::INTEGER, false},
                                               {"activityId", true},
                                               {"precedence", ValueType::INTEGER, false},
                                               {"optional", ValueType::BOOL, false},
                                               {"Description", false}});

        auto activityGroup = make_shared<Factory>(Requirements {
            {"activityGroupId", true},
            {"name", false},
            {"Activity", ValueType::ENTITY, activity, 1, entity::Requirement::Infinite},
        });

        auto activityGroups = make_shared<Factory>(Requirements {
            {"ActivityGroup", ValueType::ENTITY, activityGroup, 1, entity::Requirement::Infinite}});

        auto processStep = make_shared<Factory>(
            Requirements {{{"stepId", true},
                           {"optional", ValueType::BOOL, false},
                           {"sequence", ValueType::INTEGER, false},
                           {"Description", false},
                           {"StartTime", ValueType::TIMESTAMP, false},
                           {"Duration", ValueType::DOUBLE, false},
                           {"Targets", ValueType::ENTITY_LIST, Target::getTargetsFactory(), false},
                           {"ActivityGroups", ValueType::ENTITY_LIST, activityGroups, false}}});
        processStep->setOrder(
            {"Description", "StartTime", "Duration", "Targets", "ActivityGroups"});

        auto routing = make_shared<Factory>(Requirements {
            {"precedence", ValueType::INTEGER, true},
            {"routingId", true},
            {"ProcessStep", ValueType::ENTITY, processStep, 1, entity::Requirement::Infinite},
        });

        auto routings = make_shared<Factory>(Requirements {
            {"Routing", ValueType::ENTITY, routing, 1, entity::Requirement::Infinite}});

        factory = make_shared<Factory>(*Asset::getFactory());
        factory->addRequirements({
            {"revision", true},
            {"Targets", ValueType::ENTITY_LIST, Target::getTargetsFactory(), false},
            {"Routings", ValueType::ENTITY_LIST, routings, true},
        });
        factory->setOrder({"Configuration", "Routings", "Targets"});
      }
      return factory;
    }

    void ProcessArchetype::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("ProcessArchetype", getFactory());
        once = false;
      }
    }

    FactoryPtr Process::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = ProcessArchetype::getFactory()->deepCopy();
        auto routings = factory->getRequirement("Routings");
        auto routing = routings->getFactory()->getRequirement("Routing");
        routing->setMultiplicity(1, 1);
      }
      return factory;
    }

    void Process::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("Process", getFactory());
        once = false;
      }
    }
  }  // namespace asset
}  // namespace mtconnect
