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

#include "sensor_configuration.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model {
    namespace configuration {
      FactoryPtr SensorConfiguration::getFactory()
      {
        static FactoryPtr sensorConfiguration;
        if (!sensorConfiguration)
        {
          auto channel = make_shared<Factory>(Requirements {
              Requirement("number", true),
              Requirement("name", false),
              Requirement("Description", false),
              Requirement("CalibrationDate", false),
              Requirement("NextCalibrationDate", false),
              Requirement("CalibrationInitials", false),
          });

          auto channels = make_shared<Factory>(
              Requirements {Requirement("Channel", ENTITY, channel, 1, Requirement::Infinite)});

          sensorConfiguration = make_shared<Factory>(Requirements {
              Requirement("FirmwareVersion", true), Requirement("CalibrationDate", false),
              Requirement("NextCalibrationDate", false), Requirement("CalibrationInitials", false),
              Requirement("Channels", ENTITY_LIST, channels, false)});
        }

        return sensorConfiguration;
      }
    }  // namespace configuration
  }    // namespace device_model
}  // namespace mtconnect
