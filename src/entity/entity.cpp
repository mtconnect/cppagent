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

#include "factory.hpp"
#include <unordered_set>

#include <dlib/logger.h>

using namespace std;

namespace mtconnect
{
  namespace entity
  {
    bool Entity::addToList(const std::string &name, FactoryPtr factory, EntityPtr entity,
                           ErrorList &errors)
    {
      if (!hasProperty(name))
      {
        entity::EntityList list {entity};
        auto entities = factory->create(name, list, errors);
        if (errors.empty())
          setProperty(name, entities);
        else
          return false;
      }
      else
      {
        auto *entities = std::get_if<EntityPtr>(&getProperty_(name));
        if (entities)
        {
          std::get<EntityList>((*entities)->getProperty_("LIST")).emplace_back(entity);
        }
        else
        {
          errors.emplace_back(new EntityError("Cannont find list for: " + name));
          return false;
        }
      }

      return true;
    }
  }  // namespace entity
}  // namespace mtconnect
