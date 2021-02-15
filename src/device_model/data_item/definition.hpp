//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#pragma once

#include "entity.hpp"

#include <regex>

namespace mtconnect
{
  namespace device_model
  {
    namespace data_item
    {
      class CellDefinition : public entity::Entity
      {
      public:
        static entity::FactoryPtr getFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;
          static auto def = make_shared<Factory>(Requirements{
            {"Description", true},
            {"key", false},
            {"keyType", false},
            {"type", false},
            {"subType", false},
            {"units", false}
          });
          
          static auto defs = make_shared<Factory>(Requirements{
            {"CellDefinition", ENTITY, def, 1, Requirement::Infinite}
          });

          return defs;
        }
      };

      class EntityDefinition : public entity::Entity
      {
      public:
        static entity::FactoryPtr getFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;
          static auto cells = CellDefinition::getFactory();
          static auto def = make_shared<Factory>(Requirements{
            {"Description", true},
            {"key", false},
            {"keyType", false},
            {"type", false},
            {"subType", false},
            {"units", false},
            {"CellDefinitions", ENTITY_LIST, cells, false}
          });
          def->setOrder({"Description", "CellDefinitions"});

          static auto defs = make_shared<Factory>(Requirements{
            {"EntryDefinition", ENTITY, def, 1, Requirement::Infinite}
          });

          return defs;
        }
      };

      class Definition : public entity::Entity
      {
      public:
        static entity::FactoryPtr getFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;
          static auto cells = CellDefinition::getFactory();
          static auto entries = EntityDefinition::getFactory();
          static auto def = make_shared<Factory>(
                                                    Requirements{
            {"Description", true},
            {"EntryDefinitions", ENTITY_LIST, entries},
            {"CellDefinitions", ENTITY_LIST, cells}
          });
          def->setOrder({"Description", "EntryDefinitions", "CellDefinitions"});
          return def;
        }
      };
      
    }
  }
}

