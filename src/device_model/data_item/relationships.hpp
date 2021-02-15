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
      class Relationship : public entity::Entity
      {
      public:
        static entity::FactoryPtr getDataItemFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;
          static auto source = make_shared<Factory>(
                                                    Requirements{
            {"type", ControlledVocab{"ATTACHMENT", "COORDINATE_SYSTEM", "LIMIT", "OBSERVATION"}, true},
            {"name", true}, {"idRef", true}
          });
          return source;
        }

        static entity::FactoryPtr getSpecificationactory()
        {
          using namespace mtconnect::entity;
          using namespace std;
          static auto source = make_shared<Factory>(
                                                    Requirements{
            {"type", ControlledVocab{"LIMIT"}, true},
            {"name", false}, {"idRef", true}
          });
          return source;
        }
      };
      
      class Relationships : public entity::Entity
      {
      public:
        static entity::FactoryPtr getFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;
          static FactoryPtr di = Relationship::getDataItemFactory();
          static FactoryPtr spec = Relationship::getSpecificationactory();
          static auto source = make_shared<Factory>(Requirements{
            {"SpecificationRelationship", ENTITY, spec, 0, Requirement::Infinite},
            {"DataItemRelationship", ENTITY, di, 0, Requirement::Infinite}
          });
          return source;
        }
      };            
    }
  }
}

