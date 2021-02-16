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

#include "sensor_configuration.hpp"
#include "entity.hpp"

#include <sstream>

using namespace std;

namespace mtconnect
{
  using namespace entity;

  FactoryPtr SensorConfiguration::getFactory() 
  {
    auto firmwareVersion = make_shared<Factory>(Requirements{
        Requirement("VALUE", true),
    });

    auto calibrationDate = make_shared<Factory>(Requirements{
        Requirement("VALUE", true),
    });

    auto nextCalibrationDate = make_shared<Factory>(Requirements{
        Requirement("VALUE", true),
    });

    auto calibrationInitials = make_shared<Factory>(Requirements{
        Requirement("VALUE", true),
    });

    auto description = make_shared<Factory>(Requirements{
        Requirement("VALUE", true),
    });

    auto channel = make_shared<Factory>(Requirements{
        Requirement("number", true),
        Requirement("name", false),
        Requirement("Description", ENTITY, description, false),
        Requirement("CalibrationDate", ENTITY, calibrationDate, false),
        Requirement("NextCalibrationDate", ENTITY, nextCalibrationDate, false),
        Requirement("CalibrationInitials", ENTITY, calibrationInitials, false),
    });

    auto channels = make_shared<Factory>(Requirements{
        Requirement("Channel", ENTITY, channel, 1, Requirement::Infinite)
    });
    
    auto sensorConfiguration = make_shared<Factory>(Requirements{
        Requirement("FirmwareVersion", ENTITY, firmwareVersion, true),
        Requirement("CalibrationDate", ENTITY, calibrationDate, false),
        Requirement("NextCalibrationDate", ENTITY, nextCalibrationDate, false),
        Requirement("CalibrationInitials", ENTITY, calibrationInitials, false),
        Requirement("Channels", ENTITY_LIST, channels, false)
     });

    auto root = SensorConfiguration::getRoot();
    
    root->addRequirements(Requirements{
        Requirement("SensorConfiguration", ENTITY, sensorConfiguration, false)
    });
    
    return root;
  }

  FactoryPtr SensorConfiguration::getRoot() 
  { 
    static auto root = make_shared<Factory>();
    return root;
  }

}  // namespace mtconnect