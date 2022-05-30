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

#include "requirement.hpp"

#include <boost/algorithm/string.hpp>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <date/date.h>
#include <string_view>

#include "entity/entity.hpp"
#include "factory.hpp"
#include "logging.hpp"

using namespace std;

namespace mtconnect {
  namespace entity {
    Requirement::Requirement(const std::string &name, ValueType type, FactoryPtr f, bool required)
      : m_name(name), m_upperMultiplicity(1), m_lowerMultiplicity(required ? 1 : 0), m_type(type)
    {
      NAMED_SCOPE("EntityRequirement");

      if (type == ENTITY_LIST)
      {
        f->setList(true);
      }
      m_factory = f;
    }

    Requirement::Requirement(const std::string &name, ValueType type, FactoryPtr f, int lower,
                             int upper)
      : m_name(name), m_upperMultiplicity(upper), m_lowerMultiplicity(lower), m_type(type)
    {
      if (type == ENTITY_LIST)
      {
        f->setList(true);
      }
      m_factory = f;
    }

    bool Requirement::isMetBy(const Value &value, bool isList) const
    {
      // Is this a multiple entry
      if ((m_type == ENTITY || m_type == ENTITY_LIST))
      {
        if (!m_factory)
        {
          throw PropertyError("For entity or list requirement " + m_name + ", no factory", m_name);
        }
        if (holds_alternative<EntityPtr>(value))
        {
          const auto e = get<EntityPtr>(value);
          if (!matches(e->getName()))
          {
            throw PropertyError(
                "Requirement " + m_name + " does not have a matching entity name: " + e->getName(),
                m_name);
          }
        }
        else if (holds_alternative<EntityList>(value))
        {
          const auto l = std::get<EntityList>(value);
          int count = 0;
          for (const auto &e : l)
          {
            if (matches(e->getName()))
              count++;
          }
          if (count > m_upperMultiplicity || count < m_lowerMultiplicity)
          {
            string upper;
            if (m_upperMultiplicity != Infinite)
              upper = " and no more than " + to_string(m_upperMultiplicity);
            throw PropertyError("Entity list requirement " + m_name + " must have at least " +
                                    to_string(m_lowerMultiplicity) + upper + " entries, " +
                                    to_string(l.size()) + " found",
                                m_name);
          }
        }
        else
        {
          throw PropertyError(
              "Entity or list requirement " + m_name + " does not have correct type", m_name);
        }
      }
      else
      {
        if (value.index() != (m_type & 0xF))
        {
          throw PropertyError("Incorrect type for property " + m_name);
        }
        if (std::holds_alternative<std::string>(value))
        {
          auto &v = std::get<std::string>(value);
          if (m_pattern && !std::regex_match(v, *m_pattern))
          {
            throw PropertyError("Invalid value for '" + m_name + "': '" + v + "' is not allowed",
                                m_name);
          }
          else if (m_vocabulary && m_vocabulary->count(v) == 0)
          {
            throw PropertyError("Invalid value for '" + m_name + "': '" + v + "' is not allowed",
                                m_name);
          }
        }
        else if (std::holds_alternative<entity::Vector>(value))
        {
          auto &v = std::get<entity::Vector>(value);
          if (m_size)
          {
            if (v.size() != *m_size)
            {
              throw PropertyError(
                  "Vector size " + to_string(v.size()) + " is not equal to " + to_string(*m_size),
                  m_name);
            }
          }
          else
          {
            if (v.size() > m_upperMultiplicity)
            {
              throw PropertyError("Vector size " + to_string(v.size()) + " is greater than " +
                                      to_string(m_upperMultiplicity),
                                  m_name);
            }
            if (m_lowerMultiplicity != 0 && v.size() < m_lowerMultiplicity)
            {
              throw PropertyError("Vector size " + to_string(v.size()) + " is less than " +
                                      to_string(m_lowerMultiplicity),
                                  m_name);
            }
          }
        }
      }

      return true;
    }

    struct ValueConverter
    {
      ValueConverter(ValueType type, bool table) : m_type(type), m_table(table) {}

      // ------------ Strings ----------------
      void operator()(const string &arg, DataSet &t) { t.parse(arg, m_table); }
      void operator()(const string &arg, int64_t &r)
      {
        char *ep = nullptr;
        const char *sp = arg.c_str();
        r = strtoll(sp, &ep, 10);
        if (ep == sp)
          throw PropertyError("cannot convert string '" + arg + "' to integer");
      }
      void operator()(const string &arg, double &r)
      {
        char *ep = nullptr;
        const char *sp = arg.c_str();
        r = strtod(sp, &ep);
        if (ep == sp)
          throw PropertyError("cannot convert string '" + arg + "' to double");
      }
      void operator()(const string &arg, Timestamp &ts)
      {
        istringstream in(arg);

        // If there isa a time portion in the string, parse the time
        if (arg.find('T') != string::npos)
        {
          in >> std::setw(6) >> date::parse("%FT%T", ts);
        }
        else
        {
          // Just parse the date
          in >> date::parse("%F", ts);
        }
      }
      void operator()(const string &arg, Vector &r)
      {
        if (arg.empty())
          return;

        char *np(nullptr);
        const char *cp = arg.c_str();

        while (cp && *cp != '\0')
        {
          if (isspace(*cp))
          {
            cp++;
          }
          else
          {
            double v = strtod(cp, &np);
            if (cp == np)
            {
              throw PropertyError("cannot convert string '" + arg + "' to vector");
            }
            r.emplace_back(v);
            cp = np;
          }
        }

        if (r.size() == 0)
          throw PropertyError("cannot convert string '" + arg + "' to vector");
      }
      void operator()(const string &arg, bool &r) { r = arg == "true"; }
      void operator()(const string &arg, string &r)
      {
        r = arg;
        if (m_type == USTRING)
          toUpperCase(r);
        else if (m_type == QSTRING)
        {
          auto pos = r.find_first_of(':');
          if (pos != string::npos)
          {
            for (auto c = r.begin() + pos; c != r.end(); c++)
              *c = std::toupper(*c);
          }
          else
          {
            toUpperCase(r);
          }
        }
      }

      // ----------------- double
      void operator()(const double arg, string &v) { v = format(arg); }
      void operator()(const double arg, int64_t &v) { v = arg; }
      void operator()(const double arg, bool &v) { v = arg != 0.0; }
      void operator()(const double arg, Vector &v) { v.emplace_back(arg); }
      void operator()(const double arg, Timestamp &v)
      {
        v = std::chrono::system_clock::from_time_t(arg);
      }
      template <typename T>
      void operator()(const double arg, T &v)
      {
        throw PropertyError("Cannot convert a double to a non-scalar");
      }

      // ------------ int 64
      void operator()(const int64_t arg, string &v) { v = to_string(arg); }
      void operator()(const int64_t arg, bool &v) { v = arg != 0; }
      void operator()(const int64_t arg, double &v) { v = double(arg); }
      void operator()(const int64_t arg, Vector &v) { v.emplace_back(arg); }
      void operator()(const int64_t arg, Timestamp &v)
      {
        v = std::chrono::system_clock::from_time_t(arg);
      }
      template <typename T>
      void operator()(const int64_t arg, T &v)
      {
        throw PropertyError("Cannot convert a int64 to a non-scalar");
      }

      // ----------- Vector
      void operator()(const Vector &arg, string &v)
      {
        if (arg.size() > 0)
        {
          stringstream s;
          for (auto &v : arg)
            s << formatted(v) << ' ';
          v = string_view(s.str().c_str(), s.str().size() - 1);
        }
      }
      template <typename T>
      void operator()(const Vector &arg, T &v)
      {
        throw PropertyError("Cannot convert a Vector to anything other than a string");
      }

      // ------------ Bool
      void operator()(const bool arg, string &v) { v = arg ? "true" : "false"; }
      void operator()(const bool arg, Vector &v) { v.emplace_back(arg); }
      void operator()(const bool arg, int64_t &v) { v = arg; }
      void operator()(const bool arg, double &v) { v = arg; }
      template <typename T>
      void operator()(const bool arg, T &v)
      {
        throw PropertyError("Cannot convert a bool to a non-scalar");
      }

      // ------------ Timestamp
      void operator()(const Timestamp &arg, string &v) { v = format(arg); }
      void operator()(const Timestamp &arg, int64_t &v)
      {
        v = chrono::system_clock::to_time_t(arg);
      }
      void operator()(const Timestamp &arg, double &v) { v = arg.time_since_epoch().count(); }
      void operator()(const Timestamp &arg, Vector &v)
      {
        v.emplace_back(arg.time_since_epoch().count());
      }
      template <typename T>
      void operator()(const Timestamp &arg, T &)
      {
        throw PropertyError("Cannot convert a Timestamp to a non-scalar");
      }

      // -- Catch all
      template <typename U, typename T>
      void operator()(const U &arg, T &t)
      {
        stringstream s;
        s << "Cannot convert from " << typeid(U).name() << " to " << typeid(T).name();
        throw PropertyError(s.str());
      }
      // Default

      ValueType m_type;
      bool m_table;
    };

    bool ConvertValueToType(Value &value, ValueType type, bool table)
    {
      if (value.index() == type)
        return false;

      Value out;
      switch (type)
      {
        case QSTRING:
        case USTRING:
        case STRING:
          out.emplace<STRING>();
          break;

        case INTEGER:
          out.emplace<INTEGER>(0);
          break;

        case DOUBLE:
          out.emplace<DOUBLE>(0.0);
          break;

        case BOOL:
          out.emplace<BOOL>(false);
          break;

        case VECTOR:
          out.emplace<VECTOR>();
          break;

        case DATA_SET:
          out.emplace<DATA_SET>();
          break;

        case TABLE:
          if (value.index() == DATA_SET)
            return false;

          table = true;
          out.emplace<DATA_SET>();
          break;

        case TIMESTAMP:
          out.emplace<TIMESTAMP>();
          break;

        default:
          throw PropertyError("Cannot convert non-scaler types");
      }

      ValueConverter vc(type, table);
      visit(vc, value, out);
      value = out;

      return true;
    }
  }  // namespace entity
}  // namespace mtconnect
