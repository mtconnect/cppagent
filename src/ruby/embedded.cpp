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

#include "embedded.hpp"

#include <iostream>
#include <string>

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "device_model/device.hpp"
#include "entity.hpp"
#include "pipeline/guard.hpp"
#include "pipeline/transform.hpp"

#include <rice/rice.hpp>
#include <rice/stl.hpp>

using namespace std;

namespace mtconnect::ruby {
 
  Embedded::Embedded(Agent *agent, const ConfigOptions &options)
  {
    using namespace Rice;
    
    int argc = 0;
    char* argv = nullptr;
    char** pArgv = &argv;

    ruby_sysinit(&argc, &pArgv);
    ruby_init();
    ruby_init_loadpath();
    
    Module mtc = define_module("MTConnect");
    
    define_class_under<entity::Entity>(mtc, "Entity");
  }
}
