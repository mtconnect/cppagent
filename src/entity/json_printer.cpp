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

#include "json_printer.hpp"

#include <nlohmann/json.hpp>

#include "logging.hpp"

using namespace std;
using json = nlohmann::json;

namespace mtconnect {
  namespace entity {
    inline static json toJson(const DataSet &set)
    {
      json value;
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

      return value;
    }

    inline static json toJson(const Timestamp &t) { return format(t); }

    inline static json getValue(const Value &value)
    {
      return visit(overloaded {[](const EntityPtr &) -> json { return nullptr; },
                               [](const std::monostate &) -> json { return nullptr; },
                               [](const EntityList &) -> json { return nullptr; },
                               [](const DataSet &v) -> json { return toJson(v); },
                               [](const Timestamp &v) -> json { return toJson(v); },
                               [](const auto &arg) -> json { return arg; }},
                   value);
    }

    void JsonPrinter::printEntityList1(json &obj, const EntityList &list) const
    {
      obj = json::array();
      for (auto &ei : list)
      {
        obj.emplace_back(json {{ei->getName(), printEntity(ei)}});
      }
    }

    void JsonPrinter::printEntityList2(json &obj, const EntityList &list) const
    {
      map<string, json> items;
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

    json JsonPrinter::printEntity(const EntityPtr entity) const
    {
      NAMED_SCOPE("entity.json_printer");
      json jsonObj;

      for (auto &e : entity->getProperties())
      {
        visit(overloaded {[&](const EntityPtr &arg) {
                            json j = printEntity(arg);
                            jsonObj[e.first] = j;
                          },
                          [&](const EntityList &arg) {
                            json obj;

                            bool isPropertyList = e.first != "LIST";
                            if (isPropertyList)
                            {
                              obj = json::array();
                              for (auto &ei : arg)
                                obj.emplace_back(printEntity(ei));
                            }
                            else
                            {
                              if (m_version == 1)
                                printEntityList1(obj, arg);
                              else if (m_version == 2)
                                printEntityList2(obj, arg);
                              else
                                throw std::runtime_error("Invalid json printer version");
                            }

                            if (entity->hasListWithAttribute())
                              jsonObj["list"] = obj;
                            else if (isPropertyList)
                              jsonObj[e.first] = obj;
                            else
                              jsonObj = obj;
                          },
                          [&](const auto &arg) {
                            if (e.first == "VALUE" || e.first == "RAW")
                              jsonObj["value"] = getValue(arg);
                            else
                              jsonObj[e.first] = getValue(arg);
                          }},
              e.second);
      }

      // cout << "---------------" << endl << std::setw(2) << jsonObj << endl;
      return jsonObj;
    }
  }  // namespace entity
}  // namespace mtconnect
