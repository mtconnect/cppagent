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

#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <variant>

#include "error.hpp"
#include "mtconnect/config.hpp"

namespace mtconnect::sink::rest_sink {
  /// @brief Parameter related errors thrown during interpreting a REST request
  class AGENT_LIB_API ParameterError : public std::invalid_argument
  {
    using std::invalid_argument::invalid_argument;
  };

  /// @brief The parameter type for a REST request
  enum ParameterType : uint8_t
  {
    NONE = 0,              ///< No specific type
    STRING = 1,            ///< A string
    INTEGER = 2,           ///< A signed integer
    UNSIGNED_INTEGER = 3,  ///< An unsigned integer
    DOUBLE = 4,            ///< A double
    BOOL = 5               ///< A boolean
  };

  /// @brief The part of the path the parameter is related to
  enum UrlPart
  {
    PATH,  ///< The portion before the `?`
    QUERY  ///< The portion after the `?`
  };

  /// @brief The variant for query parameters
  using ParameterValue = std::variant<std::monostate, std::string, int32_t, uint64_t, double, bool>;
  /// @brief A struct to hold a parameter template for matching portions of a REST URI
  struct Parameter
  {
    Parameter() = default;
    /// @brief Create a parameter
    /// @param n the name of the parameter
    /// @param t the parameter type. defaults to STRING
    /// @param p path or query portion of the URI
    Parameter(const std::string &n, ParameterType t = STRING, UrlPart p = PATH)
      : m_name(n), m_type(t), m_part(p)
    {}
    Parameter(const Parameter &o) = default;

    /// @brief to support std::set interface
    bool operator<(const Parameter &o) const { return m_name < o.m_name; }

    const std::string getTypeName() const
    {
      switch (m_type)
      {
        case NONE:
          return "unknown";

        case STRING:
          return "string";

        case INTEGER:
          return "integer";

        case UNSIGNED_INTEGER:
          return "integer";

        case DOUBLE:
          return "double";

        case BOOL:
          return "boolean";
      }

      return "unknown";
    }

    const std::string getTypeFormat() const
    {
      switch (m_type)
      {
        case NONE:
          return "unknown";

        case STRING:
          return "string";

        case INTEGER:
          return "int32";

        case UNSIGNED_INTEGER:
          return "uint64";

        case DOUBLE:
          return "double";

        case BOOL:
          return "bool";
      }

      return "unknown";
    }
    
    /// @brief Helper to convert a ParameterValue to a string
    static std::string toString(const ParameterValue &v)
    {
      using namespace std::string_literals;
      return std::visit(overloaded {[](const std::monostate &) { return "none"s; },
        [](const std::string &s) { return s; },
        [](int32_t i) { return std::to_string(i); },
        [](uint64_t i) { return std::to_string(i); },
        [](double d) { return std::to_string(d); },
        [](bool b) { return b ? "true"s : "false"s; }},
                        v);
    }


    std::string m_name;
    ParameterType m_type {STRING};
    /// @brief Default value if one is available
    ParameterValue m_default;
    UrlPart m_part {PATH};

    std::optional<std::string> m_description;
  };

  /// @brief A reusable documentation holder to assign to multiple parameters after they are parsed
  struct ParameterDoc
  {
    /// @brief create reusable documentation for parameters with a name and a URL part
    /// @param[in] name name of the parameter
    /// @param[in] part part of the URL: `PATH` or `QUERY`
    /// @param[in] summary brief description of the parameter
    /// @param[in] description detailed description of the parameter
    ParameterDoc(const std::string &name, UrlPart part,
                 const std::optional<std::string> description)
      : m_name(name), m_part(part), m_description(description)
    {}

    std::string m_name;
    UrlPart m_part;
    std::optional<std::string> m_description;
  };

  /// @brief Documentation List
  using ParameterDocList = std::list<ParameterDoc>;

  /// @brief Ordered list of path parameters as they appear in the URI
  using ParameterList = std::list<Parameter>;
  /// @brief set of query parameters
  using QuerySet = std::set<Parameter>;
  /// @brief Associates a parameter name with a value
  using ParameterMap = std::map<std::string, ParameterValue>;
  /// @brief associates a query parameter with a string value
  using QueryMap = std::map<std::string, std::string>;
}  // namespace mtconnect::sink::rest_sink
