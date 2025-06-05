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

#include <boost/beast/core/detail/base64.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/unordered_set.hpp>
#include <boost/uuid/detail/sha1.hpp>

#include <unordered_map>

#include "data_set.hpp"
#include "mtconnect/config.hpp"
#include "qname.hpp"
#include "requirement.hpp"

namespace mtconnect {
  /// @brief Entity namespace
  namespace entity {
    /// @brief The key for a prpoerty in a property set
    ///
    /// A qname with a mark that allows the property to be checked for requirements
    struct PropertyKey : public QName
    {
      using QName::QName;
      PropertyKey(const PropertyKey &s) : QName(s) {}
      PropertyKey(const std::string &s) : QName(s) {}
      PropertyKey(const std::string &&s) : QName(s) {}
      PropertyKey(const char *s) : QName(s) {}

      /// @brief clears marks for this property
      void clearMark() const { const_cast<PropertyKey *>(this)->m_mark = false; }
      /// @brief sets the mark for this property
      void setMark() const { const_cast<PropertyKey *>(this)->m_mark = true; }

      /// @brief allows factory to track required properties
      bool m_mark {false};
    };

    /// @brief properties are a map of PropertyKey to Value
    using Properties = std::map<PropertyKey, Value>;
    using OrderList = std::list<std::string>;
    using OrderMap = std::unordered_map<std::string, int>;
    using OrderMapPtr = std::shared_ptr<OrderMap>;
    using Property = std::pair<PropertyKey, Value>;
    using AttributeSet = std::set<QName>;

    /// @brief Get a property by name if it exists
    /// @tparam T The property type
    /// @param[in] key the key to get
    /// @param[in] props the set of properties
    /// @return the property value or std::nullopt
    template <typename T>
    inline std::optional<T> OptionallyGet(const std::string &key, const Properties &props)
    {
      auto p = props.find(key);
      if (p != props.end())
        return std::get<T>(p->second);
      else
        return std::nullopt;
    }

    /// @brief The base entity class
    ///
    /// The Entity is the foundation of the all information models used by the agent. An entity can
    /// be parsed or serialized as XML or JSON. The entity provides a factory and requirement
    /// capability to validate the model.
    class AGENT_LIB_API Entity : public std::enable_shared_from_this<Entity>
    {
    public:
      using super = std::nullptr_t;

      /// @brief Create an empty entity
      Entity() {}
      /// @brief Create an entity with a name
      /// @param name entity name
      Entity(const std::string &name) : m_name(name) {}
      /// @brief Create an entity with a name and property set
      /// @param name entity name
      /// @param props entity properties
      Entity(const std::string &name, const Properties &props) : m_name(name), m_properties(props)
      {}
      Entity(const Entity &entity)
        : m_name(entity.m_name), m_properties(entity.m_properties), m_order(entity.m_order)
      {
        if (entity.m_attributes)
          setAttributes(*entity.m_attributes);
      }
      virtual ~Entity() {}

      /// @brief Get a shared pointer
      /// @return shared pointer to the entity
      EntityPtr getptr() const { return const_cast<Entity *>(this)->shared_from_this(); }

      /// @brief method to return the entities identity. defaults to `id`.
      /// @return the identity
      virtual const entity::Value &getIdentity() const { return getProperty("id"); }

      /// @brief create unique ids recursively
      /// @param[in,out] idMap  old entity id to new entity id map
      /// @param[in] sha the root sha1
      /// @returns optional string value of the new id
      virtual std::optional<std::string> createUniqueId(
          std::unordered_map<std::string, std::string> &idMap,
          const boost::uuids::detail::sha1 &sha1);

      /// @brief update all id references to the new ids recursively
      /// @param[in] idMap map of old ids to new ids
      virtual void updateReferences(const std::unordered_map<std::string, std::string> idMap);

      /// @brief checks if there is entity is a list that has additional properties
      /// @return `true` if this a a list with additional attributes
      bool hasListWithAttribute() const
      {
        return (m_properties.count("LIST") > 0 && m_properties.size() > 1);
      }
      /// @brief checkis if this is an entity only containing a list of entities with no other
      /// properties
      /// @returns `true` if this entity only contains a list
      bool isSimpleList() const
      {
        return (m_properties.count("LIST") > 0 && m_properties.size() == 1);
      }
      /// @brief checkis if this is an entity only containing a list
      /// @returns `true` if this entity contains a list
      bool isList() const { return m_properties.count("LIST") > 0; }
      /// @brief check if the property is hidden for normal serialization
      /// @param[in] name the name of the property
      ///
      /// Used for serializing properties for configuration purposes where additional attibutes
      /// may be included in the document. The only property is `originalId` for now.
      ///
      /// This may be extended to include a property set in the future.
      /// @returns `true` if the property is hidden
      bool isHidden(const std::string &name) const { return name == "originalId"; }
      /// @brief get the name of the entity
      /// @return name
      const auto &getName() const { return m_name; }
      /// @brief get a const reference to the properties
      /// @return properties
      const Properties &getProperties() const { return m_properties; }
      /// @brief get a property for a ley
      /// @param n the key
      /// @return The property or a Value with std::monstate() if not found
      const Value &getProperty(const std::string &n) const
      {
        static Value noValue {std::monostate()};
        auto it = m_properties.find(n);
        if (it == m_properties.end())
          return noValue;
        else
          return it->second;
      }
      /// @brief set a property
      /// @param key property key
      /// @param v property value
      virtual void setProperty(const std::string &key, const Value &v)
      {
        m_properties.insert_or_assign(key, v);
      }
      /// @brief set a property using a key/value pair
      /// @param property the key/value pair
      void setProperty(const Property &property) { setProperty(property.first, property.second); }
      /// @brief check if a propery exists
      /// @param n the key
      /// @return `true` if the property exists
      bool hasProperty(const std::string &n) const
      {
        return m_properties.find(n) != m_properties.end();
      }
      /// @brief checks if there is a `VALUE` property
      /// @return `true` if there is a `VALUE`
      bool hasValue() const { return hasProperty("VALUE"); }
      /// @brief set the name to a string
      /// @param name the name
      void setName(const std::string &name) { m_name = name; }
      /// @brief set the name as a qname
      /// @param name the qname
      void setQName(const std::string &name) { m_name.setQName(name); }
      /// @brief apply function f to the property if it exists
      /// @param name the key
      /// @param f the lambda to be called if the property exists
      void applyTo(const std::string &name, std::function<void(Value &v)> f)
      {
        auto p = m_properties.find(name);
        if (p != m_properties.end())
          f(p->second);
      }
      /// @brief apply a function to a `VALUE` property
      /// @param f the function
      void applyToValue(std::function<void(Value &v)> f) { applyTo("VALUE", f); }

      /// @brief get the `VALUE` property if it exists, Value is mutable
      /// @return `VALUE` property
      Value &getValue()
      {
        static Value null;
        auto p = m_properties.find("VALUE");
        if (p != m_properties.end())
          return p->second;
        else
          return null;
      }
      /// @brief get a const reference to the `VALUE` property
      const Value &getValue() const { return getProperty("VALUE"); }
      /// @brief get the entity with a list property if it exists
      ///
      /// This is a convience method that gets the property by name. If the property is an
      /// `EntityPtr` and has a `LIST` property, then return the `EntityList` value.
      ///
      /// @param name the key for the list
      /// @return the entity list or std::nullopt
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

      /// @brief Get the LIST property if it exists from an entity
      /// @returns a reference to the entity list. Returns an empty list if it does not exist.
      const EntityList &getListProperty() const
      {
        static EntityList null;

        auto p = m_properties.find("LIST");
        if (p != m_properties.end())
        {
          auto *l = std::get_if<EntityList>(&p->second);
          if (l)
            return *l;
        }
        return null;
      }

      /// @brief Get the LIST property if it exists from an entity
      /// @returns a reference to the entity list. Returns an empty list if it does not exist.
      EntityList &getListProperty()
      {
        static EntityList null;

        auto p = m_properties.find("LIST");
        if (p != m_properties.end())
        {
          auto *l = std::get_if<EntityList>(&p->second);
          if (l)
            return *l;
        }
        return null;
      }

      /// @brief Add an entity to an entity list
      /// @param name the property key
      /// @param factory the factory for the entity
      /// @param entity entity to add
      /// @param errors errors if add fails
      /// @return `true` if successful
      bool addToList(const std::string &name, FactoryPtr factory, EntityPtr entity,
                     ErrorList &errors);
      /// @brief Remove an entity from an entity list
      /// @param name the key for the entity list
      /// @param entity the entity to remove
      /// @return `true` if successful
      bool removeFromList(const std::string &name, EntityPtr entity);

      /// @brief sets the `VALUE` property
      /// @param v the value
      void setValue(const Value &v) { setProperty("VALUE", v); }
      /// @brief remove a property
      /// @param name the key
      void erase(const std::string &name) { m_properties.erase(name); }

      /// @brief get a property for a key
      /// @tparam T the type of the property
      /// @param name the key
      /// @return the value as type T
      /// @throws `std::bad_variant_access` if incorrect type
      template <typename T>
      const T &get(const std::string &name) const
      {
        return std::get<T>(getProperty(name));
      }

      /// @brief get the `VALUE` property
      /// @tparam T the type of the value
      /// @return the value as type T
      /// @throws `std::bad_variant_access` if incorrect type
      template <typename T>
      const T &getValue() const
      {
        return std::get<T>(getValue());
      }

      /// @brief gets property if it exists
      /// @tparam T the property type
      /// @param name the key
      /// @return the value of the property as type T or nullopt
      /// @throws `std::bad_variant_access` if incorrect type
      template <typename T>
      const std::optional<T> maybeGet(const std::string &name) const
      {
        return OptionallyGet<T>(name, m_properties);
      }
      /// @brief gets `VALUE` property if it exists
      /// @tparam T the property type
      /// @return the value as type T or nullopt
      /// @throws `std::bad_variant_access` if incorrect type
      template <typename T>
      const std::optional<T> maybeGetValue() const
      {
        return OptionallyGet<T>("VALUE", m_properties);
      }

      /// @brief Sets the entity property ordering map
      /// @param[in] order the property order
      void setOrder(const OrderMapPtr order)
      {
        if (!m_order)
          m_order = order;
      }
      /// @brief get the property order map
      const OrderMapPtr getOrder() const { return m_order; }
      /// @brief get an iterator to the property
      /// @param[in] name the key
      /// @return an iterator to the property
      auto find(const std::string &name) { return m_properties.find(name); }
      /// @brief erase a propery using an iterator
      /// @param[in] it an iterator pointing to the propery
      /// @return the iterator after erase
      auto erase(Properties::iterator &it) { return m_properties.erase(it); }
      /// @brief tells the entity which properties are attributes for XML generation
      /// @param[in] a the attributes
      void setAttributes(AttributeSet a)
      {
        std::unique_ptr<AttributeSet> set(new AttributeSet(a));
        m_attributes.swap(set);
      }
      /// @brief get the attributes for XML generation
      /// @return attribute set
      const auto &getAttributes() const
      {
        static AttributeSet empty;
        if (m_attributes)
          return *m_attributes;
        else
          return empty;
      }

      /// @brief compare two entities for equality
      /// @param other the other entity
      /// @return `true` if they have equal name and properties
      bool operator==(const Entity &other) const;

      /// @brief compare two entities for inequality
      /// @param other the other entity
      /// @return `true` if they have unequal name and properties
      bool operator!=(const Entity &other) const { return !(*this == other); }

      /// @brief update this entity to be the same as other
      /// @param other the other entity
      /// @param protect properties to protect from change
      /// @return `true` if successful
      bool reviseTo(const EntityPtr other, const std::set<std::string> protect = {});

      /// @brief Create a consistent entity digest that is independent of representation
      ///
      /// The algo uses sha1 to create the digest and then base64 encodes the result. This calls
      /// the protected virtual method`hash(boost::uuids::detail::sha1 &sha1)`
      /// which iterates all the properties and skips properties that should not be taken into
      /// consideration.
      ///
      /// @return A base64 string digest of the entity's content
      std::string hash() const
      {
        boost::uuids::detail::sha1 sha1;
        hash(sha1);

        unsigned int digest[5];
        sha1.get_digest(digest);

        char encoded[32];
        auto len = boost::beast::detail::base64::encode(encoded, digest, sizeof(digest));

        return std::string(encoded, len);
      }

      /// @brief Compute the hash of the entity and add a `hash` property.
      void addHash()
      {
        auto hv = hash();
        setProperty("hash", hv);
      }

    protected:
      template <typename ST>
      friend struct HashVisitor;

      /// @brief Computes the sha1 hash of the entity skipping properties in `skip`
      /// @param[in,out] sha1 The boost sha1 accumulator
      /// @param[in] skip A set of parameters to skip–not recursive
      void hash(boost::uuids::detail::sha1 &sha1,
                const boost::unordered_set<std::string> &skip) const;

      /// @brief The virtual method that covers `hash(boost::uuids::detail::sha1&,
      /// boost::unordered_set<std::string> skip)`
      ///
      /// Overload this methid in sub-classes to provide the set of properties to skip. Such as
      /// {"hash", "timestamp"} that should not be included in the unique hash of the entity.
      ///
      /// @param[in,out] sha1 The boost sha1 accumulator
      virtual void hash(boost::uuids::detail::sha1 &sha1) const
      {
        // Default do not skip anything, subclasses add skipped
        // parameters.
        static const boost::unordered_set<std::string> skip;
        hash(sha1, skip);
      }

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
      std::unique_ptr<AttributeSet> m_attributes;
      std::unique_ptr<ErrorList> m_errors;
    };

    /// @brief variant visitor to compare two entity parameter values for equality
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

        auto it = list.cbegin();
        if (!std::holds_alternative<std::monostate>((*it)->getIdentity()))
        {
          for (; it != list.cend(); it++)
          {
            auto id = (*it)->getIdentity();
            auto oit =
                boost::find_if(other, [&id](const auto &e) { return id == e->getIdentity(); });
            if (oit == other.end() || *(it->get()) != *(oit->get()))
              return false;
          }
        }
        else
        {
          for (auto oit = other.cbegin(); it != list.cend(); it++, oit++)
          {
            if (*(it->get()) != *(oit->get()))
              return false;
          }
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

    /// @brief variant visitor to merge two entities
    struct ValueMergeVisitor
    {
      ValueMergeVisitor(Value &t, const std::set<std::string> protect)
        : m_this(t), m_protect(protect)
      {}

      bool operator()(const EntityPtr &other)
      {
        return std::get<EntityPtr>(m_this)->reviseTo(other, m_protect);
      }

      bool mergeRemainder(EntityList &list, EntityList &revised, bool changed)
      {
        if (changed)
        {
          for (auto &o : list)
          {
            const auto &id = o->getIdentity();
            if (std::holds_alternative<std::string>(id))
            {
              auto s = std::get<std::string>(id);
              if (m_protect.count(s) > 0)
                revised.push_back(o);
            }
          }
        }
        else
        {
          changed = std::any_of(list.begin(), list.end(), [this](const auto &o) {
            const auto &id = o->getIdentity();
            if (std::holds_alternative<std::string>(id))
            {
              auto s = std::get<std::string>(id);
              return m_protect.count(s) == 0;
            }
            else
            {
              return true;
            }
          });
        }

        return changed;
      }

      bool operator()(const EntityList &other)
      {
        bool changed = false;
        auto list = std::get<EntityList>(m_this);

        EntityList revised;
        for (const auto &o : other)
        {
          if (const auto &id = o->getIdentity(); !std::holds_alternative<std::monostate>(id))
          {
            auto it = boost::find_if(list, [&id](auto &e) { return e->getIdentity() == id; });
            LOG(trace) << " ... Merging " << o->getName() << " with identity: ";
            if (std::holds_alternative<std::string>(id))
              LOG(trace) << std::get<std::string>(id);

            if (it != list.end())
            {
              if ((*it)->reviseTo(o, m_protect))
              {
                LOG(trace) << " ... ... List item: " << o->getName() << " changed";
                changed = true;
              }
              revised.push_back(*it);
              list.erase(it);
            }
            else
            {
              LOG(trace) << " ... ... List item: " << o->getName() << " added";
              revised.push_back(o);
              changed = true;
            }
          }
          else
          {
            LOG(trace) << " ... Merging " << o->getName() << " with no identity";

            auto it = boost::find_if(list, [&o](auto &e) { return *(o.get()) == *(e.get()); });

            if (it != list.end())
            {
              LOG(trace) << " ... ... List item: " << o->getName() << " found and replacing";
              revised.push_back(*it);
              list.erase(it);
            }
            else
            {
              LOG(trace) << " ... ... List item: " << o->getName() << " added";
              revised.push_back(o);
              changed = true;
            }
          }
        }

        if (!list.empty())
        {
          changed = mergeRemainder(list, revised, changed);
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
      std::set<std::string> m_protect;
    };

    inline bool Entity::reviseTo(const EntityPtr other, const std::set<std::string> protect)
    {
      bool changed = false;
      if (m_name != other->m_name)
      {
        LOG(trace) << "Entity: " << m_name << "Changed name to: " << other->m_name;
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
            LOG(trace) << m_name << " Property: " << key << " changed value type";
            value = op->second;
            changed = true;
          }
          else
          {
            if (std::visit(ValueMergeVisitor(value, protect), op->second))
            {
              LOG(trace) << m_name << " Property: " << key << " changed value";
              changed = true;
            }
          }
        }
        else
        {
          LOG(trace) << m_name << " Property: " << key << " removed";
          removed.push_back(key);
          changed = true;
        }
      }

      return changed;
    }
  }  // namespace entity
}  // namespace mtconnect
