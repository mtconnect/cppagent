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

/// @file logging.hpp
/// @brief common logging macros based on boost trivial log

#pragma once

#include <boost/log/attributes.hpp>
#include <boost/log/trivial.hpp>

#include "mtconnect/config.hpp"

/// @brief synonym for `BOOST_LOG_TRIVIAL`
#define LOG BOOST_LOG_TRIVIAL

/// @brief synonym for `BOOST_LOG_NAMED_SCOPE`
#define NAMED_SCOPE BOOST_LOG_NAMED_SCOPE

// Must be initialized in the plugin before callign log as follows:
//    mtconnect::gAgentLogger = config->getLogger();
//    After that, use PLUGIN_LOG(lvl) << ...;
namespace mtconnect {
  namespace configuration {
    AGENT_LIB_API
    extern boost::log::trivial::logger_type *gAgentLogger;
  }  // namespace configuration
}  // namespace mtconnect

#define LOG_LEVEL(lvl) ::boost::log::trivial::lvl

/// @brief Used when static using static agent_lib in a plugin shared object
#define PLUGIN_LOG(lvl)                                                 \
  BOOST_LOG_STREAM_WITH_PARAMS(*mtconnect::configuration::gAgentLogger, \
                               (::boost::log::keywords::severity = ::boost::log::trivial::lvl))
