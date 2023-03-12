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

#include <unordered_set>

#include "factory.hpp"

using namespace std;

namespace mtconnect::entity {
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

  bool Entity::removeFromList(const std::string &name, EntityPtr entity)
  {
    auto &v = getProperty_(name);
    auto *p = std::get_if<EntityPtr>(&v);
    if (p)
    {
      auto &lv = (*p)->getProperty_("LIST");
      auto *l = std::get_if<EntityList>(&lv);
      if (l)
      {
        auto it = std::find(l->begin(), l->end(), entity);
        if (it != l->end())
        {
          l->erase(it);
          return true;
        }
      }
    }

    return false;
  }

  inline static void hash(boost::uuids::detail::sha1 &sha1, const DataSet &set);

  struct HashVisitor
  {
    HashVisitor(boost::uuids::detail::sha1 &sha1) : m_sha1(sha1) {}

    void operator()(const EntityPtr &arg) { arg->hash(m_sha1); }
    void operator()(const EntityList &arg)
    {
      for (const auto &e : arg)
        e->hash(m_sha1);
    }
    void operator()(const std::monostate &arg) { m_sha1.process_bytes("NIL", 4); }
    void operator()(const Vector &arg)
    {
      for (const auto &e : arg)
        m_sha1.process_bytes(&e, sizeof(e));
    }
    void operator()(const std::string &arg) { m_sha1.process_bytes(arg.c_str(), arg.size()); }
    void operator()(const double arg) { m_sha1.process_bytes(&arg, sizeof(arg)); }
    void operator()(const int64_t arg) { m_sha1.process_bytes(&arg, sizeof(arg)); }
    void operator()(const bool arg) { m_sha1.process_bytes(&arg, sizeof(arg)); }
    void operator()(const Timestamp &arg)
    {
      auto c = arg.time_since_epoch().count();
      m_sha1.process_bytes(&c, sizeof(c));
    }
    void operator()(const DataSet &arg) { hash(m_sha1, arg); }
    void operator()(const std::nullptr_t &arg) { m_sha1.process_bytes("NULL", 4); }

    boost::uuids::detail::sha1 &m_sha1;
  };

  inline static void hash(boost::uuids::detail::sha1 &sha1, const DataSet &set)
  {
    for (auto &e : set)
    {
      sha1.process_bytes(e.m_key.c_str(), e.m_key.size());
      if (e.m_removed)
      {
        sha1.process_bytes("REMOVED", 7);
      }
      else
      {
        HashVisitor visitor(sha1);
        visit(visitor, e.m_value);
      }
    }
  }

  void Entity::hash(boost::uuids::detail::sha1 &sha1) const
  {
    sha1.process_bytes(m_name.c_str(), m_name.size());

    for (const auto &e : m_properties)
    {
      // Skip hash
      if (e.first != "hash")
      {
        const auto &value = e.second;
        sha1.process_bytes(e.first.c_str(), e.first.size());
        HashVisitor visitor(sha1);
        visit(visitor, value);
      }
    }
  }

}  // namespace mtconnect::entity
