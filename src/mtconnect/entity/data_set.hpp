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

#include "mtconnect/config.hpp"
#include "mtconnect/logging.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::entity {
  struct DataSetEntry;

  /// @brief A set of data set entries
  class AGENT_LIB_API DataSet : public std::set<DataSetEntry>
  {
  public:
    using base = std::set<DataSetEntry>;
    using base::base;

    /// @brief Get a entry for a key
    /// @tparam T the entry type
    /// @param key the key
    /// @return the typed value of the entry
    template <typename T>
    const T &get(const std::string &key) const;

    /// @brief Get a entry for a key if it exists
    /// @tparam T the entry type
    /// @param key the key
    /// @return optional typed value of the entry
    template <typename T>
    const std::optional<T> maybeGet(const std::string &key) const;

    /// @brief Split the data set entries by space delimiters and account for the
    /// use of single and double quotes as well as curly braces
    bool parse(const std::string &s, bool table);
  };

  /// @brief Data set value variant
  using DataSetValue = std::variant<std::monostate, DataSet, std::string, int64_t, double>;

  /// @brief Equality visitor for a DataSetValue
  struct DataSetValueSame
  {
    DataSetValueSame(const DataSetValue &other) : m_other(other) {}

    bool operator()(const DataSet &v);

    /// @brief Compare the types are the same and the values are the same
    /// @tparam T the data type
    /// @param v the other value
    /// @return `true` if they are the same
    template <class T>
    bool operator()(const T &v)
    {
      return std::holds_alternative<T>(m_other) && std::get<T>(m_other) == v;
    }

  private:
    const DataSetValue &m_other;
  };

  /// @brief One entry in a data set. Has necessary interface to be work with maps.
  struct DataSetEntry
  {
    /// @brief Create an entry with a key and value
    /// @param key the key
    /// @param value the value as a string
    /// @param removed `true` if the key has been removed
    DataSetEntry(std::string key, std::string &value, bool removed = false)
      : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
    {}

    /// @brief Create an entry for a table with a data set value
    /// @param key the key
    /// @param value the value as a DataSet
    /// @param removed `true` if the key has been removed
    DataSetEntry(std::string key, DataSet &value, bool removed = false)
      : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
    {}
    /// @brief Create an entry for a data set
    /// @param key the key
    /// @param value the a data set variant
    /// @param removed `true` if the key has been removed
    DataSetEntry(std::string key, DataSetValue value, bool removed = false)
      : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
    {}
    /// @brief Create a data set entry with just a key (used for search)
    /// @param key
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

  /// @brief Get a typed value from a data set
  /// @tparam T the data type
  /// @param key the key to search for
  /// @return a typed value reference
  /// @throws  std::bad_variant_access when type is incorrect
  template <typename T>
  const T &DataSet::get(const std::string &key) const
  {
    return std::get<T>(find(DataSetEntry(key))->m_value);
  }

  /// @brief Get a typed value if available
  /// @tparam T they type
  /// @param key the key to search for
  /// @return an opton typed result
  template <typename T>
  const std::optional<T> DataSet::maybeGet(const std::string &key) const
  {
    auto v = find(DataSetEntry(key));
    if (v == end())
      return std::nullopt;
    else
      return std::get<T>(v->m_value);
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
