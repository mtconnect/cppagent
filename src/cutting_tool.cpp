//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "cutting_tool.hpp"
#include "printer.hpp"

#include <sstream>

using namespace std;

namespace mtconnect
{
  using namespace entity;
  FactoryPtr CuttingToolArchetype::getFactory()
  {
    static FactoryPtr tool;
    if (!tool)
    {
      static auto definition = make_shared<Factory>(Requirements({
        Requirement("format", false),
        Requirement("RAW", true)
      }));
          
      static auto reconditionCount = make_shared<Factory>(Requirements({
        Requirement("maximumCount", INTEGER, false),
        Requirement("VALUE", INTEGER, false)
      }));

      static auto toolLife = make_shared<Factory>(Requirements({
        Requirement("type", true),
        Requirement("countDirection", true),
        Requirement("warning", DOUBLE, false),
        Requirement("limit", DOUBLE, false),
        Requirement("initial", DOUBLE, false),
        Requirement("VALUE", DOUBLE, false)
      }));
      
      static auto constraint = make_shared<Factory>(Requirements({
        Requirement("maximum", DOUBLE, false),
        Requirement("minimum", DOUBLE, false),
        Requirement("nominal", DOUBLE, false),
        Requirement("VALUE", DOUBLE, false)
      }));

      static auto measurement = make_shared<Factory>(Requirements({
        Requirement("significantDigits", INTEGER, false),
        Requirement("units", false),
        Requirement("nativeUnits", false),
        Requirement("code", false),
        Requirement("maximum", DOUBLE, false),
        Requirement("minimum", DOUBLE, false),
        Requirement("nominal", DOUBLE, false),
        Requirement("VALUE", DOUBLE, false)
      }));

      static auto measurements = make_shared<Factory>(Requirements({
        Requirement("Measurement", ENTITY, measurement,
                    1, Requirement::Infinite)
      }));
      measurements->registerMatchers();
      measurements->registerFactory(regex(".+"), measurement);

      static auto ext = make_shared<Factory>();
      
      static auto item = make_shared<Factory>(Requirements{
        { "indices", true },
        { "itemId", false },
        { "grade", false },
        { "manufacturers", false },
        { "Description", false },
        { "Locus", false },
        { "ItemLife", ENTITY, toolLife, 0, 3 },
        { "ProgramToolGroup", false },
        { "Measurements", ENTITY_LIST, measurements, false }
      });
      item->registerFactory(regex(".+"), ext);
      measurements->registerMatchers();
      item->setOrder({"Description", "Locus", "ItemLife",
        "ProgramToolGroup", "Measurements"
      });

      static auto items = make_shared<Factory>(Requirements{
        { "count", INTEGER, true},
        { "CuttingItem", ENTITY, item, 1, Requirement::Infinite }
      });
      
      static auto lifeCycle = make_shared<Factory>(Requirements({
        Requirement("ReconditionCount", ENTITY, reconditionCount, false),
        Requirement("ToolLife", ENTITY, toolLife,
                    0, 3),
        Requirement("ProgramToolGroup", false),
        Requirement("ProgramToolNumber", false),
        Requirement("ProcessSpindleSpeed", ENTITY, constraint, false),
        Requirement("ProcessFeedRate", ENTITY, constraint, false),
        Requirement("ConnectionCodeMachineSide", false),
        Requirement("Measurements", ENTITY_LIST, measurements, false),
        Requirement("CuttingItems", ENTITY_LIST, items, false)
      }));
      lifeCycle->registerFactory(regex(".+"), ext);
      measurements->registerMatchers();
      lifeCycle->setOrder({ "ReconditionCount", "ToolLife",
        "ProgramToolGroup", "ProgramToolNumber", "ProcessSpindleSpeed",
        "ProcessFeedRate", "ConnectionCodeMachineSide", "Measurements",
        "CuttingItems"
      });
      
      tool = make_shared<Factory>(*Asset::getFactory());
      tool->addRequirements(Requirements{
        Requirement("toolId", true),
        Requirement("Description", false),
        Requirement("CuttingToolDefinition", ENTITY, definition, false),
        Requirement("CuttingToolLifeCycle", ENTITY, lifeCycle, false)
      });
    }
    return tool;
  }
  
  RegisterAsset<CuttingToolArchetype>* const  CuttingToolArchetype::m_registerAsset =
   new RegisterAsset<CuttingToolArchetype>("CuttingToolArchetype");

  FactoryPtr CuttingTool::getFactory()
  {
    static FactoryPtr tool;
    if (!tool)
    {
      static auto state = make_shared<Factory>(Requirements{
        { "VALUE", STRING, true }
      });
      
      static auto status = make_shared<Factory>(Requirements{
        { "Status", ENTITY, state, 1, Requirement::Infinite }
      });
      
      static auto location = make_shared<Factory>(Requirements{
        { "type", true },
        { "negativeOverlap", INTEGER, false },
        { "positiveOverlap", INTEGER, false },
        { "turret", false },
        { "toolMagazine", false },
        { "toolRack", false },
        { "toolBar", false },
        { "automaticToolChanger", false },
        { "VALUE", true }
      });
      
      tool = CuttingToolArchetype::getFactory()->deepCopy();
      auto lifeCycle = tool->factoryFor("CuttingToolLifeCycle");
      lifeCycle->addRequirements(Requirements{
        {"CutterStatus", ENTITY_LIST, status, true},
        {"Location", ENTITY, location, false}
      });
      lifeCycle->setOrder({ "CutterStatus", "ReconditionCount", "ToolLife",
        "ProgramToolGroup", "ProgramToolNumber", "Location", "ProcessSpindleSpeed",
        "ProcessFeedRate", "ConnectionCodeMachineSide", "Measurements",
        "CuttingItems"
      });
      
      auto measmts = lifeCycle->factoryFor("Measurements");
      auto meas = measmts->factoryFor("Measurement");
      auto mv = meas->getRequirement("VALUE");
      mv->makeRequired();
      
      auto items = lifeCycle->factoryFor("CuttingItems");
      auto item = items->factoryFor("CuttingItem");

      item->addRequirements(Requirements{
        {"CutterStatus", ENTITY_LIST, status, true}
      });
      
      auto life = lifeCycle->factoryFor("ToolLife");
      auto lv = life->getRequirement("VALUE");
      lv->makeRequired();
    }
    return tool;
  }
  
  RegisterAsset<CuttingTool>* const  CuttingTool::m_registerAsset =
   new RegisterAsset<CuttingTool>("CuttingTool");

}  // namespace mtconnect
