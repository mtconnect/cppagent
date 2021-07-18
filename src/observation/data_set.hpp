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

#include <cmath>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "logging.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace observation {
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

      void parse(const std::string &s, bool table);
    };

    using DataSetValue = std::variant<DataSet, std::string, int64_t, double>;

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

    inline DataSetValue dataSetValue(const std::string &value)
    {
      using namespace std;
      constexpr const char *int_reg = "[+-]?[0-9]+";
      constexpr const char *float_reg = "[+-]?[0-9]*\\.[0-9]+([eE][+-]?[0-9]+)?";

      static const regex int_regex(int_reg);
      static const regex float_regex(float_reg);

      if (regex_match(value, float_regex))
        return DataSetValue(stod(value));
      else if (regex_match(value, int_regex))
        return DataSetValue((int64_t)stoll(value));
      else
        return DataSetValue(value);
    }

    // Split the data set entries by space delimiters and account for the
    // use of single and double quotes as well as curly braces
    inline void DataSet::parse(const std::string &s, bool table)
    {
      using namespace std;
      constexpr const char *reg =
          "[ \t]*"                      // Whitespace
          "([^ \t=]+)"                  // Key
          "(=("                         // equals
          "\"([^\\\\\"]+(\\\\\")?)+\""  // Double quotes
          "|"
          "'([^\\\\']+(\\\\')?)+'"  // Single Quotes
          "|"
          "\\{([^\\}\\\\]+(\\\\\\})?)+\\}"  // Curly braces
          "|"
          "[^ \t]+"  // Value
          ")?)?";    // Close
      static const regex tokenizer(reg);

      smatch m;
      string rest(s);

      try
      {
        // Search for key value pairs. Handle quoted text.
        while (regex_search(rest, m, tokenizer))
        {
          string key, value;
          bool removed = false;
          if (!m[3].matched)
          {
            key = m[1];
            if (!m[2].matched)
              removed = true;
            else
              value.clear();
          }
          else
          {
            key = m[1];
            string v = m[3];

            // Check for invalid termination of string
            if ((v.front() == '"' && v.back() != '"') || (v.front() == '\'' && v.back() != '\'') ||
                (v.front() == '{' && v.back() != '}'))
            {
              // consider the rest of the set invalid, issue warning.
              break;
            }

            if (v.front() == '"' || v.front() == '\'' || v.front() == '{')
              value = v.substr(1, v.size() - 2);
            else
              value = v;

            // character remove escape

            size_t pos = 0;
            do
            {
              pos = value.find('\\', pos);
              if (pos != string::npos)
              {
                value.erase(pos, 1);
                pos++;
              }
            } while (pos != string::npos && pos < value.size());
          }

          // Map the value.
          if (table)
          {
            DataSet set;
            set.parse(value, false);
            emplace(key, set, removed);
          }
          else
          {
            emplace(key, dataSetValue(value), removed);
          }

          // Parse the rest of the string...
          rest = m.suffix();
        }
      }

      catch (regex_error &e)
      {
        LOG(warning) << "Error parsing \"" << rest << "\", \nReason: " << e.what();
      }

      // If there is leftover text, the text was invalid.
      // Warn that it is being discarded
      if (!rest.empty())
      {
        LOG(warning) << "Cannot parse complete string, malformed data set: '" << rest << "'";
      }
    }
  }  // namespace observation
}  // namespace mtconnect
