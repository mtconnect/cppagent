//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology ("AMT")
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

#include "mtconnect/entity/json_parser.hpp"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include "mtconnect/logging.hpp"

using namespace std;
namespace rj = ::rapidjson;

namespace mtconnect {
  namespace entity {
    static EntityPtr parseJson(FactoryPtr factory, string entity_name, const rj::Value& jNode,
                               ErrorList& errors)
    {
      auto ef = factory->factoryFor(entity_name);

      if (ef)
      {
        Properties properties;
        EntityList* l {nullptr};

        auto nodeSize = jNode.IsArray() ? jNode.Size() : jNode.IsObject() ? jNode.MemberCount() : 0;
        if (ef->isList() && nodeSize > 0)
        {
          l = &properties["LIST"].emplace<EntityList>();
        }

        if (jNode.IsObject())
        {
          for (auto it = jNode.MemberBegin(); it != jNode.MemberEnd(); ++it)
          {
            string property_key = it->name.GetString();
            const auto& value = it->value;

            if (property_key == "value" && !ef->hasRaw())
              property_key = "VALUE";
            else if (property_key == "value" && ef->hasRaw())
              continue;

            if (value.IsString())
            {
              properties.insert({property_key, string(value.GetString(), value.GetStringLength())});
            }
            else if (value.IsNumber())
            {
              if (value.IsInt64())
                properties.insert({property_key, value.GetInt64()});
              else if (value.IsUint64())
                properties.insert({property_key, static_cast<int64_t>(value.GetUint64())});
              else
                properties.insert({property_key, value.GetDouble()});
            }
            else if (value.IsBool())
            {
              properties.insert({property_key, value.GetBool()});
            }
          }

          if (ef->hasRaw())
          {
            auto rawIt = jNode.FindMember("value");
            if (rawIt != jNode.MemberEnd() && rawIt->value.IsString())
            {
              properties.insert(
                  {"RAW", string(rawIt->value.GetString(), rawIt->value.GetStringLength())});
            }
          }
          else
          {
            for (auto it = jNode.MemberBegin(); it != jNode.MemberEnd(); ++it)
            {
              string key(it->name.GetString(), it->name.GetStringLength());
              const auto& value = it->value;

              if (value.IsObject() || value.IsArray())
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
          }
        }
        else if (jNode.IsArray() && jNode.Size() > 0)
        {
          for (rj::SizeType i = 0; i < jNode.Size(); ++i)
          {
            const auto& item = jNode[i];
            if (item.IsObject() && item.MemberCount() > 0)
            {
              auto it = item.MemberBegin();
              string key(it->name.GetString(), it->name.GetStringLength());
              auto ent = parseJson(ef, key, it->value, errors);
              if (ent)
              {
                if (l != nullptr)
                {
                  l->emplace_back(ent);
                }
              }
              else
              {
                LOG(debug) << "Unexpected element: " << key;
                errors.emplace_back(new EntityError("Invalid element '" + key + "'", entity_name));
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
      rj::Document jsonDoc;
      jsonDoc.Parse(document.c_str());

      if (jsonDoc.HasParseError())
      {
        LOG(error) << "JSON parse error: " << rj::GetParseError_En(jsonDoc.GetParseError())
                   << " at offset " << jsonDoc.GetErrorOffset();
        errors.emplace_back(new EntityError("Cannot Parse Document."));
        return nullptr;
      }

      if (!jsonDoc.IsObject() || jsonDoc.MemberCount() != 1)
      {
        errors.emplace_back(new EntityError("Cannot Parse Document."));
        return entity;
      }

      auto it = jsonDoc.MemberBegin();
      string entity_name(it->name.GetString(), it->name.GetStringLength());
      entity = parseJson(factory, entity_name, it->value, errors);

      return entity;
    }
  }  // namespace entity
}  // namespace mtconnect
