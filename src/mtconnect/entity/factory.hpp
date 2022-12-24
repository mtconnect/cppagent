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

#include <map>
#include <set>
#include <unordered_map>

#include "entity/entity.hpp"

namespace mtconnect {
  namespace entity {
    using Requirements = std::list<Requirement>;

    class Factory : public Matcher, public std::enable_shared_from_this<Factory>
    {
    public:
      using Function = std::function<EntityPtr(const std::string &name, Properties &)>;
      using Matcher = std::function<bool(const std::string &)>;
      using MatchPair = std::pair<Matcher, FactoryPtr>;
      using StringFactory = std::unordered_map<std::string, FactoryPtr>;
      using MatchFactory = std::list<MatchPair>;

    public:
      // Factory Methods
      static auto createEntity(const std::string &name, Properties &p)
      {
        return std::make_shared<Entity>(name, p);
      }

      Factory(const Factory &other) = default;
      Factory() : m_function(createEntity) {}
      ~Factory() = default;
      Factory(const Requirements r) : m_requirements(r), m_function(createEntity)
      {
        registerEntityRequirements();
      }
      Factory(const Requirements r, Function f) : m_requirements(r), m_function(f)
      {
        registerEntityRequirements();
      }
      FactoryPtr deepCopy() const;

      FactoryPtr getptr() { return shared_from_this(); }

      void setFunction(Function f) { m_function = f; }
      void setOrder(OrderList list)
      {
        m_order = std::make_shared<OrderMap>();
        int i = 0;
        for (auto &e : list)
          m_order->emplace(e, i++);
      }
      void setOrder(OrderMapPtr &list) { m_order = list; }
      const OrderMapPtr &getOrder() const { return m_order; }

      void setList(bool list) { m_isList = list; }
      bool isList() const { return m_isList; }
      void setHasRaw(bool raw) { m_hasRaw = raw; }
      bool hasRaw() const { return m_hasRaw; }
      bool isAny() const { return m_any; }
      void setAny(bool any) { m_any = any; }
      void setMinListSize(size_t size)
      {
        m_minListSize = size;
        m_isList = true;
      }

      bool isProperty(const std::string &name) const { return m_properties.count(name) > 0; }
      bool isPropertySet(const std::string &name) const { return m_propertySets.count(name) > 0; }
      bool isSimpleProperty(const std::string &name) const
      {
        return m_simpleProperties.count(name) > 0;
      }

      Requirement *getRequirement(const std::string &name)
      {
        for (auto &r : m_requirements)
        {
          if (r.getName() == name)
            return &r;
        }
        return nullptr;
      }

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
      void performConversions(Properties &p, ErrorList &errors) const;
      virtual bool isSufficient(Properties &properties, ErrorList &errors) const;

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

      // Factory
      bool registerFactory(const std::string &name, FactoryPtr factory)
      {
        m_stringFactory.emplace(make_pair(name, factory));
        return true;
      }

      bool registerFactory(const std::regex &exp, FactoryPtr factory)
      {
        auto matcher = [exp](const std::string &name) { return std::regex_match(name, exp); };
        m_matchFactory.emplace_back(make_pair(matcher, factory));
        return true;
      }

      bool registerFactory(const Matcher &matcher, FactoryPtr factory)
      {
        m_matchFactory.emplace_back(make_pair(matcher, factory));
        return true;
      }

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

      bool matches(const std::string &s) const override
      {
        auto f = factoryFor(s);
        return (bool)f;
      }

      EntityPtr operator()(const std::string &name, Properties &p, ErrorList &errors) const
      {
        return make(name, p, errors);
      }

      std::shared_ptr<Entity> create(const std::string &name, EntityList &a, ErrorList &errors)
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

      std::shared_ptr<Entity> create(const std::string &name, Properties &a, ErrorList &errors)
      {
        auto factory = factoryFor(name);
        if (factory)
          return factory->make(name, a, errors);
        else
          return nullptr;
      }
      std::shared_ptr<Entity> create(const std::string &name, Properties &&a, ErrorList &errors)
      {
        auto factory = factoryFor(name);
        if (factory)
          return factory->make(name, a, errors);
        else
          return nullptr;
      }

      std::shared_ptr<Entity> create(const std::string &name, Properties &a)
      {
        ErrorList list;
        return create(name, a, list);
      }

      void registerEntityRequirements()
      {
        for (auto &r : m_requirements)
        {
          m_properties.emplace(r.getName());
          auto factory = r.getFactory();
          if (factory && (r.getType() == ENTITY || r.getType() == ENTITY_LIST))
          {
            registerFactory(r.getName(), factory);
            if (r.getUpperMultiplicity() > 1)
              m_propertySets.insert(r.getName());
          }
          else if (r.getName() == "RAW")
          {
            m_hasRaw = true;
          }
          else
          {
            m_simpleProperties.insert(r.getName());
          }
        }
      }

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

      // For testing
      void clear()
      {
        m_stringFactory.clear();
        m_matchFactory.clear();
      }

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
      std::set<std::string> m_simpleProperties;
      std::set<std::string> m_properties;
    };

  }  // namespace entity
}  // namespace mtconnect
