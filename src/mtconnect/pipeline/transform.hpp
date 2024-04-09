//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "guard.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
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

    /// @brief Abstract entity transformation
    class AGENT_LIB_API Transform : public std::enable_shared_from_this<Transform>
    {
    public:
      Transform(const Transform &) = default;
      /// @brief Construct a transform with a name
      /// @param[in] name transform name
      Transform(const std::string &name) : m_name(name) {}
      virtual ~Transform() = default;

      /// @brief Get the transform name
      /// @return the name
      auto &getName() const { return m_name; }

      /// @brief stop this transform and all the following transforms
      virtual void stop()
      {
        for (auto &t : m_next)
          t->stop();
      }

      /// @brief start the transform on a strand and all the following transforms
      /// @param st the strand
      virtual void start(boost::asio::io_context::strand &st)
      {
        for (auto &t : m_next)
          t->start(st);
      }

      /// @brief remove all the next transforms
      virtual void clear()
      {
        for (auto &t : m_next)
          t->clear();
        unlink();
      }

      /// @brief clear the list of next transforms
      virtual void unlink() { m_next.clear(); }

      /// @brief the transform method must be overloaded
      /// @param entity the entity
      /// @return the resulting entity
      virtual entity::EntityPtr operator()(entity::EntityPtr &&entity) = 0;
      TransformPtr getptr() { return shared_from_this(); }

      /// @brief get the list of next transforms
      /// @return the list of following transforms
      TransformList &getNext() { return m_next; }

      /// @brief Find the next transform to forward the entity on to
      /// @param entity the entity
      /// @return return the result of the transformation
      entity::EntityPtr next(entity::EntityPtr &&entity)
      {
        if (m_next.empty())
          return entity;

        using namespace std;
        using namespace entity;

        for (auto &t : m_next)
        {
          switch (t->check(entity.get()))
          {
            case RUN:
              return (*t)(std::move(entity));

            case SKIP:
              return t->next(std::move(entity));

            case CONTINUE:
              // Move on to the next
              break;
          }
        }

        throw EntityError("Cannot find matching transform for " + entity->getName());

        return EntityPtr();
      }

      /// @brief Add the transform to the end of the transform list
      /// @param[in] trans the transform
      /// @return trans
      TransformPtr bind(TransformPtr trans)
      {
        m_next.emplace_back(trans);
        return trans;
      }

      /// @brief get the guard action for an entity
      /// @param[in] entity the entity
      /// @return the action to perform
      GuardAction check(const entity::Entity *entity)
      {
        if (!m_guard)
          return RUN;
        else
          return m_guard(entity);
      }

      /// @brief Get a reference to the guard
      /// @return the guard
      const Guard &getGuard() const { return m_guard; }
      /// @brief set the guard
      /// @param guard a guard
      void setGuard(const Guard &guard) { m_guard = guard; }

      using TransformPair = std::pair<TransformPtr, TransformPtr>;
      using ListOfTransforms = std::list<TransformPair>;

      /// @brief recursive step to find all transforms with a given name
      /// @param[in] target the target transform name
      /// @param[out] xforms the list of transform pairs
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

      /// @brief find all transforms with a given name
      /// @param[in] target the target transform name
      /// @param[out] xforms the transform pairs
      void find(const std::string &target, ListOfTransforms &xforms)
      {
        if (m_name == target)
        {
          xforms.push_back(TransformPair {nullptr, getptr()});
        }

        findRec(target, xforms);
      }

      /// @brief splice a transform before another transform
      /// @param old the transform to splice before
      /// @param xform the transform to bind to old
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

      /// @brief Splice after all occurrences of this transform
      ///
      /// binds xform to all the next transforms and then binds this transform to xform.
      /// @param xform the transform to splice after
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
      /// @brief Binds to the first position in the next list
      /// @param xform the transform
      void firstAfter(TransformPtr xform) { m_next.emplace_front(xform); }
      /// @brief Replace one transform with another
      ///
      /// Rebinds the new transform replacing the old transform
      /// @param old the transform to be replaced
      /// @param xform the new transform
      void replace(TransformPtr old, TransformPtr xform)
      {
        for (auto it = m_next.begin(); it != m_next.end(); it++)
        {
          if (it->get() == old.get())
          {
            *it = xform;
            for (auto nxt = old->m_next.begin(); nxt != old->m_next.end(); nxt++)
            {
              xform->bind(*nxt);
            }
          }
        }
      }

      /// @brief remove a transform from the list of next
      ///
      /// Connects the this transform to the old transforms next transforms
      /// @param[in] the transform to remove
      void remove(TransformPtr old)
      {
        for (auto it = m_next.begin(); it != m_next.end(); it++)
        {
          if (it->get() == old.get())
          {
            m_next.erase(it);
            for (auto nxt = old->m_next.begin(); nxt != old->m_next.end(); nxt++)
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

    /// @brief A transform that just returns the entity. It does not call next.
    class AGENT_LIB_API NullTransform : public Transform
    {
    public:
      NullTransform(Guard guard) : Transform("NullTransform") { m_guard = guard; }
      entity::EntityPtr operator()(entity::EntityPtr &&entity) override { return entity; }
    };

    /// @brief A transform that forwards an enetity baed on a guard. Used to merge streams..
    class AGENT_LIB_API MergeTransform : public Transform
    {
    public:
      MergeTransform(Guard guard) : Transform("MergeTransform") { m_guard = guard; }
      entity::EntityPtr operator()(entity::EntityPtr &&entity) override
      {
        return next(std::move(entity));
      }
    };

  }  // namespace pipeline
}  // namespace mtconnect
