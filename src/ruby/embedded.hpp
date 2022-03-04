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

#include "utilities.hpp"

#include <boost/asio.hpp>
#include <memory>

namespace Rice {
  class Module;
}

namespace mtconnect {
  class Agent;
  namespace ruby {
    
    
    class Embedded
    {
    public:
      Embedded(Agent *agent, const ConfigOptions &options);
      ~Embedded();
      
      void start(boost::asio::io_context &context, int threads);
      
    protected:
      Agent *m_agent;
      ConfigOptions m_options;
      std::unique_ptr<Rice::Module> m_module;
    };
  }
}
