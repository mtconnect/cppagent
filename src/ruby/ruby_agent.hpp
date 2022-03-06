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

#include "agent.hpp"
#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>


namespace mtconnect::ruby {
  using namespace mtconnect::device_model;
  using namespace std;
  using namespace Rice;

  struct RubyAgent {
    void create(Rice::Module &module)
    {
      m_agent = make_unique<Class>(define_class_under<Agent>(module, "Agent"));
      m_source = make_unique<Class>(define_class_under<mtconnect::Source>(module, "Source"));
      m_sink = make_unique<Class>(define_class_under<mtconnect::Sink>(module, "Sink"));      
    }
    
    void methods()
    {
      
      m_agent->define_method("sources", [](Agent *agent) {
          Rice::Array ary;
          for (auto &s : agent->getSources())
            ary.push(s.get());
          return ary;
        }).
        define_method("data_item_for_device", [](Agent *agent, const string device, const string name) {
          return agent->getDataItemForDevice(device, name);
        }, Arg("device"), Arg("name")).
        define_method("device", [](Agent *agent, const string device) {
          return agent->findDeviceByUUIDorName(device);
        }, Arg("name")).
        define_method("devices", [](Agent *agent) {
          Rice::Array ary;
          for (auto &d : agent->getDevices())
            ary.push(d.get());
          return ary;
        });

      m_source->define_method("name", [](mtconnect::Source *s) -> string {
          return s->getName();
        }).
	define_method("pipeline", [](mtconnect::Source *s) { return s->getPipeline(); });


    }
    
    std::unique_ptr<Rice::Class> m_agent;
    std::unique_ptr<Rice::Class> m_source;
    std::unique_ptr<Rice::Class> m_sink;
  };
}
