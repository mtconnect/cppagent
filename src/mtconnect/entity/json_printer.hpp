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

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"

#include "mtconnect/printer/json_printer_helper.hpp"

namespace mtconnect::entity {
  /// @brief Serializes entities as JSON text
  template<typename T>
  class AGENT_LIB_API JsonPrinter
  {
  public:
    /// @brief Create a json printer
    /// @param version the supported MTConnect serialization version
    /// - Version 1 has a repreated objects in arrays for collections of objects
    /// - Version 2 combines arrays of objects by type
    JsonPrinter(T &writer, uint32_t version) : m_writer(writer), m_version(version) {};

    /// @brief create a json object from an entity
    ///
    /// Cover method for `printEntity()`
    ///
    /// @param entity the entity
    /// @return json object
    void print(const EntityPtr entity)
    {
      m_writer.Key(entity->getName());
      printEntity(entity);
    }
    /// @brief Convert properties of an entity into a json object
    /// @param entity the entity
    /// @return json object
    void printEntity(const EntityPtr entity)
    {
      AutoJsonObject obj(m_writer);

      for (auto &e : entity->getProperties())
      {
        visit(overloaded {[&](const EntityPtr &arg) {
                            m_writer.Key(e.first);
                            printEntity(arg);
                          },
                          [&](const EntityList &arg) {
                            bool isPropertyList = e.first != "LIST";
                            if (entity->hasListWithAttribute())
                              obj.Key("list");
                            else if (isPropertyList)
                              obj.Key(e.first);

                            if (isPropertyList)
                            {
                              AutoJsonArray ary(m_writer);
                              for (auto &ei : arg)
                                printEntity(ei);
                            }
                            else
                            {
                              if (m_version == 1)
                                printEntityList1(arg);
                              else if (m_version == 2)
                                printEntityList2(arg);
                              else
                                throw std::runtime_error("Invalid json printer version");
                            }
                          },
                          [&](const auto &arg) {
                            if (e.first == "VALUE" || e.first == "RAW")
                              obj.AddPairs("value", getValue(arg));
                            else
                              obj.AddPairs(e.first, getValue(arg));
                          }},
              e.second);
      }
    }

  protected:
    void printEntityList1(const EntityList &list)
    {
      AutoJsonArray ary(m_writer);
      for (auto &ei : list)
      {
        AutoJsonObject obj(m_writer, ei->getName());
        printEntity(ei);
      }
    }
    
    void printEntityList2(const EntityList &list)
    {
      for (auto &ei : list)
      {
        auto eo = printEntity(ei);
        auto it = items.find(ei->getName());
        if (it == items.end())
        {
          items.insert_or_assign(ei->getName(), eo);
        }
        else if (!it->second.is_array())
        {
          auto f = json::array();
          it->second.swap(f);
          it->second.emplace_back(f);
          it->second.emplace_back(eo);
        }
        else
        {
          it->second.emplace_back(printEntity(ei));
        }
      }

      for (auto &me : items)
      {
        obj[me.first] = me.second;
      }
    }
    
    void getValue(const Value &value)
    {
      JsonHelper hlp(m_writer);
      visit(overloaded {[](const EntityPtr &) { return nullptr; },
                               [](const std::monostate &) { },
                               [](const EntityList &) { },
                               [](const DataSet &v) { return toJson(v); },
                               [](const Timestamp &v) { return toJson(v); },
                               [](const auto &arg) { return hlp.Add(arg); }},
                   value);
    }
    void toJson(const DataSet &set)
    {
      for (auto &e : set)
      {
        if (e.m_removed)
        {
          value[e.m_key] = json::object({{"removed", true}});
        }
        else
        {
          visit(overloaded {[](const monostate &) {},
                            [&value, &e](const std::string &st) { value[e.m_key] = st; },
                            [&value, &e](const int64_t &i) { value[e.m_key] = i; },
                            [&value, &e](const double &d) { value[e.m_key] = d; },
                            [&value, &e](const DataSet &arg) {
                              auto row = json::object();
                              for (auto &c : arg)
                              {
                                visit(overloaded {
                                          [&row, &c](const std::string &st) { row[c.m_key] = st; },
                                          [&row, &c](const int64_t &i) { row[c.m_key] = i; },
                                          [&row, &c](const double &d) { row[c.m_key] = d; },
                                          [](auto &a) {
                                            LOG(error) << "Invalid  variant type for table cell";
                                          }},
                                      c.m_value);
                              }
                              value[e.m_key] = row;
                            }},
                e.m_value);
        }
      }
    }
    void toJson(const Timestamp &t) { m_writer.String(format(t).c_str()); }

  protected:
    uint32_t m_version;
    T &m_writer;
  };
}  // namespace mtconnect::entity
