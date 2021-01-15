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

#include <nlohmann/json.hpp>
#include <dlib/logger.h>

#include "json_printer.hpp"

using namespace std;
using json = nlohmann::json;

namespace mtconnect
{
  namespace entity
  {
    using Property = pair<string, Value>;

    json JsonPrinter::print(const EntityPtr entity) const
    {
      json jsonObj;
      
      auto &properties = entity->getProperties();

      for (auto &e : properties)
      {
        visit(overloaded{
            [&](const EntityPtr &arg) {
                json j = print(arg);
                if (!j.is_array())
                  j = j[arg->getName()];
                jsonObj[entity->getName()][arg->getName()] = j;
              },
            [&](const EntityList &arg) {
                auto &list = arg;
                jsonObj = json::array();
                for (auto &f : list)
                {
                    jsonObj.emplace_back(print(f));
                }
            },
            [&](const auto arg){
                jsonObj[entity->getName()][e.first]  = GetValue(arg);
            }},
              e.second);
      }
      return jsonObj;
    }
    json JsonPrinter::GetValue(const Value &value) const
    {
    
    return visit(overloaded{
        [&](const string &arg)-> json {return arg;},
        [&](const int64_t &arg)-> json {return arg;},
        [&](const double &arg)-> json {return arg;},
        [&](const Vector &arg)-> json {return arg;},
        [&](const auto &arg)-> json {
          cout << "Unrecognized Type";
          return "";
        },},
      value);
    }
  }
}