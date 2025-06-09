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

  /// @brief One entry in a data set. Has necessary interface to be work with maps.
  struct TableCell
  {
    /// @brief Create an entry with a key and value
    /// @param key the key
    /// @param value the value as a string
    TableCell(std::string key, std::string &value, bool removed = false)
      : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
    {}

    /// @brief Create an entry for a data set
    /// @param key the key
    /// @param value the a data set variant
    TableCell(std::string key, TableCellValue value, bool removed = false)
      : m_key(std::move(key)), m_value(std::move(value)), m_removed(removed)
    {}

    /// @brief Create a data set entry with just a key (used for search)
    /// @param key
    TableCell(std::string key) : m_key(std::move(key)), m_value(std::string("")) {}
    TableCell(const TableCell &other) = default;
    TableCell() {}

    /// @brief copy another table cell
    /// @param other the other cell
    TableCell &operator=(const TableCell &other)
    {
      m_key = other.m_key;
      m_value = other.m_value;
      m_removed = other.m_removed;
      return *this;
    }

    /// @brief only compares keys for equality
    bool operator==(const TableCell &other) const { return m_key == other.m_key; }
    /// @brief only compares keys for less than
    bool operator<(const TableCell &other) const { return m_key < other.m_key; }

    /// @brief helper visitor to compare variant values
    struct SameValue
    {
      /// @brief Construct the with a cell value to compare
      SameValue(const TableCellValue &o) : m_other(o) {}

      /// @brief Compare the types are the same and the values are the same
      /// @tparam T the data type
      /// @param v the other value
      /// @return `true` if they are the same
      template <class T>
      bool operator()(const T &v)
      {
        return std::holds_alternative<T>(m_other) && std::get<T>(m_other) == v;
      }

      /// @brief the other value to compare
      const TableCellValue &m_other;
    };

    /// @brief compares two cell values
    /// @param other the other cell value to compare
    bool sameValue(const TableCell &other) const
    {
      auto &ov = other.m_value;
      return std::visit(SameValue(ov), m_value);
    }

    /// @brief Compares to cells
    /// @param other the other cell
    bool same(const TableCell &other) const
    {
      return m_key == other.m_key && m_removed == other.m_removed && sameValue(other);
    }

    std::string m_key;
    TableCellValue m_value;
    bool m_removed {false};
  };

  /// @brief A set of data set entries
  class AGENT_LIB_API TableRow : public std::set<TableCell>
  {
  public:
    using base = std::set<TableCell>;
    using base::base;

    /// @brief Get a entry for a key
    /// @tparam T the entry type
    /// @param key the key
    /// @return the typed value of the entry
    template <typename T>
    const T &get(const std::string &key) const
    {
      return std::get<T>(find(TableCell(key))->m_value);
    }

    /// @brief Get a entry for a key if it exists
    /// @tparam T the entry type
    /// @param key the key
    /// @return optional typed value of the entry
    template <typename T>
    const std::optional<T> maybeGet(const std::string &key) const
    {
      auto v = find(TableCell(key));
      if (v == end())
        return std::nullopt;
      else
        return std::get<T>(v->m_value);
    }
  };

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
    DataSetEntry(std::string key, TableRow &value, bool removed = false)
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
    DataSetEntry(std::string key)
      : m_key(std::move(key)), m_value(std::string("")), m_removed(false)
    {}
    DataSetEntry(const DataSetEntry &other) = default;
    DataSetEntry() : m_removed(false) {}

    /// @brief copy a data set entry from another
    DataSetEntry &operator=(const DataSetEntry &other)
    {
      m_key = other.m_key;
      m_value = other.m_value;
      m_removed = other.m_removed;
      return *this;
    }

    /// @brief only compares keys for equality
    bool operator==(const DataSetEntry &other) const { return m_key == other.m_key; }
    /// @brief only compares keys for less than
    bool operator<(const DataSetEntry &other) const { return m_key < other.m_key; }

    /// @brief compare a data entry ewith another
    bool same(const DataSetEntry &other) const;

    std::string m_key;
    DataSetValue m_value;
    bool m_removed;
  };

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
    const T &get(const std::string &key) const
    {
      return std::get<T>(find(DataSetEntry(key))->m_value);
    }

    /// @brief Get a entry for a key if it exists
    /// @tparam T the entry type
    /// @param key the key
    /// @return optional typed value of the entry
    template <typename T>
    const std::optional<T> maybeGet(const std::string &key) const
    {
      auto v = find(DataSetEntry(key));
      if (v == end())
        return std::nullopt;
      else
        return std::get<T>(v->m_value);
    }

    /// @brief Split the data set entries by space delimiters and account for the
    /// use of single and double quotes as well as curly braces
    bool parse(const std::string &s, bool table);
  };

  /// @brief Equality visitor for a DataSetValue
  struct DataSetValueSame
  {
    DataSetValueSame(const DataSetValue &other) : m_other(other) {}

    /// @brief Compare two table rows and see if the are the same
    /// @param v The other value
    /// @return `true ` if they are the same
    bool operator()(const TableRow &v)
    {
      if (!std::holds_alternative<TableRow>(m_other))
        return false;

      const auto &oset = std::get<TableRow>(m_other);

      if (v.size() != oset.size())
        return false;

      for (const auto &e1 : v)
      {
        const auto &e2 = oset.find(e1);
        if (e2 == oset.end() || !e2->sameValue(e1))
          return false;
      }

      return true;
    }

    /// @brief Compare the types are the same and the values are the same
    /// @tparam T the data type
    /// @param v the other value
    /// @return `true` if they are the same
    template <class T>
    bool operator()(const T &v)
    {
      return std::holds_alternative<T>(m_other) && std::get<T>(m_other) == v;
    }

    const DataSetValue &m_other;  //! the other data set value
  };

  inline bool DataSetEntry::same(const DataSetEntry &other) const
  {
    return m_key == other.m_key && m_removed == other.m_removed &&
           std::visit(DataSetValueSame(other.m_value), m_value);
  }
}  // namespace mtconnect::entity
