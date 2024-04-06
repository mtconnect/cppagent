#pragma once

//
// Copyright Copyright 2009-2024, AMT � The Association For Manufacturing Technology (�AMT�)
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

/// @file config.hpp
/// @brief common includes and cross platform requirements

// TODO: Remove when BOOST fixes its multiple defined symbol issue with phoenix placeholders
#define BOOST_PHOENIX_STL_TUPLE_H_
#define BOOST_BIND_NO_PLACEHOLDERS

#include <boost/config.hpp>

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif
#if _MSC_VER > 1500
#include <cstdint>
#else
#endif
#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFull
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <chrono>
#include <ctime>
#include <date/date.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <time.h>
#include <unordered_map>
#include <variant>

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WINDOWS
#define _WINDOWS 1
#endif
#define ISNAN(x) _isnan(x)
#if _MSC_VER < 1800
#define NAN numeric_limits<double>::quiet_NaN()
#endif
#if _MSC_VER >= 1900
#define gets gets_s
#define timezone _timezone
#endif
typedef unsigned __int64 uint64_t;
#else
#define O_BINARY 0
#define ISNAN(x) std::isnan(x)
#include <cstdint>
#include <memory>
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define AGENT_SYMBOL_EXPORT __declspec(dllexport)
#define AGENT_SYMBOL_IMPORT __declspec(dllimport)
#else  // _WINDOWS
#define AGENT_SYMBOL_EXPORT __attribute__((visibility("default")))
#define AGENT_SYMBOL_IMPORT __attribute__((visibility("default")))
#endif  // _WINDOWS

#ifdef SHARED_AGENT_LIB

#ifdef AGENT_BUILD_SHARED_LIB
#define AGENT_LIB_API AGENT_SYMBOL_EXPORT
#else
#define AGENT_LIB_API AGENT_SYMBOL_IMPORT
#endif

#define AGENT_SYMBOL_VISIBLE AGENT_LIB_API

#else  // SHARED_AGENT_LIB

#define AGENT_LIB_API
#define AGENT_SYMBOL_VISIBLE

#endif
