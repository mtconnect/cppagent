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

#include <cmath>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "logging.hpp"
#include "utilities.hpp"

namespace mtconnect::entity {
  struct DataSetEntry;

  class DataSet : public std::set<DataSetEntry>
  {
  public:
    using base = std::set<DataSetEntry>;
    using base::base;

    template <typename T>
    const T &get(const std::string &key) const;
    template <typename T>
    const std::optional<T> maybeGet(const std::string &key) const;

    bool parse(const std::string &s, bool table);
  };

  using DataSetValue = std::variant<std::monostate, DataSet, std::string, int64_t, double>;

  struct DataSetValueSame
  {
    DataSetValueSame(const DataSetValue &other) : m_other(other) {}

    bool operator()(const DataSet &v);
    template <class T>
    bool operator()(const T &v)
    {
      return std::holds_alternative<T>(m_other) && std::get<T>(m_other) == v;
    }

  private:
    const DataSetValue &m_other;
  };

  struct DataSetEntry
  {
    DataSetEntry(std::string key, std::string &value, bool removed = false)
      : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
    {}
    DataSetEntry(std::string key, DataSet &value, bool removed = false)
      : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
    {}
    DataSetEntry(std::string key, DataSetValue value, bool removed = false)
      : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
    {}
    DataSetEntry(std::string key) : m_key(std::move(key)), m_value(""), m_removed(false) {}
    DataSetEntry(const DataSetEntry &other) = default;
    DataSetEntry() : m_removed(false) {}

    std::string m_key;
    DataSetValue m_value;
    bool m_removed;

    bool operator==(const DataSetEntry &other) const { return m_key == other.m_key; }
    bool operator<(const DataSetEntry &other) const { return m_key < other.m_key; }

    bool same(const DataSetEntry &other) const
    {
      return m_key == other.m_key && m_removed == other.m_removed &&
             std::visit(DataSetValueSame(other.m_value), m_value);
    }
  };

  template <typename T>
  const T &DataSet::get(const std::string &key) const
  {
    return std::get<T>(find(DataSetEntry(key))->m_value);
  }

  template <typename T>
  const std::optional<T> DataSet::maybeGet(const std::string &key) const
  {
    auto v = find(DataSetEntry(key));
    if (v == end())
      return std::nullopt;
    else
      return get<T>(v->m_value);
  }

  inline bool DataSetValueSame::operator()(const DataSet &v)
  {
    if (!std::holds_alternative<DataSet>(m_other))
      return false;

    const auto &oset = std::get<DataSet>(m_other);

    if (v.size() != oset.size())
      return false;

    for (const auto &e1 : v)
    {
      const auto &e2 = oset.find(e1);
      if (e2 == oset.end() || !visit(DataSetValueSame(e2->m_value), e1.m_value))
        return false;
    }

    return true;
  }
}  // namespace mtconnect::entity
