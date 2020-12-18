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

#include "entity.hpp"

namespace mtconnect
{
  namespace entity
  {
    using ErrorList = std::list<PropertyError>;
    using Requirements = std::list<Requirement>;
    
    class Factory : public Matcher, public std::enable_shared_from_this<Factory>
    {
    public:
      using Function = std::function<std::shared_ptr<Entity>(const std::string &name,
                                                             Properties &)>;
      using RegexPair = std::pair<std::regex, FactoryPtr>;
      using StringFactory = std::map<std::string, FactoryPtr>;
      using RegexFactory = std::list<RegexPair>;
      
    public:
      
      // Factory Methods
      static auto createEntity(const std::string &name,
                               Properties &p)
      {
        return std::make_shared<Entity>(name, p);
      }
      
      Factory(const Factory &other) = default;
      Factory() : m_function(createEntity) {}
      ~Factory() = default;
      Factory(const Requirements r)
      : m_requirements(r), m_function(createEntity)
      {
        registerEntityRequirements();
      }
      Factory(const Requirements r, Function f)
      : m_requirements(r), m_function(f)
      {
        registerEntityRequirements();
      }
      
      std::shared_ptr<Factory> getptr() {
        return shared_from_this();
      }
      
      void setList(bool list) { m_isList = list; }
      bool isList() const { return m_isList; }
      
      Requirement* getRequirement(const std::string &name)
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
                                  [&r](Requirement &o){
                                      return r.getName() == o.getName();
                                  });
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
      
      void setFunction(Function f)
      {
        m_function = f;
      }
      
      void performConversions(Properties &p, ErrorList &errors) const;
      bool isSufficient(const Properties &properties, ErrorList &errors) const;
      
      EntityPtr make(const std::string &name,
                     Properties &p, ErrorList &errors) const
      {
        try
        {
          performConversions(p, errors);
          if (isSufficient(p, errors))
            return m_function(name, p);
        }
        catch (PropertyError &e)
        {
          errors.emplace_back(e);
          LogError("Failed to create " + name + ": " + e.what());
        }
        
        return nullptr;
      }
      
      // Factory
      bool registerFactory(const std::string &name, FactoryPtr &factory)
      {
        m_stringFactory.emplace(make_pair(name, factory));
        return true;
      }
      
      bool registerFactory(const std::regex &exp, FactoryPtr &factory)
      {
        m_regexFactory.emplace_back(make_pair(exp, factory));
        return true;
      }
      
      FactoryPtr factoryFor(const std::string &name) const
      {
        const auto it = m_stringFactory.find(name);
        if (it != m_stringFactory.end())
          return it->second;
        else
        {
          for (const auto &r : m_regexFactory)
          {
            if (std::regex_match(name, r.first))
              return r.second;
          }
        }
        
        return nullptr;
      }
      
      bool matches(const std::string &s) const override
      {
        auto f = factoryFor(s);
        return (bool) f;
      }
      
      EntityPtr operator()(const std::string &name,
                           Properties &p, ErrorList &errors) const
      {
        return make(name, p, errors);
      }

      std::shared_ptr<Entity> create(const std::string &name, EntityList &a,
                                     ErrorList &errors)
      {
        auto factory = factoryFor(name);
        if (factory)
        {
          Properties p { { "list", a } };
          return factory->make(name, p, errors);
        }
        else
          return nullptr;
      }

      std::shared_ptr<Entity> create(const std::string &name, Properties &a,
                                     ErrorList &errors)
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
          auto factory = r.getFactory();
          if (factory && (r.getType() == ENTITY ||
                          r.getType() == ENTITY_LIST))
          {
            registerFactory(r.getName(), factory);
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
        m_regexFactory.clear();
      }
      
    protected:
      static void LogError(const std::string &what);
      
    protected:
      Requirements m_requirements;
      Function m_function;
      
      StringFactory m_stringFactory;
      RegexFactory m_regexFactory;
      bool m_isList { false };
    };

    
  }
}

