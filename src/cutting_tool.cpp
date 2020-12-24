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
      Requirement("Measurement", ENTITY_LIST, measurement,
                  0, Requirement::Infinite)
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
      { "locus", false },
      { "ItemLife", ENTITY, toolLife, 0, Requirement::Infinite },
      { "ProgramToolGroup", false },
      { "Measurements", ENTITY, measurements, false }
    });
    item->registerFactory(regex(".+"), ext);
    measurements->registerMatchers();
    item->setOrder({"Description", "Locus",
      "ItemLife", "ProgramToolGroup", "Measurements"
    });

    static auto items = make_shared<Factory>(Requirements{
      { "count", INTEGER, true},
      { "CuttingItem", ENTITY, item, 1, Requirement::Infinite }
    });
    
    static auto lifeCycle = make_shared<Factory>(Requirements({
      Requirement("ReconditionCount", ENTITY, reconditionCount, false),
      Requirement("CuttingToolLife", ENTITY, toolLife,
                  0, 3),
      Requirement("ProgramToolGroup", false),
      Requirement("ProgramToolNumber", false),
      Requirement("ProcessSpindleSpeed", ENTITY, constraint, false),
      Requirement("ProcessFeedRate", ENTITY, constraint, false),
      Requirement("ConnectionCodeMachineSide", false),
      Requirement("Measurements", ENTITY, measurements, false),
      Requirement("CuttingItems", ENTITY, toolLife, false)
    }));
    lifeCycle->registerFactory(regex(".+"), ext);
    measurements->registerMatchers();
    lifeCycle->setOrder({ "ReconditionCount", "CuttingToolLife",
      "ProgramToolGroup", "ProgramToolNumber", "ProcessSpindleSpeed",
      "ProcessFeedRate", "ConnectionCodeMachineSide", "Measurements",
      "CuttingItems"
    });
    
    static auto tool = make_shared<Factory>(*Asset::getFactory());
    tool->addRequirements(Requirements{
      Requirement("toolId", true),
      Requirement("Description", false),
      Requirement("CuttingToolDefinition", ENTITY, definition, false),
      Requirement("CuttingToolLifeCycle", ENTITY, lifeCycle, false)
    });

    return tool;
  }
  
  RegisterAsset<CuttingToolArchetype>* const  CuttingToolArchetype::m_registerAsset =
   new RegisterAsset<CuttingToolArchetype>("CuttingToolArchetype");

  FactoryPtr CuttingTool::getFactory()
  {
    FactoryPtr tool;
    
    return tool;
  }
  
  RegisterAsset<CuttingTool>* const  CuttingTool::m_registerAsset =
   new RegisterAsset<CuttingTool>("CuttingTool");

}  // namespace mtconnect
