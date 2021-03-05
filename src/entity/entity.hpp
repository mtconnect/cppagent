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

#pragma once

#include "observation/data_set.hpp"
#include "qname.hpp"
#include "requirement.hpp"
#include <unordered_map>

namespace mtconnect
{
  namespace entity
  {
    struct PropertyKey : public std::string
    {
      using std::string::string;
      PropertyKey(const PropertyKey &s) : std::string(s) {}
      PropertyKey(const std::string &s) : std::string(s) {}
      PropertyKey(const std::string &&s) : std::string(s) {}
      PropertyKey(const char *s) : std::string(s) {}

      void clearMark() const { const_cast<PropertyKey *>(this)->m_mark = false; }
      void setMark() const { const_cast<PropertyKey *>(this)->m_mark = true; }
      bool m_mark {false};
    };

    using Properties = std::map<PropertyKey, Value>;
    using OrderList = std::list<std::string>;
    using OrderMap = std::unordered_map<std::string, int>;
    using OrderMapPtr = std::shared_ptr<OrderMap>;
    using Property = std::pair<std::string, Value>;

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
      Entity(const std::string &name, const Properties &props) : m_name(name), m_properties(props)
      {
      }
      Entity(const Entity &entity) = default;
      virtual ~Entity() {}

      EntityPtr getptr() const { return const_cast<Entity *>(this)->shared_from_this(); }

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
  }  // namespace entity
}  // namespace mtconnect
