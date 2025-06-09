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

  /// @brief the namespace to hold the inner abstract templates of the data set
  namespace data_set {
    /// @brief Compares two values where the first is a variant and the second is a variant value
    /// using ==
    /// @tparam T1 The variant type
    /// @tparam T2 The value type
    /// @param v1 The variant
    /// @param v2 The value to match against
    ///
    /// This method should be overloaded in the data_set namespace to implement types that
    /// do not have a simple `==` method. The method first makes sure the types are the same and
    /// then compares using `==`.
    template <typename T1, typename T2>
    inline bool SameValue(const T1 &v1, const T2 &v2)
    {
      return std::holds_alternative<T2>(v1) && std::get<T2>(v1) == v2;
    }
    
    /// @brief One entry in a data set. Has necessary interface to be work with maps.
    /// @tparam T The type of the underlying variant data
    template <typename T>
    struct AGENT_LIB_API Entry
    {
      /// @brief Create an entry with a key and value
      /// @param key the key
      /// @param value the value as a string
      /// @param removed `true` if the key has been removed
      Entry(const std::string &key, std::string &value, bool removed = false)
        : m_key(key), m_value(std::move(value)), m_removed(removed)
      {}

      /// @brief Create an entry with a key and value
      /// @param key the key
      /// @param value the value as a string
      /// @param removed `true` if the key has been removed
      Entry(std::string &&key, std::string &value, bool removed = false)
        : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
      {}

      /// @brief Create an entry for a table with a data set value
      /// @param key the key
      /// @param value the value as a DataSet
      /// @param removed `true` if the key has been removed
      Entry(const std::string &key, Entry &value, bool removed = false)
        : m_key(key), m_value(std::move(value)), m_removed(removed)
      {}

      /// @brief Create an entry for a data set
      /// @param key the key
      /// @param value the a data set variant
      /// @param removed `true` if the key has been removed
      Entry(const std::string &key, T value, bool removed = false)
        : m_key(key), m_value(std::move(value)), m_removed(removed)
      {}
      /// @brief Create a data set entry with just a key (used for search)
      /// @param key
      Entry(const std::string &key) : m_key(key), m_removed(false) {}
      Entry(const Entry &other) = default;
      Entry() : m_removed(false) {}

      /// @brief copy a data set entry from another
      Entry &operator=(const Entry &other)
      {
        m_key = other.m_key;
        m_value = other.m_value;
        m_removed = other.m_removed;
        return *this;
      }

      /// @brief only compares keys for equality
      bool operator==(const Entry &other) const { return m_key == other.m_key; }
      /// @brief only compares keys for less than
      bool operator<(const Entry &other) const { return m_key < other.m_key; }

      /// @brief Compares the values of the entiry
      /// @param other the other value to compare against `m_value`
      /// @returns `true` if the values are the same
      ///
      /// Compares using the `SameValue` free function in the `data_set` namespace. It must be overloaded
      /// for any special types required by the variant type T.
      bool sameValue(const Entry &other) const
      {
        const auto &ov = other.m_value;
        return std::visit([&ov](const auto &v) { return SameValue(ov, v); }, m_value);
      }

      /// @brief compare a data entry ewith another
      /// @param other the other entry  to compare
      bool same(const Entry &other) const
      {
        return m_key == other.m_key && m_removed == other.m_removed && sameValue(other);
      }

      std::string m_key;  ///< The key of the entry
      T m_value;          ///< The value of the entry
      bool m_removed;     ///< boolean indicator if this entry is removed.
    };

    /// @brief A set of data set entries
    /// @tparam EV the entry type for the set, must have < operator.
    template <typename ET>
    class AGENT_LIB_API Set : public std::set<ET>
    {
    public:
      using base = std::set<ET>;
      using base::base;

      /// @brief Get a entry for a key
      /// @tparam T the entry type
      /// @param key the key
      /// @return the typed value of the entry
      template <typename T>
      const T &get(const std::string &key) const
      {
        auto v = base::find(ET(key));
        if (v == this->end())
          throw std::logic_error("DataSet get: key not found '" + key + "'");
        else
          return std::get<T>(v->m_value);
      }

      /// @brief Get a entry for a key if it exists
      /// @tparam T the entry type
      /// @param key the key
      /// @return optional typed value of the entry
      template <typename T>
      const std::optional<T> maybeGet(const std::string &key) const
      {
        auto v = base::find(ET(key));
        if (v == this->end())
          return std::nullopt;
        else
          return std::get<T>(v->m_value);
      }
    };
  }  // namespace data_set

  /// @brief Data Set Value type enumeration
  enum class TabelCellType : std::uint16_t
  {
    EMPTY = 0x0,    ///< monostate for no value
    STRING = 0x02,  ///< string value
    INTEGER = 0x3,  ///< 64 bit integer
    DOUBLE = 0x4    ///< double
  };

  /// @brief Table Cell value variant
  using TableCellValue = std::variant<std::monostate, std::string, int64_t, double>;

  /// @brief A table cell which is an entry with table cell values
  using TableCell = data_set::Entry<TableCellValue>;

  /// @brief A table row as a data set of table cells
  using TableRow = data_set::Set<TableCell>;

  namespace data_set {
    /// @brief Compare a data set entry against a table row
    /// @tparam T1 The type of the other variant
    /// @param v1 The value of the other variant
    /// @param row The row we're comparing
    template <typename T1>
    inline bool SameValue(const T1 &v1, const TableRow &row)
    {
      if (!std::holds_alternative<TableRow>(v1))
        return false;

      const auto &orow = std::get<TableRow>(v1);

      if (row.size() != orow.size())
        return false;

      for (const auto &c1 : row)
      {
        const auto &c2 = orow.find(c1);
        if (c2 == orow.end() || !c2->sameValue(c1))
          return false;
      }

      return true;
    }
  }  // namespace data_set

  /// @brief Data Set Value type enumeration
  enum class DataSetValueType : std::uint16_t
  {
    EMPTY = 0x0,       ///< monostate for no value
    TABLE_ROW = 0x01,  ///< data set member for tables
    STRING = 0x02,     ///< string value
    INTEGER = 0x3,     ///< 64 bit integer
    DOUBLE = 0x4       ///< double
  };

  /// @brief Data set value variant
  using DataSetValue = std::variant<std::monostate, TableRow, std::string, int64_t, double>;

  /// @brief One entry in a data set. Has necessary interface to be work with maps.
  using DataSetEntry = data_set::Entry<DataSetValue>;

  /// @brief A set of data set entries
  class AGENT_LIB_API DataSet : public data_set::Set<DataSetEntry>
  {
  public:
    using base = data_set::Set<DataSetEntry>;
    using base::base;

    /// @brief Split the data set entries by space delimiters and account for the
    /// use of single and double quotes as well as curly braces
    bool parse(const std::string &s, bool table);
  };
}  // namespace mtconnect::entity
