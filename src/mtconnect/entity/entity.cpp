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

#include <boost/algorithm/string.hpp>

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

  void Entity::hash(boost::uuids::detail::sha1 &sha1,
                    const boost::unordered_set<string> &skip) const
  {
    sha1.process_bytes(m_name.c_str(), m_name.size());

    for (const auto &e : m_properties)
    {
      // Skip hash
      if (!skip.contains(e.first) && !isHidden(e.first))
      {
        const auto &value = e.second;
        sha1.process_bytes(e.first.c_str(), e.first.size());
        HashVisitor visitor(sha1);
        visit(visitor, value);
      }
    }
  }

  struct UniqueIdVisitor
  {
    std::unordered_map<string, string> &m_idMap;
    const boost::uuids::detail::sha1 &m_sha1;
    UniqueIdVisitor(std::unordered_map<string, string> &idMap,
                    const boost::uuids::detail::sha1 &sha1)
      : m_idMap(idMap), m_sha1(sha1)
    {}

    void operator()(EntityPtr &p) { p->createUniqueId(m_idMap, m_sha1); }

    void operator()(EntityList &l)
    {
      for (auto &e : l)
        e->createUniqueId(m_idMap, m_sha1);
    }

    template <typename T>
    void operator()(const T &)
    {}
  };

  std::optional<std::string> Entity::createUniqueId(
      std::unordered_map<std::string, std::string> &idMap, const boost::uuids::detail::sha1 &sha1)
  {
    optional<string> res;

    auto it = m_properties.find("id");
    if (it != m_properties.end())
    {
      std::string newId, oldId;
      auto origId = maybeGet<std::string>("originalId");
      if (!origId)
      {
        oldId = std::get<std::string>(it->second);
        m_properties.emplace("originalId", oldId);
        newId = makeUniqueId(sha1, oldId);
        it->second = newId;
      }
      else
      {
        oldId = *origId;
        newId = std::get<std::string>(it->second);
      }
      idMap.emplace(oldId, newId);
      res.emplace(newId);
    }

    UniqueIdVisitor visitor(idMap, sha1);

    // Recurse properties
    for (auto &p : m_properties)
    {
      std::visit(visitor, p.second);
    }

    return res;
  }

  struct ReferenceIdVisitor
  {
    const std::unordered_map<string, string> &m_idMap;
    ReferenceIdVisitor(const std::unordered_map<string, string> &idMap) : m_idMap(idMap) {}

    void operator()(EntityPtr &p) { p->updateReferences(m_idMap); }

    void operator()(EntityList &l)
    {
      for (auto &e : l)
        e->updateReferences(m_idMap);
    }

    template <typename T>
    void operator()(T &)
    {}
  };

  void Entity::updateReferences(std::unordered_map<std::string, std::string> idMap)
  {
    using namespace boost::algorithm;
    for (auto &prop : m_properties)
    {
      if (prop.first != "originalId" && (iends_with(prop.first, "idref") ||
                                         (prop.first.length() > 2 && iends_with(prop.first, "id"))))
      {
        auto it = idMap.find(std::get<string>(prop.second));
        if (it != idMap.end())
        {
          prop.second = it->second;
        }
      }
    }

    ReferenceIdVisitor visitor(idMap);

    // Recurse all
    for (auto &p : m_properties)
    {
      std::visit(visitor, p.second);
    }
  }
}  // namespace mtconnect::entity
