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

#include "json_printer.hpp"

#include <dlib/logger.h>

#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

namespace mtconnect
{
  namespace entity
  {
    static dlib::logger g_logger("entity.xml.printer");

    inline static json toJson(const observation::DataSet &set)
    {
      using namespace observation;
      json value;
      for (auto &e : set)
      {
        if (e.m_removed)
        {
          value[e.m_key] = json::object({{"removed", true}});
        }
        else
        {
          visit(overloaded {[&value, &e](const std::string &st) { value[e.m_key] = st; },
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
                                            g_logger << dlib::LERROR
                                                     << "Invalid  variant type for table cell";
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
                               [](const observation::DataSet &v) -> json { return toJson(v); },
                               [](const Timestamp &v) -> json { return toJson(v); },
                               [](const auto &arg) -> json { return arg; }},
                   value);
    }

    json JsonPrinter::printEntity(const EntityPtr entity) const
    {
      json jsonObj;

      for (auto &e : entity->getProperties())
      {
        visit(overloaded {[&](const EntityPtr &arg) {
                            json j = printEntity(arg);
                            jsonObj[e.first] = j;
                          },
                          [&](const EntityList &arg) {
                            auto array = json::array();

                            bool isPropertyList = e.first != "LIST";
                            for (auto &ei : arg)
                            {
                              json obj;
                              if (isPropertyList)
                                obj = printEntity(ei);
                              else
                                obj = json::object({{ei->getName(), printEntity(ei)}});
                              array.emplace_back(obj);
                            }

                            if (entity->hasListWithAttribute())
                              jsonObj["list"] = array;
                            else if (isPropertyList)
                              jsonObj[e.first] = array;
                            else
                              jsonObj = array;
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
