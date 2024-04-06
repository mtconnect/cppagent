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

#include "mtconnect/asset/cutting_tool.hpp"

#include <sstream>

#include "mtconnect/printer//printer.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace asset {
    FactoryPtr CuttingToolArchetype::getFactory()
    {
      static FactoryPtr tool;
      if (!tool)
      {
        static auto definition = make_shared<Factory>(
            Requirements({Requirement("format", {"EXPRESS", "XML", "TEXT", "UNDEFINED"}),
                          Requirement("RAW", true)}));

        static auto reconditionCount = make_shared<Factory>(
            Requirements({Requirement("maximumCount", ValueType::INTEGER, false),
                          Requirement("VALUE", ValueType::INTEGER, false)}));

        static auto toolLife = make_shared<Factory>(
            Requirements({Requirement("type", {"MINUTES", "PART_COUNT", "WEAR"}),
                          Requirement("countDirection", ControlledVocab {"UP", "DOWN"}),
                          Requirement("warning", ValueType::DOUBLE, false),
                          Requirement("limit", ValueType::DOUBLE, false),
                          Requirement("initial", ValueType::DOUBLE, false),
                          Requirement("VALUE", ValueType::DOUBLE, false)}));

        static auto constraint =
            make_shared<Factory>(Requirements({Requirement("maximum", ValueType::DOUBLE, false),
                                               Requirement("minimum", ValueType::DOUBLE, false),
                                               Requirement("nominal", ValueType::DOUBLE, false),
                                               Requirement("VALUE", ValueType::DOUBLE, false)}));

        static auto measurement = make_shared<Factory>(Requirements(
            {Requirement("significantDigits", ValueType::INTEGER, false),
             Requirement("units", false), Requirement("nativeUnits", false),
             Requirement("code", false), Requirement("maximum", ValueType::DOUBLE, false),
             Requirement("minimum", ValueType::DOUBLE, false),
             Requirement("nominal", ValueType::DOUBLE, false),
             Requirement("VALUE", ValueType::DOUBLE, false)}));

        static auto measurements = make_shared<Factory>(Requirements({Requirement(
            "Measurement", ValueType::ENTITY, measurement, 1, Requirement::Infinite)}));
        measurements->registerMatchers();
        measurements->registerFactory(regex(".+"), measurement);

        static auto ext = make_shared<Factory>();
        ext->registerFactory(regex(".+"), ext);
        ext->setAny(true);

        static auto item = make_shared<Factory>(
            Requirements {{"indices", true},
                          {"itemId", false},
                          {"grade", false},
                          {"manufacturers", false},
                          {"Description", false},
                          {"Locus", false},
                          {"ItemLife", ValueType::ENTITY, toolLife, 0, 3},
                          {"ProgramToolGroup", false},
                          {"Measurements", ValueType::ENTITY_LIST, measurements, false}});
        item->registerFactory(regex(".+"), ext);
        item->setAny(true);

        measurements->registerMatchers();
        item->setOrder({"Description", "CutterStatus", "Locus", "ItemLife", "ProgramToolGroup",
                        "Measurements"});

        static auto items = make_shared<Factory>(
            Requirements {{"count", ValueType::INTEGER, true},
                          {"CuttingItem", ValueType::ENTITY, item, 1, Requirement::Infinite}});

        static auto lifeCycle = make_shared<Factory>(Requirements(
            {Requirement("ReconditionCount", ValueType::ENTITY, reconditionCount, false),
             Requirement("ToolLife", ValueType::ENTITY, toolLife, 0, 3),
             Requirement("ProgramToolGroup", false), Requirement("ProgramToolNumber", false),
             Requirement("ProcessSpindleSpeed", ValueType::ENTITY, constraint, false),
             Requirement("ProcessFeedRate", ValueType::ENTITY, constraint, false),
             Requirement("ConnectionCodeMachineSide", false),
             Requirement("Measurements", ValueType::ENTITY_LIST, measurements, false),
             Requirement("CuttingItems", ValueType::ENTITY_LIST, items, false)}));
        lifeCycle->registerFactory(regex(".+"), ext);
        lifeCycle->setAny(true);

        measurements->registerMatchers();
        lifeCycle->setOrder({"ReconditionCount", "ToolLife", "ProgramToolGroup",
                             "ProgramToolNumber", "ProcessSpindleSpeed", "ProcessFeedRate",
                             "ConnectionCodeMachineSide", "Measurements", "CuttingItems"});

        tool = make_shared<Factory>(*Asset::getFactory());
        tool->addRequirements(
            Requirements {{"toolId", true},
                          {"serialNumber", false},
                          {"manufacturers", false},
                          {"Description", false},
                          {"CuttingToolDefinition", ValueType::ENTITY, definition, false},
                          {"CuttingToolLifeCycle", ValueType::ENTITY, lifeCycle, false}});
      }
      return tool;
    }

    void CuttingToolArchetype::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("CuttingToolArchetype", getFactory());
        once = false;
      }
    }

    FactoryPtr CuttingTool::getFactory()
    {
      static FactoryPtr tool;
      if (!tool)
      {
        static auto state = make_shared<Factory>(
            Requirements {{"VALUE",
                           {"NEW", "AVAILABLE", "UNAVAILABLE", "ALLOCATED", "ALLOCATED", "MEASURED",
                            "NOT_REGISTERED", "RECONDITIONED", "USED", "EXPIRED", "TAGGED_OUT",
                            "BROKEN", "UNKNOWN"}}});

        static auto status = make_shared<Factory>(
            Requirements {{"Status", ValueType::ENTITY, state, 1, Requirement::Infinite}});

        static auto location = make_shared<Factory>(
            Requirements {{"type",
                           {"POT", "STATION", "CRIB", "SPINDLE", "TRANSFER_POT", "RETURN_POT",
                            "STAGING_POT", "REMOVAL_POT", "EXPIRED_POT", "END_EFFECTOR"}},
                          {"negativeOverlap", ValueType::INTEGER, false},
                          {"positiveOverlap", ValueType::INTEGER, false},
                          {"turret", false},
                          {"toolMagazine", false},
                          {"toolRack", false},
                          {"toolBar", false},
                          {"automaticToolChanger", false},
                          {"VALUE", true}});

        tool = CuttingToolArchetype::getFactory()->deepCopy();
        tool->getRequirement("serialNumber")->makeRequired();
        tool->getRequirement("toolId")->makeRequired();

        auto lifeCycle = tool->factoryFor("CuttingToolLifeCycle");
        lifeCycle->addRequirements(
            Requirements {{"CutterStatus", ValueType::ENTITY_LIST, status, true},
                          {"Location", ValueType::ENTITY, location, false}});
        lifeCycle->setOrder({"CutterStatus", "ReconditionCount", "ToolLife", "ProgramToolGroup",
                             "ProgramToolNumber", "Location", "ProcessSpindleSpeed",
                             "ProcessFeedRate", "ConnectionCodeMachineSide", "Measurements",
                             "CuttingItems"});

        auto measmts = lifeCycle->factoryFor("Measurements");
        auto meas = measmts->factoryFor("Measurement");
        meas->getRequirement("VALUE")->makeRequired();

        auto items = lifeCycle->factoryFor("CuttingItems");
        auto item = items->factoryFor("CuttingItem");

        item->addRequirements(
            Requirements {{"CutterStatus", ValueType::ENTITY_LIST, status, false}});

        auto life = lifeCycle->factoryFor("ToolLife");
        life->getRequirement("VALUE")->makeRequired();
      }
      return tool;
    }

    void CuttingTool::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("CuttingTool", getFactory());
        once = false;
      }
    }
  }  // namespace asset
}  // namespace mtconnect
