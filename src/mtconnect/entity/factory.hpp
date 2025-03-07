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

#include <map>
#include <set>
#include <unordered_map>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"

namespace mtconnect {
  namespace entity {
    using Requirements = std::list<Requirement>;

    /// @brief Factory for creating entities
    class AGENT_LIB_API Factory : public Matcher, public std::enable_shared_from_this<Factory>
    {
    public:
      using Function = std::function<EntityPtr(const std::string &name, Properties &)>;
      using Matcher = std::function<bool(const std::string &)>;
      using MatchPair = std::pair<Matcher, FactoryPtr>;
      using StringFactory = std::unordered_map<std::string, FactoryPtr>;
      using MatchFactory = std::list<MatchPair>;

    public:
      /// @brief factory method to create an `Entity`
      /// @param name name of the entity
      /// @param p properties for the entity
      /// @return shared entity pointer
      static auto createEntity(const std::string &name, Properties &p)
      {
        return std::make_shared<Entity>(name, p);
      }

      Factory(const Factory &other) = default;
      Factory() : m_function(createEntity) {}
      ~Factory() = default;

      /// @brief create a factory with a set of requirements
      /// @param r
      Factory(const Requirements r) : m_requirements(r), m_function(createEntity)
      {
        registerEntityRequirements();
      }
      /// @brief create a factory with a set of requirements and a factory lambda
      /// @param r the requirements
      /// @param f the factory method
      Factory(const Requirements r, Function f) : m_requirements(r), m_function(f)
      {
        registerEntityRequirements();
      }
      /// @brief copy the factory and all the related factories
      /// @return a new factory
      FactoryPtr deepCopy() const;

      /// @brief get a shared pointer to this factory
      /// @return shared factory pointer
      FactoryPtr getptr() { return shared_from_this(); }

      /// @brief set the factory function
      /// @param f the factory function
      void setFunction(Function f) { m_function = f; }
      /// @brief set the entity parameter order
      /// @param list the ordered list of parameters
      void setOrder(OrderList list)
      {
        m_order = std::make_shared<OrderMap>();
        int i = 0;
        for (auto &e : list)
          m_order->emplace(e, i++);
      }
      /// @brief set the order from a order map
      /// @param list the order map
      void setOrder(OrderMapPtr &list) { m_order = list; }
      /// @brief get the order list
      /// @return pointer to the order list
      const OrderMapPtr &getOrder() const { return m_order; }

      /// @brief set if this is a list factory
      /// @param list `true` if this is a list
      void setList(bool list) { m_isList = list; }
      /// @brief gets the list state of this factory
      /// @return `true` if this is a list factory
      bool isList() const { return m_isList; }
      /// @brief sets the raw state of this factory
      ///
      /// Raw does not interpret the data, but turns it into a set of entities
      ///
      /// @param raw `true` if it is raw
      void setHasRaw(bool raw) { m_hasRaw = raw; }
      /// @brief gets raw state
      /// @return `true` if the factory creates a raw entity
      bool hasRaw() const { return m_hasRaw; }
      /// @brief indicator if this factory creates extended entities
      /// @return `true` if this factory supports any types
      bool isAny() const { return m_any; }
      /// @brief sets the any state
      /// @param any `true` if supports any
      void setAny(bool any) { m_any = any; }
      /// @brief sets the minimum size of the entity list
      /// @param size minimum size of an entity list
      void setMinListSize(size_t size)
      {
        m_minListSize = size;
        m_isList = true;
      }

      /// @brief does this factory have a requirement for the property
      /// @param name the property key
      /// @return `true` if there is a requirement
      bool isProperty(const std::string &name) const { return m_properties.count(name) > 0; }
      /// @brief checks if this is a property set
      /// @param name the property key
      /// @return `true` if this is a property with ENTITY or ENTITY_SET with multiplicity more than
      /// 0
      bool isPropertySet(const std::string &name) const { return m_propertySets.count(name) > 0; }
      /// @brief is there a requirement with a simple value
      /// @param name the property key
      /// @return `true` if this is a value propery
      bool isSimpleProperty(const std::string &name) const
      {
        return m_simpleProperties.count(name) > 0;
      }

      /// @brief is this requirement resolved with a data set
      /// @param name the name of the property
      /// @return `true` if this is a data set
      bool isDataSet(const std::string &name) const { return m_dataSets.count(name) > 0; }

      /// @brief is this requirement resolved with a table
      /// @param name the name of the property
      /// @return `true` if this is a table
      bool isTable(const std::string &name) const { return m_tables.count(name) > 0; }

      /// @brief get the requirement pointer for a key
      /// @param name the property key
      /// @return requirement pointer
      Requirement *getRequirement(const std::string &name)
      {
        for (auto &r : m_requirements)
        {
          if (r.getName() == name)
            return &r;
        }
        return nullptr;
      }

      /// @brief add requirements to the factory
      /// @param reqs the set of requirements
      void addRequirements(const Requirements &reqs)
      {
        for (const auto &r : reqs)
        {
          auto old = std::find_if(m_requirements.begin(), m_requirements.end(),
                                  [&r](Requirement &o) { return r.getName() == o.getName(); });
          if (old != m_requirements.end())
          {
            *old = r;
          }
          else
          {
            m_requirements.emplace_back(r);
          }
        }
        registerEntityRequirements();
      }
      /// @brief convert properties to the requirements
      /// @param[in,out] p the properties to convert
      /// @param[in,out] errors errors related to conversions
      void performConversions(Properties &p, ErrorList &errors) const;
      /// @brief check if the properties are sufficient for the factory
      /// @param[in,out] properties the properties for the entity
      /// @param[in,out] errors errors related to verification
      /// @return `true` if the properties are sufficient
      virtual bool isSufficient(Properties &properties, ErrorList &errors) const;

      /// @name Entity factory
      ///@{

      /// @brief make an entity given an entity name and properties
      /// @param[in] name the name of the entity
      /// @param[in,out] p properties for the entity
      /// @param[in,out] errors errors when creating the entity
      /// @return shared entity pointer if successful
      EntityPtr make(const std::string &name, Properties &p, ErrorList &errors) const
      {
        try
        {
          performConversions(p, errors);
          if (isSufficient(p, errors))
          {
            auto ent = m_function(name, p);
            if (m_order)
              ent->setOrder(m_order);
            return ent;
          }
        }

        catch (EntityError &e)
        {
          e.setEntity(name);
          errors.emplace_back(std::make_unique<EntityError>(e));
          LogError("Failed to create " + name + ": " + e.what());
        }

        for (auto &e : errors)
        {
          if (e->getEntity().empty())
            e->setEntity(name);
        }

        return nullptr;
      }

      /// @brief alias for `make()`
      EntityPtr operator()(const std::string &name, Properties &p, ErrorList &errors) const
      {
        return make(name, p, errors);
      }

      ///@}

      /// @brief add a factory for entities with a string matcher
      /// @param name the name to match against
      /// @param factory the factory to create entities
      /// @return `true` if successful
      bool registerFactory(const std::string &name, FactoryPtr factory)
      {
        m_stringFactory.emplace(make_pair(name, factory));
        return true;
      }
      /// @brief add a factory for entities with a regular expression matcher
      /// @param exp expression to match against
      /// @param factory the factory to create entities
      /// @return `true` if successful
      bool registerFactory(const std::regex &exp, FactoryPtr factory)
      {
        auto matcher = [exp](const std::string &name) { return std::regex_match(name, exp); };
        m_matchFactory.emplace_back(make_pair(matcher, factory));
        return true;
      }
      /// @brief add a factory for entities with custom matcher
      /// @param matcher matcher to use to match
      /// @param factory the factory to create entities
      /// @return `true` if successful
      bool registerFactory(const Matcher &matcher, FactoryPtr factory)
      {
        m_matchFactory.emplace_back(make_pair(matcher, factory));
        return true;
      }

      /// @brief find a factory for a name
      /// @param name the name to match
      /// @return the factory
      FactoryPtr factoryFor(const std::string &name) const
      {
        const auto it = m_stringFactory.find(name);
        if (it != m_stringFactory.end())
          return it->second;
        else
        {
          for (const auto &r : m_matchFactory)
          {
            if (r.first(name))
              return r.second;
          }
        }

        return nullptr;
      }

      /// @brief check if a factory exists
      /// @param s name to match against
      /// @return `true` if there is a factory
      bool matches(const std::string &s) const override
      {
        auto f = factoryFor(s);
        return (bool)f;
      }

      /// @name Methods to create entities
      ///@{

      /// @brief create an entity with an entity list
      ///
      /// Looks for a factory for name
      ///
      /// @param name the name of the entity
      /// @param a list of entities
      /// @param errors errors when creating entity
      /// @return entity if successful
      EntityPtr create(const std::string &name, EntityList &a, ErrorList &errors)
      {
        auto factory = factoryFor(name);
        if (factory)
        {
          Properties p {{"LIST", a}};
          return factory->make(name, p, errors);
        }
        else
          return nullptr;
      }

      /// @brief create an entity with properties
      ///
      /// Looks for a factory for name
      ///
      /// @param[in] name the entity name
      /// @param[in,out] a the properties
      /// @param[in,out] errors errors when creating entity
      /// @return entity if successful
      EntityPtr create(const std::string &name, Properties &a, ErrorList &errors)
      {
        auto factory = factoryFor(name);
        if (factory)
          return factory->make(name, a, errors);
        else
          return nullptr;
      }

      /// @brief create an entity with properties as rvalue
      ///
      /// Looks for a factory for name
      ///
      /// @param name the entity name
      /// @param[in,out] a the properties as an rvalue
      /// @param[in,out] errors errors when creating entity
      /// @return entity if successful
      EntityPtr create(const std::string &name, Properties &&a, ErrorList &errors)
      {
        auto factory = factoryFor(name);
        if (factory)
          return factory->make(name, a, errors);
        else
          return nullptr;
      }

      /// @brief create an entity with properties as rvalue
      ///
      /// Suppresses errors.
      ///
      /// @param name the entity name
      /// @param[in,out] a the properties as an rvalue
      /// @return entity if successful
      EntityPtr create(const std::string &name, Properties &a)
      {
        ErrorList list;
        return create(name, a, list);
      }

      ///@}

      /// @brief scan requirements and register factories refrenced in the requirements
      void registerEntityRequirements()
      {
        if (!m_simpleProperties.count("originalId"))
        {
          m_requirements.emplace_back("originalId", false);
        }
        for (auto &r : m_requirements)
        {
          m_properties.emplace(r.getName());
          auto factory = r.getFactory();
          if (factory &&
              (r.getType() == ValueType::ENTITY || r.getType() == ValueType::ENTITY_LIST))
          {
            registerFactory(r.getName(), factory);
            if (r.getUpperMultiplicity() > 1)
              m_propertySets.insert(r.getName());
          }
          else if (r.getName() == "RAW")
          {
            m_hasRaw = true;
          }
          else if (BaseValueType(r.getType()) == ValueType::DATA_SET)
          {
            m_dataSets.insert(r.getName());
            if (r.getType() == ValueType::TABLE)
              m_tables.insert(r.getName());
          }
          else
          {
            m_simpleProperties.insert(r.getName());
          }
        }
      }

      /// @brief register matchers for requirements to this factory if not given
      void registerMatchers()
      {
        auto m = getptr();
        for (auto &r : m_requirements)
        {
          if (r.getUpperMultiplicity() > 1 && !r.hasMatcher())
          {
            r.setMatcher(m);
          }
        }
      }

      /// @name Testing
      ///@{

      /// @brief method used only in testing
      void clear()
      {
        m_stringFactory.clear();
        m_matchFactory.clear();
      }
      ///@}

    protected:
      using FactoryMap = std::map<FactoryPtr, FactoryPtr>;
      static void LogError(const std::string &what);
      void _deepCopy(FactoryMap &factories);
      static void _dupFactory(FactoryPtr &factory, FactoryMap &factories);

    protected:
      Requirements m_requirements;
      Function m_function;
      OrderMapPtr m_order;

      StringFactory m_stringFactory;
      MatchFactory m_matchFactory;

      bool m_isList {false};
      size_t m_minListSize {0};
      bool m_hasRaw {false};
      bool m_any {false};

      std::set<std::string> m_propertySets;
      std::set<std::string> m_dataSets;
      std::set<std::string> m_tables;
      std::set<std::string> m_simpleProperties;
      std::set<std::string> m_properties;
    };

  }  // namespace entity
}  // namespace mtconnect
