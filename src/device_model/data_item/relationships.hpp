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

#pragma once

#include <regex>

#include "entity/entity.hpp"

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      class DataItem;
      class Relationship : public entity::Entity
      {
      public:
        using entity::Entity::Entity;
        ~Relationship() override = default;

        const entity::Value &getIdentity() const override { return getProperty("idRef"); }

        static entity::FactoryPtr getDataItemFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;

          static FactoryPtr factory;
          if (!factory)
          {
            factory = make_shared<Factory>(Requirements {
                {"type",
                 ControlledVocab {"ATTACHMENT", "COORDINATE_SYSTEM", "LIMIT", "OBSERVATION"}, true},
                {"name", false},
                {"idRef", true}});
            factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
              return std::make_shared<Relationship>(name, props);
            });
          }
          return factory;
        }

        static entity::FactoryPtr getSpecificationFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;

          static FactoryPtr factory;
          if (!factory)
          {
            factory = make_shared<Factory>(Requirements {
                {"type", ControlledVocab {"LIMIT"}, true}, {"name", false}, {"idRef", true}});
            factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
              return std::make_shared<Relationship>(name, props);
            });
          }
          return factory;
        }

        std::weak_ptr<DataItem> m_target;
      };

      class Relationships : public entity::Entity
      {
      public:
        using entity::Entity::Entity;
        ~Relationships() override = default;

        static entity::FactoryPtr getFactory()
        {
          using namespace mtconnect::entity;
          using namespace std;
          static FactoryPtr relationships;
          if (!relationships)
          {
            auto di = Relationship::getDataItemFactory();
            auto spec = Relationship::getSpecificationFactory();
            relationships = make_shared<Factory>(
                Requirements {{"SpecificationRelationship", ENTITY, spec, 0, Requirement::Infinite},
                              {"DataItemRelationship", ENTITY, di, 0, Requirement::Infinite}});
            relationships->setMinListSize(1);
            relationships->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
              return std::make_shared<Relationships>(name, props);
            });
          }
          return relationships;
        }
      };
    }  // namespace data_item
  }    // namespace device_model
}  // namespace mtconnect
