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

#include <list>
#include <map>
#include <set>
#include <variant>

namespace mtconnect::sink::rest_sink {
  class ParameterError : public std::logic_error
  {
    using std::logic_error::logic_error;
  };

  enum ParameterType
  {
    NONE = 0,
    STRING = 1,
    INTEGER = 2,
    UNSIGNED_INTEGER = 3,
    DOUBLE = 4
  };

  enum UrlPart
  {
    PATH,
    QUERY
  };

  using ParameterValue = std::variant<std::monostate, std::string, int32_t, uint64_t, double>;
  struct Parameter
  {
    Parameter() = default;
    Parameter(const std::string &n, ParameterType t = STRING, UrlPart p = PATH)
      : m_name(n), m_type(t), m_part(p)
    {}
    Parameter(const Parameter &o) = default;

    std::string m_name;
    ParameterType m_type {STRING};
    ParameterValue m_default;
    UrlPart m_part {PATH};

    bool operator<(const Parameter &o) const { return m_name < o.m_name; }
  };

  using ParameterList = std::list<Parameter>;
  using QuerySet = std::set<Parameter>;
  using ParameterMap = std::map<std::string, ParameterValue>;
  using QueryMap = std::map<std::string, std::string>;
}  // namespace mtconnect::sink::rest_sink
