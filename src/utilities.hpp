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
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <time.h>
#include <variant>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
namespace beast=boost::beast;
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

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
  inline double stringToFloat(const std::string &text)
  {
    double value = 0.0;
    try
    {
      value = stof(text);
    }
    catch (const std::out_of_range &)
    {
      value = 0.0;
    }
    catch (const std::invalid_argument &)
    {
      value = 0.0;
    }
    return value;
  }

  inline int stringToInt(const std::string &text, int outOfRangeDefault)
  {
    int value = 0.0;
    try
    {
      value = stoi(text);
    }
    catch (const std::out_of_range &)
    {
      value = outOfRangeDefault;
    }
    catch (const std::invalid_argument &)
    {
      value = 0;
    }
    return value;
  }

  // Convert a float to string
  inline std::string format(double value)
  {
    std::stringstream s;
    constexpr int precision = std::numeric_limits<double>::digits10;
    s << std::setprecision(precision) << value;
    return s.str();
  }

  class format_double_stream
  {
  protected:
    double val;

  public:
    format_double_stream(double v) { val = v; }

    template <class _CharT, class _Traits>
    inline friend std::basic_ostream<_CharT, _Traits> &operator<<(
        std::basic_ostream<_CharT, _Traits> &os, const format_double_stream &fmter)
    {
      constexpr int precision = std::numeric_limits<double>::digits10;
      os << std::setprecision(precision) << fmter.val;
      return os;
    }
  };

  inline format_double_stream formatted(double v) { return format_double_stream(v); }

  // Convert a string to the same string with all upper case letters
  inline std::string toUpperCase(std::string &text)
  {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    return text;
  }

  // Check if each char in a string is a positive integer
  inline bool isNonNegativeInteger(const std::string &s)
  {
    for (const char c : s)
    {
      if (!isdigit(c))
        return false;
    }

    return true;
  }

  inline bool isInteger(const std::string &s)
  {
    auto iter = s.cbegin();
    if (*iter == '-' || *iter == '+')
      ++iter;

    for (; iter != s.end(); iter++)
    {
      if (!isdigit(*iter))
        return false;
    }

    return true;
  }

  void mt_localtime(const time_t *time, struct tm *buf);

  // Get a specified time formatted
  inline std::string getCurrentTime(std::chrono::time_point<std::chrono::system_clock> timePoint,
                                    TimeFormat format)
  {
    using namespace std;
    using namespace std::chrono;
    constexpr char ISO_8601_FMT[] = "%Y-%m-%dT%H:%M:%SZ";

    switch (format)
    {
      case HUM_READ:
        return date::format("%a, %d %b %Y %H:%M:%S GMT", date::floor<seconds>(timePoint));
      case GMT:
        return date::format(ISO_8601_FMT, date::floor<seconds>(timePoint));
      case GMT_UV_SEC:
        return date::format(ISO_8601_FMT, date::floor<microseconds>(timePoint));
      case LOCAL:
        auto time = system_clock::to_time_t(timePoint);
        struct tm timeinfo = {0};
        mt_localtime(&time, &timeinfo);
        char timestamp[64] = {0};
        strftime(timestamp, 50u, "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
        return timestamp;
    }

    return "";
  }

  // Get the current time formatted
  inline std::string getCurrentTime(TimeFormat format)
  {
    return getCurrentTime(std::chrono::system_clock::now(), format);
  }

  template <class timePeriod>
  inline uint64_t getCurrentTimeIn()
  {
    return std::chrono::duration_cast<timePeriod>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  // time_t to the ms
  inline uint64_t getCurrentTimeInMicros() { return getCurrentTimeIn<std::chrono::microseconds>(); }

  inline uint64_t getCurrentTimeInSec() { return getCurrentTimeIn<std::chrono::seconds>(); }

  // Get the current time in number of seconds as an integer
  uint64_t getCurrentTimeInSec();

  uint64_t parseTimeMicro(const std::string &aTime);

  // Replace illegal XML characters with the correct corresponding characters
  inline void replaceIllegalCharacters(std::string &data)
  {
    for (auto i = 0u; i < data.length(); i++)
    {
      char c = data[i];

      switch (c)
      {
        case '&':
          data.replace(i, 1, "&amp;");
          break;

        case '<':
          data.replace(i, 1, "&lt;");
          break;

        case '>':
          data.replace(i, 1, "&gt;");
          break;
      }
    }
  }

  std::string addNamespace(const std::string aPath, const std::string aPrefix);

  // Ends with
  inline bool ends_with(const std::string &value, const std::string_view &ending)
  {
    if (ending.size() > value.size())
      return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
  }

  inline bool starts_with(const std::string &value, const std::string_view &beginning)
  {
    if (beginning.size() > value.size())
      return false;
    return std::equal(beginning.begin(), beginning.end(), value.begin());
  }

  inline bool iequals(const std::string &a, const std::string_view &b)
  {
    if (a.size() != b.size())
      return false;

    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char a, char b) {
             return tolower(a) == tolower(b);
           });
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
  using Microseconds = std::chrono::microseconds;
  using Seconds = std::chrono::seconds;
  using Timestamp = std::chrono::time_point<std::chrono::system_clock>;
  using StringList = std::list<std::string>;
  using ConfigOption = std::variant<std::monostate, bool, int, std::string, double, Seconds,
                                    Milliseconds, StringList>;
  using ConfigOptions = std::map<std::string, ConfigOption>;
  template <typename T>
  inline const std::optional<T> GetOption(const ConfigOptions &options, const std::string &name)
  {
    auto v = options.find(name);
    if (v != options.end())
      return std::get<T>(v->second);
    else
      return std::nullopt;
  }

  inline bool IsOptionSet(const ConfigOptions &options, const std::string &name)
  {
    auto v = options.find(name);
    if (v != options.end())
      return std::get<bool>(v->second);
    else
      return false;
  }

  inline std::string format(const Timestamp &ts)
  {
    using namespace std;
    string time = date::format("%FT%T", date::floor<Microseconds>(ts));
    auto pos = time.find_last_not_of("0");
    if (pos != string::npos)
    {
      if (time[pos] != '.')
        pos++;
      time.erase(pos);
    }
    time.append("Z");
    return time;
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
