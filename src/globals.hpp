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

#pragma once

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

#include <date/date.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <limits>
#include <list>
#include <map>
#include <sstream>
#include <string>
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
#include <sys/resource.h>

#include <cstdint>
#include <memory>
#include <unistd.h>
#endif

//####### CONSTANTS #######

// Port number to put server on
const unsigned int SERVER_PORT = 8080;

// Size of sliding buffer
const unsigned int DEFAULT_SLIDING_BUFFER_SIZE = 131072;

// Size of buffer exponent: 2^SLIDING_BUFFER_EXP
const unsigned int DEFAULT_SLIDING_BUFFER_EXP = 17;
const unsigned int DEFAULT_MAX_ASSETS = 1024;

namespace mtconnect
{
  // Message for when enumerations do not exist in an array/enumeration
  const int ENUM_MISS = -1;

  // Time format
  enum TimeFormat
  {
    HUM_READ,
    GMT,
    GMT_UV_SEC,
    LOCAL
  };

  //####### METHODS #######
  float stringToFloat(const std::string &text);
  int stringToInt(const std::string &text, int outOfRangeDefault = 0);

  // Convert a float to string
  std::string floatToString(double f);

  // Convert a string to the same string with all upper case letters
  std::string toUpperCase(std::string &text);

  // Check if each char in a string is a positive integer
  bool isNonNegativeInteger(const std::string &s);
  bool isInteger(const std::string &s);

  // Get a specified time formatted
  std::string getCurrentTime(std::chrono::time_point<std::chrono::system_clock> timePoint,
                             TimeFormat format);

  // Get the current time formatted
  std::string getCurrentTime(TimeFormat format);

  // time_t to the ms
  uint64_t getCurrentTimeInMicros();

  // Get the relative time from using an uint64 offset in ms to time_t as a web time
  std::string getRelativeTimeString(uint64_t aTime);

  // Get the current time in number of seconds as an integer
  uint64_t getCurrentTimeInSec();

  uint64_t parseTimeMicro(const std::string &aTime);

  // Replace illegal XML characters with the correct corresponding characters
  void replaceIllegalCharacters(std::string &data);

  std::string addNamespace(const std::string aPath, const std::string aPrefix);

  // Ends with
  inline bool ends_with(const std::string &value, const std::string &ending)
  {
    if (ending.size() > value.size())
      return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
  }

  inline bool iequals(const std::string &a, const std::string &b)
  {
    return std::equal(a.begin(), a.end(), b.begin(),
                      [](char a, char b) { return tolower(a) == tolower(b); });
  }

  typedef std::map<std::string, std::string> Attributes;

  template <class... Ts>
  struct overloaded : Ts...
  {
    using Ts::operator()...;
  };
  template <class... Ts>
  overloaded(Ts...) -> overloaded<Ts...>;

  template <typename T>
  class reverse
  {
  private:
    T &m_iterable;

  public:
    explicit reverse(T &iterable) : m_iterable(iterable) {}
    auto begin() const { return std::rbegin(m_iterable); }
    auto end() const { return std::rend(m_iterable); }
  };

  using Milliseconds = std::chrono::milliseconds;
  using Seconds = std::chrono::seconds;
  using Timestamp = std::chrono::time_point<std::chrono::system_clock>;
  using StringList = std::list<std::string>;
  using ConfigOption = std::variant<std::monostate, bool, int, std::string, double, Seconds,
                                    Milliseconds, StringList>;
  using ConfigOptions = std::map<std::string, ConfigOption>;
  template<typename T>
  const std::optional<T> GetOption(const ConfigOptions &options, const std::string &name)
  {
    auto v = options.find(name);
    if (v != options.end())
      return std::get<T>(v->second);
    else
      return std::nullopt;
  }

#ifdef _WINDOWS
#include <io.h>
  typedef long volatile AtomicInt;
#else
#ifdef MACOSX
#include <libkern/OSAtomic.h>
  typedef volatile long AtomicInt;
#else
  using AtomicInt = int;
#endif
#endif
}  // namespace mtconnect
