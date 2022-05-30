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

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>

#include "entity/entity.hpp"
#include "guard.hpp"
#include "pipeline_context.hpp"

namespace mtconnect {
  namespace device_model::data_item {
    class DataItem;
  }  // namespace device_model::data_item
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;
  namespace pipeline {
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

    using ApplyDataItem = std::function<void(const DataItemPtr di)>;
    using EachDataItem = std::function<void(ApplyDataItem)>;
    using FindDataItem = std::function<DataItemPtr(const std::string &, const std::string &)>;

    class Transform : public std::enable_shared_from_this<Transform>
    {
    public:
      Transform(const Transform &) = default;
      Transform(const std::string &name) : m_name(name) {}
      virtual ~Transform() = default;

      auto &getName() const { return m_name; }

      virtual void stop()
      {
        for (auto &t : m_next)
          t->stop();
      }

      virtual void start(boost::asio::io_context::strand &st)
      {
        for (auto &t : m_next)
          t->start(st);
      }

      virtual void clear()
      {
        for (auto &t : m_next)
          t->clear();
        unlink();
      }

      virtual void unlink() { m_next.clear(); }

      virtual const entity::EntityPtr operator()(const entity::EntityPtr entity) = 0;
      TransformPtr getptr() { return shared_from_this(); }

      TransformList &getNext() { return m_next; }

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

            case CONTINUE:
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

      using TransformPair = std::pair<TransformPtr, TransformPtr>;
      using ListOfTransforms = std::list<TransformPair>;

      void findRec(const std::string &target, ListOfTransforms &xforms)
      {
        for (auto &t : m_next)
        {
          if (t->getName() == target)
          {
            xforms.push_back(TransformPair {getptr(), t});
          }
          t->findRec(target, xforms);
        }
      }

      void find(const std::string &target, ListOfTransforms &xforms)
      {
        if (m_name == target)
        {
          xforms.push_back(TransformPair {nullptr, getptr()});
        }

        findRec(target, xforms);
      }

      void spliceBefore(TransformPtr old, TransformPtr xform)
      {
        for (auto it = m_next.begin(); it != m_next.end(); it++)
        {
          if (it->get() == old.get())
          {
            xform->bind(old);
            *it = xform;
            return;
          }
        }
      }
      void spliceAfter(TransformPtr xform)
      {
        for (auto it = m_next.begin(); it != m_next.end(); it++)
        {
          xform->bind(*it);
        }
        m_next.clear();
        bind(xform);
        return;
      }
      void firstAfter(TransformPtr xform) { m_next.emplace_front(xform); }
      void replace(TransformPtr old, TransformPtr xform)
      {
        for (auto it = m_next.begin(); it != m_next.end(); it++)
        {
          if (it->get() == old.get())
          {
            *it = xform;
            for (auto nxt = old->m_next.begin(); it != old->m_next.end(); it++)
            {
              xform->bind(*nxt);
            }
          }
        }
      }

      void remove(TransformPtr old)
      {
        for (auto it = m_next.begin(); it != m_next.end(); it++)
        {
          if (it->get() == old.get())
          {
            m_next.erase(it);
            for (auto nxt = old->m_next.begin(); it != old->m_next.end(); it++)
            {
              bind(*nxt);
            }
            break;
          }
        }
      }

    protected:
      std::string m_name;
      TransformList m_next;
      Guard m_guard;
    };

    class NullTransform : public Transform
    {
    public:
      NullTransform(Guard guard) : Transform("NullTransform") { m_guard = guard; }
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override { return entity; }
    };

  }  // namespace pipeline
}  // namespace mtconnect
