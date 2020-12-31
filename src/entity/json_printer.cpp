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

#include "xml_printer.cpp"
#include "json_printer.hpp"

using namespace std;
using json = nlohmann::json;

namespace mtconnect
{
  namespace entity
  {
    using Property = pair<string, Value>;

    json JsonPrinter::jprint(const EntityPtr entity)
    {
      json jsonObj;
      list<Property> attributes;
      list<Property> elements;
      auto &properties = entity->getProperties();

      for (auto &e : properties)
      {
        if (holds_alternative<EntityPtr>(e.second))
        {
          jsonObj[entity->getName()][get<EntityPtr>(e.second)->getName()] =
              jprint(get<EntityPtr>(e.second));
        }
        else if (holds_alternative<EntityList>(e.second))
        {
          auto &list = get<EntityList>(e.second);
          jsonObj = json::array();
          for (auto &f : list)
          {
            jsonObj.emplace_back(jprint(f));
          }
        }
        else
        {
          string t;
          jsonObj[entity->getName()][e.first] = toCharPtr(e.second, t);
        }
      }
      return jsonObj;
    }
  }
}