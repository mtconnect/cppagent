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

#include "task.hpp"

#include "target.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace asset {
    FactoryPtr TaskArchetype::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        auto collaborator = make_shared<Factory>(Requirements {
            {"collaboratorId", true},
            {"type", false},
            {"optional", ValueType::BOOL, false},
            {"Targets", ValueType::ENTITY_LIST, Target::getAllTargetsFactory(), true}});

        auto collaborators = make_shared<Factory>(Requirements {
            {"Collaborator", ValueType::ENTITY, collaborator, 1, entity::Requirement::Infinite}});
        auto coordinator = make_shared<Factory>(
            Requirements {{"Collaborator", ValueType::ENTITY, collaborator, true}});

        auto ext = make_shared<Factory>();
        ext->registerFactory(regex(".+"), ext);
        ext->setAny(true);
        ext->setList(true);

        auto subTaskRef = make_shared<Factory>(Requirements {{"order", ValueType::INTEGER, true},
                                                             {"parallel", ValueType::BOOL, false},
                                                             {"optional", ValueType::BOOL, false},
                                                             {"group", false},
                                                             {"VALUE", true}});

        auto subTaskRefs = make_shared<Factory>(Requirements {
            {"SubTaskRef", ValueType::ENTITY, subTaskRef, 1, entity::Requirement::Infinite}});

        factory = make_shared<Factory>(*Asset::getFactory());
        factory->addRequirements(
            {{"TaskType", true},
             {"Priority", ValueType::INTEGER, false},
             {"Coordinator", ValueType::ENTITY, coordinator, true},
             {"Collaborators", ValueType::ENTITY_LIST, collaborators, true},
             {"Targets", ValueType::ENTITY_LIST, Target::getAllTargetsFactory(), false},
             {"SubTaskRefs", ValueType::ENTITY_LIST, subTaskRefs, false}});
        factory->setOrder({{"Configuration", "TaskType", "Priority", "Targets", "Coordinator",
                            "Collaborators", "SubTaskREfs"}});
        factory->registerFactory(regex(".+"), ext);
        factory->setAny(true);
      }
      return factory;
    }

    void TaskArchetype::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("TaskArchetype", getFactory());
        once = false;
      }
    }

    FactoryPtr Task::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        auto collaborator = make_shared<Factory>(Requirements {{"collaboratorId", true},
                                                               {"type", false},
                                                               {"collaboratorDeviceUuid", false},
                                                               {"requirementId", false}});

        auto collaborators = make_shared<Factory>(Requirements {
            {"Collaborator", ValueType::ENTITY, collaborator, 1, entity::Requirement::Infinite}});
        auto coordinator = make_shared<Factory>(
            Requirements {{"Collaborator", ValueType::ENTITY, collaborator, true}});

        auto ext = make_shared<Factory>();
        ext->registerFactory(regex(".+"), ext);
        ext->setAny(true);
        ext->setList(true);

        factory = make_shared<Factory>(*Asset::getFactory());
        factory->addRequirements({{"TaskType", true},
                                  {"TaskState",
                                   ControlledVocab {"INACTIVE", "PREPARING", "COMMITTING",
                                                    "COMMITTED", "COMPLETE", "FAIL"},
                                   true},
                                  {"ParentTaskAssetId", false},
                                  {"TaskArchetypeAssetId", false},
                                  {"Coordinator", ValueType::ENTITY, coordinator, true},
                                  {"Collaborators", ValueType::ENTITY_LIST, collaborators, true}});
        auto task = make_shared<Factory>(
            Requirements {{"Task", ValueType::ENTITY, factory, 1, entity::Requirement::Infinite}});

        factory->addRequirements({{"SubTasks", ValueType::ENTITY_LIST, task, false}});
        factory->setOrder({"Configuration", "TaskType", "TaskState", "ParentTaskAssetId",
                           "TaskArchetypeAssetId", "Coordinator", "Collaborators", "SubTasks"});
        factory->registerFactory(regex(".+"), ext);
        factory->setAny(true);
      }
      return factory;
    }

    void Task::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("Task", getFactory());
        once = false;
      }
    }
  }  // namespace asset
}  // namespace mtconnect
