//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  class Agent;
  namespace configuration {
    class AgentConfiguration;
  }
  /// @brief Embedded MRuby namespace
  namespace ruby {
    class RubyVM;
    /// @brief Static wrapper classes that add types to the Ruby instance
    class AGENT_LIB_API Embedded
    {
    public:
      /// @brief Create an embedded mruby instance
      Embedded(configuration::AgentConfiguration *config, const ConfigOptions &options);
      ~Embedded();

    protected:
      Agent *m_agent;
      ConfigOptions m_options;
      boost::asio::io_context *m_context = nullptr;
      std::unique_ptr<RubyVM> m_rubyVM;
    };
  }  // namespace ruby
}  // namespace mtconnect
