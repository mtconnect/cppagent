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
  using namespace mtconnect::printer;

  /// @brief Serializes entities as JSON text
  template <typename T>
  class AGENT_LIB_API JsonPrinter
  {
  public:
    /// @brief Create a json printer
    /// @param version the supported MTConnect serialization version
    /// - Version 1 has a repreated objects in arrays for collections of objects
    /// - Version 2 combines arrays of objects by type
    JsonPrinter(T &writer, uint32_t version) : m_version(version), m_writer(writer) {};

    /// @brief create a json object from an entity
    ///
    /// Cover method for `printEntity()`
    ///
    /// @param entity the entity
    /// @return json object
    void print(const EntityPtr entity)
    {
      AutoJsonObject obj(m_writer);
      obj.Key(entity->getName());
      printEntity(entity);
    }
    /// @brief Convert properties of an entity into a json object
    /// @param entity the entity
    /// @return json object
    void printEntity(const EntityPtr entity)
    {
      std::optional<AutoJsonObject<T>> obj;
      if (!entity->isSimpleList())
        obj.emplace(m_writer);

      for (auto &e : entity->getProperties())
      {
        visit(overloaded {[&](const EntityPtr &arg) {
                            m_writer.Key(e.first.c_str());
                            printEntity(arg);
                          },
                          [&](const EntityList &arg) {
                            bool isPropertyList = e.first != "LIST";
                            if (entity->hasListWithAttribute())
                            {
                              obj->Key("list");
                              printEntityList(arg);
                            }
                            else if (isPropertyList)
                            {
                              obj->Key(e.first.str());
                              AutoJsonArray ary(m_writer);
                              for (auto &ei : arg)
                                printEntity(ei);
                            }
                            else
                            {
                              printEntityList(arg);
                            }
                          },
                          [&](const std::monostate &arg) {}, [&](const std::nullptr_t &arg) {},
                          [&](const Vector &v) {
                            printKey(*obj, e.first);
                            AutoJsonArray ary(m_writer);
                            for (auto &d : v)
                              obj->Add(d);
                          },
                          [&](const DataSet &v) {
                            printKey(*obj, e.first);
                            print(v);
                          },
                          [&](const Timestamp &v) {
                            printKey(*obj, e.first);
                            print(v);
                          },
                          [&](const auto &arg) {
                            printKey(*obj, e.first);
                            obj->Add(arg);
                          }},
              e.second);
      }
    }

    /// @brief Helper method to serialize a list entity list using json version 1 format
    /// @param[in] list a list of EntityPtr objects
    /// @tparam T2 Type of iterable collection must contain Entity subclass
    template <typename T2>
    void printEntityList(const T2 &list)
    {
      if (m_version == 1)
        printEntityList1(list);
      else if (m_version == 2)
        printEntityList2(list);
      else
        throw std::runtime_error("Invalid json printer version");
    }

    /// @brief Helper method to serialize a list entity list using json version 1 format
    ///
    /// A simple array is created holding entities with the key being the object name and properties
    /// as an object. The keys are repeated for each entity.
    ///
    /// @param[in] list a list of EntityPtr objects
    /// @tparam T2 Type of iterable collection must contain Entity subclass
    template <typename T2>
    void printEntityList1(const T2 &list)
    {
      AutoJsonArray ary(m_writer);
      for (auto &ei : list)
      {
        AutoJsonObject obj(m_writer);
        obj.Key(ei->getName());
        printEntity(ei);
      }
    }

    /// @brief Helper method to serialize a list entity list using json version 2 format
    ///
    /// Groups entities by their name and uses the names as keys. The properties of the entites are
    /// then serialized in objects contained in a list. The entitiy name is only given once.
    ///
    /// @param[in] list a list of EntityPtr objects
    /// @tparam T2 Type of iterable collection must contain Entity subclass
    template <typename T2>
    void printEntityList2(const T2 &list)
    {
      AutoJsonObject obj(m_writer);
      // Sort the entities by name, use a string view so we don't copy
      std::multimap<std::string_view, EntityPtr> entities;
      for (auto &e : list)
        entities.emplace(std::string_view(e->getName()), e);

      /// Group the entities by name
      for (auto it = entities.begin(); it != entities.end();)
      {
        auto next =
            std::upper_bound(it, entities.end(), it->first,
                             [](const auto &a, const auto &b) { return a.compare(b.first) < 0; });

        obj.Key(it->first);

        AutoJsonArray ary(m_writer);
        for (auto it2 = it; it2 != next; it2++)
        {
          printEntity(it2->second);
        }

        it = next;
      }
    }

  protected:
    void printKey(AutoJsonObject<T> &obj, const PropertyKey &key)
    {
      if (key == "VALUE" || key == "RAW")
        obj.Key("value");
      else
        obj.Key(key);
    }

    void print(const DataSet &set)
    {
      AutoJsonObject obj(m_writer);
      for (auto &e : set)
      {
        if (e.m_removed)
        {
          obj.Key(e.m_key);
          AutoJsonObject rem(m_writer);
          rem.AddPairs("removed", true);
        }
        else
        {
          visit(overloaded {
                    [](const std::monostate &) {},
                    [&obj, &e](const std::string &st) { obj.AddPairs(e.m_key, st); },
                    [&obj, &e](const int64_t &i) { obj.AddPairs(e.m_key, i); },
                    [&obj, &e](const double &d) { obj.AddPairs(e.m_key, d); },
                    [&e, this](const DataSet &arg) {
                      AutoJsonObject<T> row(m_writer, e.m_key);
                      for (auto &c : arg)
                      {
                        visit(overloaded {
                                  [&row, &c](const std::string &st) { row.AddPairs(c.m_key, st); },
                                  [&row, &c](const int64_t &i) { row.AddPairs(c.m_key, i); },
                                  [&row, &c](const double &d) { row.AddPairs(c.m_key, d); },
                                  [](auto &a) {
                                    LOG(error) << "Invalid  variant type for table cell";
                                  }},
                              c.m_value);
                      }
                    }},
                e.m_value);
        }
      }
    }
    void print(const Timestamp &t) { m_writer.String(format(t).c_str()); }

  protected:
    uint32_t m_version;
    T &m_writer;
  };

  /// @brief Serialization wrapper to turn an entity into a json string.
  class AGENT_LIB_API JsonEntityPrinter
  {
  public:
    /// @brief Create a printer for with a JSON vesion and flag to pretty print
    JsonEntityPrinter(uint32_t version, bool pretty = false) : m_version(version), m_pretty(pretty)
    {}

    /// @brief wrapper around the JsonPrinter print method that creates the correct printer
    /// depending on pretty flag
    /// @param[in] entity the entity to print
    /// @returns string representation  of the json
    std::string printEntity(const EntityPtr entity)
    {
      using namespace rapidjson;
      StringBuffer output;
      RenderJson(output, m_pretty, [&](auto &writer) {
        JsonPrinter printer(writer, m_version);
        printer.printEntity(entity);
      });

      return std::string(output.GetString(), output.GetLength());
    }

    /// @brief wrapper around the JsonPrinter print method that creates the correct printer
    /// depending on pretty flag
    ///
    /// This method creates a wrapper object with a Key using the entity's name.
    ///
    /// @param[in] entity the entity to print
    /// @returns string representation  of the json
    std::string print(const EntityPtr entity)
    {
      using namespace rapidjson;
      StringBuffer output;
      RenderJson(output, m_pretty, [&](auto &writer) {
        JsonPrinter printer(writer, m_version);
        printer.print(entity);
      });

      return std::string(output.GetString(), output.GetLength());
    }

  protected:
    uint32_t m_version;
    bool m_pretty;
  };
}  // namespace mtconnect::entity
