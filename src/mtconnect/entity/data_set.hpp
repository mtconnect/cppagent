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
  namespace data_set {
    /// @brief helper visitor to compare variant values
    /// @tparam T The type of the underlying data
    template <typename T>
    struct SameValueVisitor
    {
      /// @brief Construct the with a cell value to compare
      SameValueVisitor(const T &o) : m_other(o) {}

      /// @brief Compare the types are the same and the values are the same
      /// @tparam T the data type
      /// @param v the other value
      /// @return `true` if they are the same
      template <typename TV>
      bool operator()(const TV &v)
      {
        return std::holds_alternative<TV>(m_other) && std::get<TV>(m_other) == v;
      }

      /// @brief the other value to compare
      const T &m_other;
    };

    /// @brief One entry in a data set. Has necessary interface to be work with maps.
    /// @tparam T The type of the underlying data
    /// @tparam SV The visitor to compare values of the entry
    template <typename T, typename SV = SameValueVisitor<T>>
    struct Entry
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
      bool sameValue(const Entry &other) const
      {
        const auto &ov = other.m_value;
        return std::visit(SV(m_value), ov);
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

  using TableCell = data_set::Entry<TableCellValue>;
  using TableRow = data_set::Set<TableCell>;

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

  /// @brief A visitor that handles table values
  struct DataSetValueSameVisitor
  {
    DataSetValueSameVisitor(const DataSetValue &o) : m_other(o) {}

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
    template <typename TV>
    bool operator()(const TV &v)
    {
      return std::holds_alternative<TV>(m_other) && std::get<TV>(m_other) == v;
    }

    const DataSetValue &m_other;  ///< The other value to compare
  };

  /// @brief One entry in a data set. Has necessary interface to be work with maps.
  using DataSetEntry = data_set::Entry<DataSetValue, DataSetValueSameVisitor>;

  /// @brief A set of data set entries
  class DataSet : public data_set::Set<DataSetEntry>
  {
  public:
    using base = data_set::Set<DataSetEntry>;
    using base::base;
    
    /// @brief Split the data set entries by space delimiters and account for the
    /// use of single and double quotes as well as curly braces
    bool parse(const std::string &s, bool table);
  };
}  // namespace mtconnect::entity
