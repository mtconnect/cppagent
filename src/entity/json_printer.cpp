//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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
    using Property = pair<string, Value>;

    json JsonPrinter::print(const EntityPtr entity) const
    {
      return json::object({{ entity->getName(), printEntity(entity) }});
    }
    
    json JsonPrinter::printEntity(const EntityPtr entity) const
    {
      json jsonObj;
      
      for (auto &e : entity->getProperties())
      {
        visit(overloaded{
                  [&](const EntityPtr &arg) {
                    json j = printEntity(arg);
                    jsonObj[e.first] = j;
                  },
                  [&](const EntityList &arg) {
                    auto array = json::array();
                    for (auto &f : arg)
                    {
                      auto obj = json::object({{ f->getName(), printEntity(f) }});
                      array.emplace_back(obj);
                    }
                    
                    if (entity->hasListWithAttribute())
                      jsonObj["list"] = array;
                    else if (e.first == "LIST")
                      jsonObj = array;
                    else
                      jsonObj[e.first] = array;
                  },
                  [&](const auto arg) {
                    if (e.first == "VALUE" || e.first == "RAW")
                      jsonObj["value"] = GetValue(arg);
                    else
                      jsonObj[e.first] = GetValue(arg);
                  }},
              e.second);
      }
      
      //cout << "---------------" << endl << std::setw(2) << jsonObj << endl;
      return jsonObj;
    }
    json JsonPrinter::GetValue(const Value &value) const
    {
      return visit(overloaded{
                       [&](const string &arg) -> json { return arg; },
                       [&](const int64_t &arg) -> json { return arg; },
                       [&](const double &arg) -> json { return arg; },
                       [&](const Vector &arg) -> json { return arg; },
                       [&](const auto &arg) -> json {
                         cout << "Unrecognized Type";
                         return "";
                       },
                   },
                   value);
    }
  }  // namespace entity
}  // namespace mtconnect
