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

#include "entity/json_parser.hpp"

#include <nlohmann/json.hpp>

#include "logging.hpp"

using namespace std;
using json = nlohmann::json;

namespace mtconnect {
  namespace entity {
    static EntityPtr parseJson(FactoryPtr factory, string entity_name, json jNode,
                               ErrorList& errors)
    {
      auto ef = factory->factoryFor(entity_name);

      if (ef)
      {
        Properties properties;
        EntityList* l {nullptr};

        if (ef->isList() && jNode.size() > 0)
        {
          l = &properties["LIST"].emplace<EntityList>();
        }

        for (auto& [key, value] : jNode.items())
        {
          string property_key = key;

          if (key == "value" && !ef->hasRaw())
            property_key = "VALUE";
          else if (key == "value" && ef->hasRaw())
            continue;

          if (value.is_string())
          {
            properties.insert({property_key, string {value}});
          }
          else if (value.is_number())
          {
            if (jNode[key].get<double>() == jNode[key].get<int64_t>())
              properties.insert({property_key, int64_t(value)});
            else
              properties.insert({property_key, double(value)});
          }
          else if (value.is_boolean())
          {
            properties.insert({property_key, bool(value)});
          }
        }

        if (ef->hasRaw())
        {
          properties.insert({"RAW", string {jNode["value"]}});
        }
        else
        {
          if (jNode.is_object())
          {
            for (auto& [key, value] : jNode.items())
            {
              auto ent = parseJson(ef, key, value, errors);
              if (ent)
              {
                if (ef->isPropertySet(ent->getName()))
                {
                  auto res = properties.try_emplace(ent->getName(), EntityList {});
                  get<EntityList>(res.first->second).emplace_back(ent);
                }
                else
                {
                  properties.insert({ent->getName(), ent});
                }
              }
            }
          }
          else if (jNode.is_array() && jNode.size() > 0)
          {
            for (auto const& i : jNode)
            {
              auto it = i.begin();
              auto ent = parseJson(ef, it.key(), it.value(), errors);
              if (ent)
              {
                if (l != nullptr)
                {
                  l->emplace_back(ent);
                }
              }
              else
              {
                LOG(debug) << "Unexpected element: " << it.key();
                errors.emplace_back(
                    new EntityError("Invalid element '" + it.key() + "'", entity_name));
              }
            }
          }
        }
        try
        {
          auto entity = ef->make(entity_name, properties, errors);
          return entity;
        }
        catch (EntityError& e)
        {
          e.setEntity(entity_name);
          errors.emplace_back(e.dup());
        }
      }
      return nullptr;
    }

    EntityPtr JsonParser::parse(FactoryPtr factory, const string& document, const string& version,
                                ErrorList& errors)
    {
      NAMED_SCOPE("entity.json_parser");
      EntityPtr entity;
      auto jsonObj = json::parse(document.c_str());
      auto entity_name = jsonObj.begin().key();

      if (jsonObj.size() == 1)
      {
        entity = parseJson(factory, entity_name, jsonObj[entity_name], errors);
      }
      else
      {
        errors.emplace_back(new EntityError("Cannot Parse Document."));
      }
      return entity;
    }
  }  // namespace entity
}  // namespace mtconnect
