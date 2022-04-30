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

#include <boost/asio.hpp>

#include <memory>

#include "utilities.hpp"

namespace Rice {
  class Module;
  class Class;
}  // namespace Rice

namespace mtconnect {
  class Agent;
  namespace ruby {
    class RubyVM;
    class Embedded
    {
    public:
      Embedded(Agent *agent, const ConfigOptions &options);
      ~Embedded();

    protected:
      Agent *m_agent;
      ConfigOptions m_options;
      boost::asio::io_context *m_context = nullptr;
      std::unique_ptr<RubyVM> m_rubyVM;
    };
  }  // namespace ruby
}  // namespace mtconnect
