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

#pragma once

#include "entity/entity.hpp"
#include <any>
#include "guard.hpp"

namespace mtconnect
{
  namespace pipeline
  {
    // A transform takes an entity and transforms it to another
    // entity. The transform is an object with the overloaded
    // operation () takes the object type and performs produces an
    // output. The entities are pass as shared pointers.
    //
    // Additiional parameters can be bound if additional context is
    // required.

    class Transform;
    using TransformPtr = std::shared_ptr<Transform>;
    using TransformList = std::list<TransformPtr>;

    class Transform : public std::enable_shared_from_this<Transform>
    {
    public:
      Transform(const Transform &) = default;
      Transform() = default;
      virtual ~Transform() = default;

      virtual const entity::EntityPtr operator()(const entity::EntityPtr entity) = 0;
      TransformPtr getptr() { return shared_from_this(); }
            
      const entity::EntityPtr next(const entity::EntityPtr entity)
      {
        if (m_next.empty())
          return entity;
        
        using namespace std;
        using namespace entity;
        
        for (auto &t : m_next)
        {
          switch (t->check(entity))
          {
          case RUN:
            return (*t)(entity);
            
          case SKIP:
            return t->next(entity);
                            
          case DISPARATE:
            // Move on to the next
            break;
          }
        }
        
        throw EntityError("Cannot find matching transform for " + entity->getName());
        
        return EntityPtr();
      }

      TransformPtr bind(TransformPtr trans)
      {
        m_next.emplace_back(trans);
        return trans;
      }
      
      GuardAction check(const entity::EntityPtr entity)
      {
        if (!m_guard)
          return RUN;
        else
          return m_guard(entity);
      }
      const Guard &getGuard() const { return m_guard; }
      void setGuard(const Guard &guard) { m_guard = guard; }

    protected:
      std::string m_name;
      TransformList m_next;
      Guard        m_guard;
    };
    
    class NullTransform : public Transform
    {
    public:
      NullTransform(Guard guard)
      {
        m_guard = guard;
      }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override
      {
        return entity;
      }
    };

  }  // namespace source
}  // namespace mtconnect
