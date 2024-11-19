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

/// @file logging.hpp
/// @brief common logging macros based on boost trivial log

#pragma once

#include <boost/log/attributes.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>

#include "mtconnect/config.hpp"

typedef boost::log::sources::severity_channel_logger_mt
<
    boost::log::trivial::severity_level,     // the type of the severity level
    std::string         // the type of the channel name
> channel_logger_mt;

BOOST_LOG_INLINE_GLOBAL_LOGGER_INIT(agent_logger, channel_logger_mt)
{
  // Specify the channel name on construction, similarly as with the channel_logger
  return channel_logger_mt(boost::log::keywords::channel = "agent");
}

#define LOG(lvl) BOOST_LOG_SEV(agent_logger::get(), ::boost::log::trivial::lvl)

/// @brief synonym for `BOOST_LOG_NAMED_SCOPE`
#define NAMED_SCOPE BOOST_LOG_NAMED_SCOPE

#define LOG_LEVEL(lvl) ::boost::log::trivial::lvl

