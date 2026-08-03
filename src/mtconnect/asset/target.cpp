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

#include "target.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace asset {
    FactoryPtr Target::getFactory()
    {
      static FactoryPtr target = make_shared<Factory>();
      return target;
    }

    FactoryPtr Target::getDeviceTargetsFactory()
    {
      static FactoryPtr targets;
      if (!targets)
      {
        targets = make_shared<Factory>(entity::Requirements {
            entity::Requirement("TargetDevice", ValueType::ENTITY, TargetDevice::getFactory(), 0,
                                entity::Requirement::Infinite),
            entity::Requirement("TargetGroup", ValueType::ENTITY_LIST, TargetGroup::getFactory(), 0,
                                entity::Requirement::Infinite)});

        targets->registerMatchers();
        targets->setMinListSize(1);
      }

      return targets;
    }

    FactoryPtr Target::getTargetsFactory()
    {
      static FactoryPtr targets;
      if (!targets)
      {
        targets = make_shared<Factory>(entity::Requirements {
            entity::Requirement("TargetDevice", ValueType::ENTITY, TargetDevice::getFactory(), 0,
                                entity::Requirement::Infinite),
            entity::Requirement("TargetGroup", ValueType::ENTITY_LIST, TargetGroup::getFactory(), 0,
                                entity::Requirement::Infinite),
            entity::Requirement("TargetRef", ValueType::ENTITY, TargetRef::getFactory(), 0,
                                entity::Requirement::Infinite)});

        targets->registerMatchers();
        targets->setMinListSize(1);
      }

      return targets;
    }

    FactoryPtr Target::getAllTargetsFactory()
    {
      static FactoryPtr targets;
      if (!targets)
      {
        targets = make_shared<Factory>(entity::Requirements {
            entity::Requirement("TargetDevice", ValueType::ENTITY, TargetDevice::getFactory(), 0,
                                entity::Requirement::Infinite),
            entity::Requirement("TargetGroup", ValueType::ENTITY_LIST, TargetGroup::getFactory(), 0,
                                entity::Requirement::Infinite),
            entity::Requirement("TargetRef", ValueType::ENTITY, TargetRef::getFactory(), 0,
                                entity::Requirement::Infinite),
            entity::Requirement("TargetRequirementTable", ValueType::ENTITY,
                                TargetRequirement::getFactory(), 0,
                                entity::Requirement::Infinite)});

        targets->registerMatchers();
        targets->setMinListSize(1);
      }

      return targets;
    }

    FactoryPtr Target::getRequirementTargetsFactory()
    {
      static FactoryPtr targets;
      if (!targets)
      {
        targets = make_shared<Factory>(entity::Requirements {entity::Requirement(
            "TargetRequirementTable", ValueType::ENTITY, TargetRequirement::getFactory(), 0,
            entity::Requirement::Infinite)});

        targets->registerMatchers();
        targets->setMinListSize(1);
      }

      return targets;
    }

    FactoryPtr TargetDevice::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Target::getFactory());
        factory->addRequirements({{"deviceUuid", true}});
      }

      return factory;
    }

    FactoryPtr TargetRef::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Target::getFactory());
        factory->addRequirements({{"groupIdRef", true}});
      }

      return factory;
    }

    FactoryPtr TargetGroup::getFactory()
    {
      using namespace entity;
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Target::getFactory());
        factory->addRequirements({{"groupId", true},
                                  {"TargetDevice", ValueType::ENTITY, TargetDevice::getFactory(), 0,
                                   entity::Requirement::Infinite},
                                  {"TargetRef", ValueType::ENTITY, TargetRef::getFactory(), 0,
                                   entity::Requirement::Infinite}});
        factory->registerMatchers();
        factory->setMinListSize(1);
      }

      return factory;
    }

    FactoryPtr TargetRequirement::getFactory()
    {
      using namespace entity;
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Target::getFactory());
        factory->addRequirements({{"requirementId", true}, {"VALUE", ValueType::TABLE, true}});
      }

      return factory;
    }
  }  // namespace asset
}  // namespace mtconnect
