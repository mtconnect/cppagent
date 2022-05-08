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

#include <unordered_map>

#include "data_set.hpp"
#include "qname.hpp"
#include "requirement.hpp"

namespace mtconnect {
  namespace entity {
    struct PropertyKey : public QName
    {
      using QName::QName;
      PropertyKey(const PropertyKey &s) : QName(s) {}
      PropertyKey(const std::string &s) : QName(s) {}
      PropertyKey(const std::string &&s) : QName(s) {}
      PropertyKey(const char *s) : QName(s) {}

      void clearMark() const { const_cast<PropertyKey *>(this)->m_mark = false; }
      void setMark() const { const_cast<PropertyKey *>(this)->m_mark = true; }
      bool m_mark {false};
    };

    using Properties = std::map<PropertyKey, Value>;
    using OrderList = std::list<std::string>;
    using OrderMap = std::unordered_map<std::string, int>;
    using OrderMapPtr = std::shared_ptr<OrderMap>;
    using Property = std::pair<PropertyKey, Value>;

    template <typename T>
    inline std::optional<T> OptionallyGet(const std::string &key, const Properties &props)
    {
      auto p = props.find(key);
      if (p != props.end())
        return std::get<T>(p->second);
      else
        return std::nullopt;
    }

    class Entity : public std::enable_shared_from_this<Entity>
    {
    public:
      using super = std::nullptr_t;

      Entity() {}
      Entity(const std::string &name) : m_name(name) {}
      Entity(const std::string &name, const Properties &props) : m_name(name), m_properties(props)
      {}
      Entity(const Entity &entity) = default;
      virtual ~Entity() {}

      EntityPtr getptr() const { return const_cast<Entity *>(this)->shared_from_this(); }

      virtual const entity::Value &getIdentity() const { return getProperty("id"); }

      bool hasListWithAttribute() const
      {
        return (m_properties.count("LIST") > 0 && m_properties.size() > 1);
      }
      const auto &getName() const { return m_name; }
      const Properties &getProperties() const { return m_properties; }
      const Value &getProperty(const std::string &n) const
      {
        static Value noValue {std::monostate()};
        auto it = m_properties.find(n);
        if (it == m_properties.end())
          return noValue;
        else
          return it->second;
      }
      virtual void setProperty(const std::string &key, const Value &v)
      {
        m_properties.insert_or_assign(key, v);
      }
      void setProperty(const Property &property) { setProperty(property.first, property.second); }
      bool hasProperty(const std::string &n) const
      {
        return m_properties.find(n) != m_properties.end();
      }
      bool hasValue() const { return hasProperty("VALUE"); }
      void setName(const std::string &name) { m_name = name; }
      void setQName(const std::string &name) { m_name.setQName(name); }
      void applyTo(const std::string &name, std::function<void(Value &v)> f)
      {
        auto p = m_properties.find(name);
        if (p != m_properties.end())
          f(p->second);
      }
      void applyToValue(std::function<void(Value &v)> f) { applyTo("VALUE", f); }

      Value &getValue()
      {
        static Value null;
        auto p = m_properties.find("VALUE");
        if (p != m_properties.end())
          return p->second;
        else
          return null;
      }
      const Value &getValue() const { return getProperty("VALUE"); }
      std::optional<EntityList> getList(const std::string &name) const
      {
        auto &v = getProperty(name);
        auto *p = std::get_if<EntityPtr>(&v);
        if (p)
        {
          auto &lv = (*p)->getProperty("LIST");
          auto *l = std::get_if<EntityList>(&lv);
          if (l)
            return *l;
        }

        return std::nullopt;
      }

      bool addToList(const std::string &name, FactoryPtr factory, EntityPtr entity,
                     ErrorList &errors);

      void setValue(const Value &v) { setProperty("VALUE", v); }
      void erase(const std::string &name) { m_properties.erase(name); }

      template <typename T>
      const T &get(const std::string &name) const
      {
        return std::get<T>(getProperty(name));
      }

      template <typename T>
      const T &getValue() const
      {
        return std::get<T>(getValue());
      }

      template <typename T>
      const std::optional<T> maybeGet(const std::string &name) const
      {
        return OptionallyGet<T>(name, m_properties);
      }
      template <typename T>
      const std::optional<T> maybeGetValue() const
      {
        return OptionallyGet<T>("VALUE", m_properties);
      }

      void setOrder(const OrderMapPtr order) { m_order = order; }
      const OrderMapPtr getOrder() const { return m_order; }
      auto find(const std::string &name) { return m_properties.find(name); }
      auto erase(Properties::iterator &it) { return m_properties.erase(it); }

      bool operator==(const Entity &other) const;
      bool operator!=(const Entity &other) const { return !(*this == other); }

      bool updateTo(const EntityPtr other);

      // Entity Factory
    protected:
      Value &getProperty_(const std::string &name)
      {
        static Value noValue {std::monostate()};
        auto it = m_properties.find(name);
        if (it == m_properties.end())
          return noValue;
        else
          return it->second;
      }

    protected:
      QName m_name;
      Properties m_properties;
      OrderMapPtr m_order;
    };

    struct ValueEqualVisitor
    {
      ValueEqualVisitor(const Value &t) : m_this(t) {}

      bool operator()(const EntityPtr &other)
      {
        return *std::get<EntityPtr>(m_this) == *(other.get());
      }

      bool operator()(const EntityList &other)
      {
        const auto &list = std::get<EntityList>(m_this);
        if (list.size() != other.size())
          return false;
        for (auto it1 = list.cbegin(), it2 = other.cbegin(); it1 != list.cend(); it1++, it2++)
        {
          if (*(it1->get()) != *(it2->get()))
            return false;
        }

        return true;
      }

      template <class T>
      bool operator()(const T &other)
      {
        return std::get<T>(m_this) == other;
      }

    private:
      const Value &m_this;
    };

    inline bool operator==(const Value &v1, const Value &v2)
    {
      if (v1.index() != v2.index())
        return false;

      return std::visit(ValueEqualVisitor(v1), v2);
    }

    inline bool operator!=(const Value &v1, const Value &v2) { return !(v1 == v2); }

    inline bool Entity::operator==(const Entity &other) const
    {
      if (m_name != other.m_name)
        return false;

      if (m_properties.size() != other.m_properties.size())
        return false;

      for (auto it1 = m_properties.cbegin(), it2 = other.m_properties.cbegin();
           it1 != m_properties.cend(); it1++, it2++)
      {
        if (it1->first != it2->first || it1->second != it2->second)
        {
          return false;
        }
      }
      return true;
    }

    struct ValueMergeVisitor
    {
      ValueMergeVisitor(Value &t) : m_this(t) {}

      bool operator()(const EntityPtr &other) { return std::get<EntityPtr>(m_this)->updateTo(other); }

      bool operator()(const EntityList &other)
      {
        bool changed = false;
        auto list = std::get<EntityList>(m_this);

        EntityList revised;
        for (const auto &o : other)
        {
          const auto &id = o->getIdentity();
          if (id.index() != EMPTY)
          {
            decltype(list.begin()) it;
            for (it = list.begin(); it != list.end(); it++)
            {
              if ((*it)->getIdentity() == id)
                break;
            }

            if (it != list.end())
            {
              if ((*it)->updateTo(o))
                changed = true;
              list.erase(it);
              revised.push_back(*it);
            }
          }
          else
          {
            decltype(list.begin()) it;
            for (it = list.begin(); it != list.end(); it++)
            {
              if (*(o.get()) == *(it->get()))
                break;
            }

            if (it != list.end())
            {
              revised.push_back(*it);
              list.erase(it);
            }
            else
            {
              revised.push_back(o);
              changed = true;
            }
          }
        }

        if (changed)
          m_this = revised;

        return changed;
      }

      template <class T>
      bool operator()(const T &other)
      {
        if (std::get<T>(m_this) != other)
        {
          m_this = other;
          return true;
        }
        else
        {
          return false;
        }
      }

    private:
      Value &m_this;
    };

    inline bool Entity::updateTo(const EntityPtr other)
    {
      bool changed = false;
      if (m_name != other->m_name)
      {
        m_name = other->m_name;
        changed = true;
      }

      std::vector<PropertyKey> removed;
      for (auto &[key, value] : m_properties)
      {
        auto op = other->m_properties.find(key);
        if (op != other->m_properties.end())
        {
          if (value.index() != op->second.index())
          {
            value = op->second;
            changed = true;
          }
          else
          {
            if (std::visit(ValueMergeVisitor(value), op->second))
              changed = true;
          }
        }
        else
        {
          removed.push_back(key);
          changed = true;
        }
      }

      return changed;
    }
  }  // namespace entity
}  // namespace mtconnect
