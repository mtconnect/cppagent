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

/// @file utilities.hpp
/// @brief Common utility functions

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/regex.hpp>
#include <boost/uuid/detail/sha1.hpp>

#include <chrono>
#include <date/date.h>
#include <filesystem>
#include <map>
#include <mtconnect/version.h>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include "mtconnect/config.hpp"
#include "mtconnect/logging.hpp"

// ####### CONSTANTS #######

// Port number to put server on
const unsigned int SERVER_PORT = 8080;

// Size of sliding buffer
const unsigned int DEFAULT_SLIDING_BUFFER_SIZE = 131072;

// Size of buffer exponent: 2^SLIDING_BUFFER_EXP
const unsigned int DEFAULT_SLIDING_BUFFER_EXP = 17;
const unsigned int DEFAULT_MAX_ASSETS = 1024;

namespace boost::asio {
  class io_context;
}

/// @brief MTConnect namespace
///
/// Top level mtconnect namespace
namespace mtconnect {
  // Message for when enumerations do not exist in an array/enumeration
  const int ENUM_MISS = -1;

  /// @brief Time formats
  enum TimeFormat
  {
    HUM_READ,    ///< Human readable
    GMT,         ///< GMT or UTC with second resolution
    GMT_UV_SEC,  ///< GMT with microsecond resolution
    LOCAL        ///< Time using local time zone
  };

  /// @brief Converts string to floating point numberss
  /// @param[in] text the number
  /// @return the converted value or 0.0 if incorrect.
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

  /// @brief Converts string to integer
  /// @param[in] text the number
  /// @return the converted value or 0 if incorrect.
  inline int stringToInt(const std::string &text, int outOfRangeDefault)
  {
    int value = 0;
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

  /// @brief converts a double to a string
  /// @param[in] value the double
  /// @return the string representation of the double (10 places max)
  inline std::string format(double value)
  {
    std::stringstream s;
    constexpr int precision = std::numeric_limits<double>::digits10;
    s << std::setprecision(precision) << value;
    return s.str();
  }

  /// @brief inline formattor support for doubles
  class format_double_stream
  {
  protected:
    double val;

  public:
    /// @brief create a formatter
    /// @param[in] v the value
    format_double_stream(double v) { val = v; }

    /// @brief writes a double to an output stream with up to 10 digits of precision
    /// @tparam _CharT from std::basic_ostream
    /// @tparam _Traits from std::basic_ostream
    /// @param[in,out] os output stream
    /// @param[in] fmter reference to this formatter
    /// @return reference to the output stream
    template <class _CharT, class _Traits>
    inline friend std::basic_ostream<_CharT, _Traits> &operator<<(
        std::basic_ostream<_CharT, _Traits> &os, const format_double_stream &fmter)
    {
      constexpr int precision = std::numeric_limits<double>::digits10;
      os << std::setprecision(precision) << fmter.val;
      return os;
    }
  };

  /// @brief create a `format_doulble_stream`
  /// @param[in] v the value
  /// @return the format_double_stream
  inline format_double_stream formatted(double v) { return format_double_stream(v); }

  /// @brief Convert text to upper case
  /// @param[in,out] text text
  /// @return upper-case of text as string
  inline std::string toUpperCase(std::string &text)
  {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    return text;
  }

  /// @brief Simple check if a number as a string is negative
  /// @param s the numbeer
  /// @return `true` if positive
  inline bool isNonNegativeInteger(const std::string &s)
  {
    for (const char c : s)
    {
      if (!isdigit(c))
        return false;
    }

    return true;
  }

  /// @brief Checks if a string is a valid integer
  /// @param s the string
  /// @return `true` if is `[+-]\d+`
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

  /// @brief Gets the local time
  /// @param[in] time the time
  /// @param[out] buf struct tm
  AGENT_LIB_API void mt_localtime(const time_t *time, struct tm *buf);

  /// @brief Formats the timePoint as  string given the format
  /// @param[in] timePoint the time
  /// @param[in] format the format
  /// @return the time as a string
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

  /// @brief get the current time in the given format
  ///
  /// cover method for `getCurrentTime()` with `system_clock::now()`
  ///
  /// @param[in] format the format for the time
  /// @return the time as a text
  inline std::string getCurrentTime(TimeFormat format)
  {
    return getCurrentTime(std::chrono::system_clock::now(), format);
  }

  /// @brief Get the current time as a unsigned uns64 since epoch
  /// @tparam timePeriod the resolution type of time
  /// @return the time as an uns64
  template <class timePeriod>
  inline uint64_t getCurrentTimeIn()
  {
    return std::chrono::duration_cast<timePeriod>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  /// @brief Current time in microseconds since epoch
  /// @return the time as uns64 in microsecnods
  inline uint64_t getCurrentTimeInMicros() { return getCurrentTimeIn<std::chrono::microseconds>(); }

  /// @brief Current time in seconds since epoch
  /// @return the time as uns64 in seconds
  inline uint64_t getCurrentTimeInSec() { return getCurrentTimeIn<std::chrono::seconds>(); }

  /// @brief Parse the given time
  /// @param aTime the time in text
  /// @return uns64 in microseconds since epoch
  inline uint64_t parseTimeMicro(const std::string &aTime)
  {
    std::stringstream str(aTime);
    if (isdigit(aTime.back()))
    {
      str.seekp(0, std::ios_base::end);
      str << 'Z';
      str.seekg(0);
    }
    using micros = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;
    date::fields<std::chrono::microseconds> fields;
    std::chrono::minutes offset;
    std::string abbrev;
    date::from_stream(str, "%FT%T%Z", fields, &abbrev, &offset);
    if (!fields.ymd.ok() || !fields.tod.in_conventional_range())
      return 0;

    micros microdays {date::sys_days(fields.ymd)};
    auto us = fields.tod.to_duration().count() + microdays.time_since_epoch().count();
    return us;
  }

  /// @brief escaped reserved XML characters from text
  /// @param data text with reserved characters escaped
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

  /// @brief add namespace prefixes to each element of the XPath
  /// @param[in] aPath the path to modify
  /// @param[in] aPrefix the prefix to add
  /// @return the modified path prefixed
  AGENT_LIB_API std::string addNamespace(const std::string aPath, const std::string aPrefix);

  /// @brief determines of a string ends with an ending
  /// @param[in] value the string to check
  /// @param[in] ending the ending to verify
  /// @return `true` if the string ends with ending
  inline bool ends_with(const std::string &value, const std::string_view &ending)
  {
    if (ending.size() > value.size())
      return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
  }

  /// @brief removes white space at the beginning of a string
  /// @param[in,out] s the string
  /// @return string with spaces removed
  inline std::string ltrim(std::string s)
  {
    boost::algorithm::trim_left(s);
    return s;
  }

  /// @brief removes whitespace from the end of the string
  /// @param[in,out] s the string
  /// @return string with spaces removed
  static inline std::string rtrim(std::string s)
  {
    boost::algorithm::trim_right(s);
    return s;
  }

  /// @brief removes spaces from the beginning and end of a string
  /// @param[in] s the string
  /// @return string with spaces removed
  inline std::string trim(std::string s)
  {
    boost::algorithm::trim(s);
    return s;
  }

  /// @brief split a string into two parts using a ':' separator
  /// @param key the key to split
  /// @return a pair of the key and an optional prefix.
  static inline std::pair<std::string, std::optional<std::string>> splitKey(const std::string &key)
  {
    auto c = key.find(':');
    if (c != std::string::npos)
      return {key.substr(c + 1, std::string::npos), key.substr(0, c)};
    else
      return {key, std::nullopt};
  }

  /// @brief determines of a string starts with a beginning
  /// @param[in] value the string to check
  /// @param[in] beginning the beginning to verify
  /// @return `true` if the string begins with beginning
  inline bool starts_with(const std::string &value, const std::string_view &beginning)
  {
    if (beginning.size() > value.size())
      return false;
    return std::equal(beginning.begin(), beginning.end(), value.begin());
  }

  /// @brief Case insensitive equals
  /// @param a first string
  /// @param b second string
  /// @return `true` if equal
  inline bool iequals(const std::string &a, const std::string_view &b)
  {
    if (a.size() != b.size())
      return false;

    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char a, char b) {
             return tolower(a) == tolower(b);
           });
  }

  using Attributes = std::map<std::string, std::string>;

  /// @brief overloaded pattern for variant visitors using list of lambdas
  /// @tparam ...Ts list of lambda classes
  template <class... Ts>
  struct overloaded : Ts...
  {
    using Ts::operator()...;
  };
  template <class... Ts>
  overloaded(Ts...) -> overloaded<Ts...>;

  /// @brief Reverse an iterable
  /// @tparam T The iterable type
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

  /// @brief observation sequence type
  using SequenceNumber_t = uint64_t;
  /// @brief set of data item ids for filtering
  using FilterSet = std::set<std::string>;
  using FilterSetOpt = std::optional<FilterSet>;
  using Milliseconds = std::chrono::milliseconds;
  using Microseconds = std::chrono::microseconds;
  using Seconds = std::chrono::seconds;
  using Timestamp = std::chrono::time_point<std::chrono::system_clock>;
  using Timestamp = std::chrono::time_point<std::chrono::system_clock>;
  using StringList = std::list<std::string>;

  /// @name Configuration related methods
  ///@{

  /// @brief Variant for configuration options
  using ConfigOption = std::variant<std::monostate, bool, int, std::string, double, Seconds,
                                    Milliseconds, StringList>;
  /// @brief A map of name to option value
  using ConfigOptions = std::map<std::string, ConfigOption>;

  /// @brief Get an option if available
  /// @tparam T the option type
  /// @param options the set of options
  /// @param name the name to get
  /// @return the value of the option otherwise std::nullopt
  template <typename T>
  inline const std::optional<T> GetOption(const ConfigOptions &options, const std::string &name)
  {
    auto v = options.find(name);
    if (v != options.end())
      return std::get<T>(v->second);
    else
      return std::nullopt;
  }

  /// @brief checks if a boolean option is set
  /// @param options the set of options
  /// @param name the name of the option
  /// @return `true` if the option exists and has a bool type
  inline bool IsOptionSet(const ConfigOptions &options, const std::string &name)
  {
    auto v = options.find(name);
    if (v != options.end())
      return std::get<bool>(v->second);
    else
      return false;
  }

  /// @brief checks if there is an option
  /// @param[in] options the set of options
  /// @param[in] name the name of the option
  /// @return `true` if the option exists
  inline bool HasOption(const ConfigOptions &options, const std::string &name)
  {
    auto v = options.find(name);
    return v != options.end();
  }

  /// @brief convert an option from a string to a typed option
  /// @param[in] s the
  /// @param[in] def template for the option
  /// @return a typed option matching `def`
  inline auto ConvertOption(const std::string &s, const ConfigOption &def,
                            const ConfigOptions &options)
  {
    ConfigOption option {s};
    if (std::holds_alternative<std::string>(option))
    {
      std::string sv = std::get<std::string>(option);
      visit(overloaded {[&option, &sv](const std::string &) {
                          if (sv.empty())
                            option = std::monostate();
                          else
                            option = sv;
                        },
                        [&option, &sv](const int &) { option = stoi(sv); },
                        [&option, &sv](const Milliseconds &) { option = Milliseconds {stoi(sv)}; },
                        [&option, &sv](const Seconds &) { option = Seconds {stoi(sv)}; },
                        [&option, &sv](const double &) { option = stod(sv); },
                        [&option, &sv](const bool &) { option = sv == "yes" || sv == "true"; },
                        [&option, &sv](const StringList &) {
                          StringList list;
                          boost::split(list, sv, boost::is_any_of(","));
                          for (auto &s : list)
                            boost::trim(s);
                          option = list;
                        },
                        [](const auto &) {}},
            def);
    }
    return option;
  }

  /// @brief convert from a string option to a size
  ///
  /// Recognizes the following suffixes:
  /// - [Gg]: Gigabytes
  /// - [Mm]: Megabytes
  /// - [Kk]: Kilobytes
  ///
  /// @param[in] options A set of options
  /// @param[in] name the name of the options
  /// @param[in] size the default size (0)
  /// @return the size honoring suffixes
  inline int64_t ConvertFileSize(const ConfigOptions &options, const std::string &name,
                                 int64_t size = 0)
  {
    using namespace std;
    using boost::regex;
    using boost::smatch;

    auto value = GetOption<string>(options, name);
    if (value)
    {
      static const regex pat("([0-9]+)([GgMmKkBb]*)");
      smatch match;
      string v = *value;
      if (regex_match(v, match, pat))
      {
        size = boost::lexical_cast<int64_t>(match[1]);
        if (match[2].matched)
        {
          switch (match[2].str()[0])
          {
            case 'G':
            case 'g':
              size *= 1024;

            case 'M':
            case 'm':
              size *= 1024;

            case 'K':
            case 'k':
              size *= 1024;
          }
        }
      }
      else
      {
        std::stringstream msg;
        msg << "Invalid value for " << name << ": " << *value << endl;
        throw std::runtime_error(msg.str());
      }
    }

    return size;
  }

  /// @brief adds a property tree node to an option set
  /// @param[in] tree the property tree coming from configuration parser
  /// @param[in,out] options the options set
  /// @param[in] entries a set of typed options to check
  inline void AddOptions(const boost::property_tree::ptree &tree, ConfigOptions &options,
                         const ConfigOptions &entries)
  {
    for (auto &e : entries)
    {
      auto val = tree.get_optional<std::string>(e.first);
      if (val)
      {
        try
        {
          auto v = ConvertOption(*val, e.second, options);
          if (v.index() != 0)
            options.insert_or_assign(e.first, v);
        }
        catch (std::exception ex)
        {
          LOG(error) << "Cannot convert option " << val << ": " << ex.what();
        }
      }
    }
  }

  /// @brief adds a property tree node to an option set with defaults
  /// @param[in] tree the property tree coming from configuration parser
  /// @param[in,out] options the option set
  /// @param[in] entries the options with default values
  inline void AddDefaultedOptions(const boost::property_tree::ptree &tree, ConfigOptions &options,
                                  const ConfigOptions &entries)
  {
    for (auto &e : entries)
    {
      auto val = tree.get_optional<std::string>(e.first);
      if (val)
      {
        try
        {
          auto v = ConvertOption(*val, e.second, options);
          if (v.index() != 0)
            options.insert_or_assign(e.first, v);
        }
        catch (std::exception ex)
        {
          LOG(error) << "Cannot convert option " << val << ": " << ex.what();
        }
      }
      else if (options.find(e.first) == options.end())
        options.insert_or_assign(e.first, e.second);
    }
  }

  /// @brief combine two option sets
  /// @param[in,out] options existing set of options
  /// @param[in] entries options to add or update
  inline void MergeOptions(ConfigOptions &options, const ConfigOptions &entries)
  {
    for (auto &e : entries)
    {
      options.insert_or_assign(e.first, e.second);
    }
  }

  /// @brief get options from a property tree and create typed options
  /// @param[in] tree the property tree coming from configuration parser
  /// @param[in,out] options option set to modify
  /// @param[in] entries a set of typed options to check
  inline void GetOptions(const boost::property_tree::ptree &tree, ConfigOptions &options,
                         const ConfigOptions &entries)
  {
    for (auto &e : entries)
    {
      if (!std::holds_alternative<std::string>(e.second) ||
          !std::get<std::string>(e.second).empty())
      {
        options.emplace(e.first, e.second);
      }
    }
    AddOptions(tree, options, entries);
  }

  /// @}

  /// @brief Format a timestamp as a string in microseconds
  /// @param[in] ts the timestamp
  /// @return the time with microsecond resolution
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

  /// @brief Capitalize a word
  ///
  /// Has special treatment of acronyms like AC, DC, PH, etc.
  ///
  /// @param[in,out] start starting iterator
  /// @param[in,out] end ending iterator
  inline void capitalize(std::string::iterator start, std::string::iterator end)
  {
    using namespace std;

    // Exceptions to the rule
    const static std::unordered_map<std::string, std::string> exceptions = {
        {"AC", "AC"}, {"DC", "DC"},   {"PH", "PH"},
        {"IP", "IP"}, {"URI", "URI"}, {"MTCONNECT", "MTConnect"}};

    const auto &w = exceptions.find(std::string(start, end));
    if (w != exceptions.end())
    {
      copy(w->second.begin(), w->second.end(), start);
    }
    else
    {
      *start = ::toupper(*start);
      start++;
      transform(start, end, start, ::tolower);
    }
  }

  /// @brief creates an upper-camel-case string from words separated by an underscore (`_`) with
  /// optional prefix
  ///
  /// Uses `capitalize()` method to capitalize words.
  ///
  /// @param[in] type the words to capitalize
  /// @param[out] prefix the prefix of the string
  /// @return a pascalized upper-camel-case string
  inline std::string pascalize(const std::string &type, std::optional<std::string> &prefix)
  {
    using namespace std;
    if (type.empty())
      return "";

    string camel;
    auto colon = type.find(':');

    if (colon != string::npos)
    {
      prefix = type.substr(0ul, colon);
      camel = type.substr(colon + 1ul);
    }
    else
      camel = type;

    auto start = camel.begin();
    decltype(start) end;

    bool done;
    do
    {
      end = find(start, camel.end(), '_');
      capitalize(start, end);
      done = end == camel.end();
      if (!done)
      {
        camel.erase(end);
        start = end;
      }
    } while (!done);

    return camel;
  }

  /// @brief parse a string timestamp to a `Timestamp`
  /// @param timestamp[in] the timestamp as a string
  /// @return converted `Timestamp`
  inline Timestamp parseTimestamp(const std::string &timestamp)
  {
    using namespace date;
    using namespace std::chrono;
    using namespace std::chrono_literals;
    using namespace date::literals;

    Timestamp ts;
    std::istringstream in(timestamp);
    in >> std::setw(6) >> parse("%FT%T", ts);
    if (!in.good())
    {
      ts = std::chrono::system_clock::now();
    }
    return ts;
  }

/// @brief Creates a comparable schema version from a major and minor number
#define SCHEMA_VERSION(major, minor) (major * 100 + minor)

  /// @brief Get the default schema version of the agent as a string
  /// @return the version
  inline std::string StrDefaultSchemaVersion()
  {
    return std::to_string(AGENT_VERSION_MAJOR) + "." + std::to_string(AGENT_VERSION_MINOR);
  }

  inline constexpr int32_t IntDefaultSchemaVersion()
  {
    return SCHEMA_VERSION(AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR);
  }

  /// @brief convert a string version to a major and minor as two integers separated by a char.
  /// @param s the version
  inline int32_t IntSchemaVersion(const std::string &s)
  {
    int major {0}, minor {0};
    char c;
    std::stringstream vstr(s);
    vstr >> major >> c >> minor;
    if (major == 0)
    {
      return IntDefaultSchemaVersion();
    }
    else
    {
      return SCHEMA_VERSION(major, minor);
    }
  }

  /// @brief Retrieve the best Host IP address from the network interfaces.
  /// @param[in] context the boost asio io_context for resolving the address
  /// @param[in] onlyV4 only consider IPV4 addresses if `true`
  std::string GetBestHostAddress(boost::asio::io_context &context, bool onlyV4 = false);

  /// @brief Function to create a unique id given a sha1 namespace and an id.
  ///
  /// Creates a base 64 encoded version of the string and removes any illegal characters
  /// for an ID. If the first character is not a legal start character, maps the first 2 characters
  /// to the legal ID start char set.
  ///
  /// @param[in] sha the sha1 namespace to use as context
  /// @param[in] id the id to use transform
  /// @returns Returns the first 16 characters of the  base 64 encoded sha1
  inline std::string makeUniqueId(const ::boost::uuids::detail::sha1 &sha, const std::string &id)
  {
    using namespace std;

    ::boost::uuids::detail::sha1 sha1(sha);

    constexpr string_view startc("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_");
    constexpr auto isIDStartChar = [](unsigned char c) -> bool { return isalpha(c) || c == '_'; };
    constexpr auto isIDChar = [isIDStartChar](unsigned char c) -> bool {
      return isIDStartChar(c) || isdigit(c) || c == '.' || c == '-';
    };

    sha1.process_bytes(id.data(), id.length());
    unsigned int digest[5];
    sha1.get_digest(digest);

    string s(32, ' ');
    auto len = boost::beast::detail::base64::encode(s.data(), digest, sizeof(digest));

    s.erase(len - 1);
    s.erase(std::remove_if(++(s.begin()), s.end(), not_fn(isIDChar)), s.end());

    // Check if the character is legal.
    if (!isIDStartChar(s[0]))
    {
      // Change the start character to a legal character
      uint32_t c = s[0] + s[1];
      s.erase(0, 1);
      s[0] = startc[c % startc.size()];
    }

    s.erase(16);

    return s;
  }

  namespace url {
    using UrlQueryPair = std::pair<std::string, std::string>;

    /// @brief A map of URL query parameters that can format as a string
    struct AGENT_LIB_API UrlQuery : public std::map<std::string, std::string>
    {
      using std::map<std::string, std::string>::map;

      /// @brief join the parameters as `<key1>=<value1>&<key2>=<value2>&...`
      /// @return
      std::string join() const
      {
        std::stringstream ss;
        bool has_pre = false;

        for (const auto &kv : *this)
        {
          if (has_pre)
            ss << '&';

          ss << kv.first << '=' << kv.second;
          has_pre = true;
        }

        return ss.str();
      }

      /// @brief Merge twos sets over-writing existing pairs set with `query` and adding new pairs
      /// @param query query to merge
      void merge(UrlQuery query)
      {
        for (const auto &kv : query)
        {
          insert_or_assign(kv.first, kv.second);
        }
      }
    };

    /// @brief URL struct to parse and format URLs
    struct AGENT_LIB_API Url
    {
      /// @brief Variant for the Host that is either a host name or an ip address
      using Host = std::variant<std::string, boost::asio::ip::address>;

      std::string m_protocol;  ///< either `http` or `https`

      Host m_host;                            ///< the host component
      std::optional<std::string> m_username;  ///< optional username
      std::optional<std::string> m_password;  ///< optional password
      std::optional<int> m_port;              ///< The optional port number (defaults to 5000)
      std::string m_path = "/";               ///< The path component
      UrlQuery m_query;                       ///< Query parameters
      std::string m_fragment;                 ///< The component after a `#`

      /// @brief Visitor to format the Host as a string
      struct HostVisitor
      {
        std::string operator()(std::string v) const { return v; }

        std::string operator()(boost::asio::ip::address v) const { return v.to_string(); }
      };

      /// @brief Get the host as a string
      /// @return the host
      std::string getHost() const { return std::visit(HostVisitor(), m_host); }
      /// @brief Get the port as a string
      /// @return the port
      std::string getService() const { return boost::lexical_cast<std::string>(getPort()); }

      /// @brief Get the path and the query portion of the URL
      /// @return the path and query
      std::string getTarget() const
      {
        if (m_query.size())
          return m_path + '?' + m_query.join();
        else
          return m_path;
      }

      /// @brief Format a target using the existing host and port to make a request
      /// @param device an optional device
      /// @param operation the operation (probe,sample,current, or asset)
      /// @param query query parameters
      /// @return A string with the target for a GET reuest
      std::string getTarget(const std::optional<std::string> &device, const std::string &operation,
                            const UrlQuery &query) const
      {
        UrlQuery uq {m_query};
        if (!query.empty())
          uq.merge(query);

        std::stringstream path;
        path << m_path;
        if (m_path[m_path.size() - 1] != '/')
          path << '/';
        if (device)
          path << *device << '/';
        if (!operation.empty())
          path << operation;
        if (uq.size() > 0)
          path << '?' << uq.join();

        return path.str();
      }

      int getPort() const
      {
        if (m_port)
          return *m_port;
        else if (m_protocol == "https")
          return 443;
        else if (m_protocol == "http")
          return 80;
        else
          return 0;
      }

      /// @brief Format the URL as text
      /// @param device optional device to add to the URL
      /// @return formatted URL
      std::string getUrlText(const std::optional<std::string> &device)
      {
        std::stringstream url;
        url << m_protocol << "://" << getHost() << ':' << getPort() << getTarget();
        if (device)
          url << *device;
        return url.str();
      }

      /// @brief parse a string to a Url
      /// @return parsed URL
      static Url parse(const std::string_view &url);
    };
  }  // namespace url
}  // namespace mtconnect
