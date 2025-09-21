//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "configuration.hpp"

#include "coordinate_systems.hpp"
#include "image_file.hpp"
#include "motion.hpp"
#include "mtconnect/entity/factory.hpp"
#include "relationships.hpp"
#include "sensor_configuration.hpp"
#include "solid_model.hpp"
#include "specifications.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model {
    namespace configuration {
      FactoryPtr Configuration::getFactory()
      {
        static auto configuration = make_shared<Factory>(Requirements {
            {"SensorConfiguration", ValueType::ENTITY, SensorConfiguration::getFactory(), false},
            {"Relationships", ValueType::ENTITY_LIST, Relationships::getFactory(), false},
            {"SolidModel", ValueType::ENTITY, SolidModel::getFactory(), false},
            {"ImageFiles", ValueType::ENTITY_LIST, ImageFile::getFactory(), false},
            {"Motion", ValueType::ENTITY, Motion::getFactory(), false},
            {"CoordinateSystems", ValueType::ENTITY_LIST, CoordinateSystems::getFactory(), false},
            {"Specifications", ValueType::ENTITY_LIST, Specifications::getFactory(), false}});

        return configuration;
      }

      FactoryPtr Configuration::getRoot()
      {
        static auto root = make_shared<Factory>(Requirements {
            {"Configuration", ValueType::ENTITY, Configuration::getFactory(), false}});

        return root;
      }
    }  // namespace configuration
  }    // namespace device_model
}  // namespace mtconnect
