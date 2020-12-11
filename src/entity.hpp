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

#include "asset.hpp"
#include "globals.hpp"

#include <map>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <variant>
#include <list>
#include <memory>
#include <string>
#include <functional>
#include <regex>
#include <typeindex>
#include <tuple>
#include <limits>
#include <stdexcept>

namespace mtconnect
{
  class Entity;
  class EntityFactory;
  using EntityPtr = std::shared_ptr<Entity>;
  using EntityList = std::list<std::shared_ptr<Entity>>;
  using Value = std::variant<EntityPtr, EntityList, std::string, int, double>;
  using EntityFactoryPtr = std::shared_ptr<EntityFactory>;
  
  class PropertyError : public std::logic_error
  {
  public:
    using std::logic_error::logic_error;
  };
  
  class Requirement
  {
  public:
    enum Type {
      ENTITY = 0,
      ENTITY_LIST = 1,
      STRING = 2,
      INTEGER = 3,
      DOUBLE = 4
    };
    
    const static auto Infinite { std::numeric_limits<int>::max() };

  public:
    Requirement(const std::string &name, Type type, bool required = true)
      : m_name(name), m_type(type), m_upperMultiplicity(1),
        m_lowerMultiplicity(required ? 1 : 0)
    {
    }
    Requirement(const std::string &name, bool required, Type type = STRING)
    : m_name(name), m_type(type), m_upperMultiplicity(1),
      m_lowerMultiplicity(required ? 1 : 0)
    {
    }
    Requirement(const std::string &name, Type type, int lower, int upper)
    : m_name(name), m_type(type), m_lowerMultiplicity(lower),
      m_upperMultiplicity(upper)
    {
    }
    Requirement(const std::string &name, Type type, EntityFactoryPtr &o,
                bool required = true);
    
    Requirement() = default;
    Requirement(const Requirement &o);
    ~Requirement() = default;
    
    bool isRequired() const { return m_lowerMultiplicity > 0; }
    bool isOptional() const { return !isRequired(); }
    int getUpperMultiplicity() const { return m_upperMultiplicity; }
    int getLowerMultiplicity() const { return m_lowerMultiplicity; }
    const std::string &getName() const { return m_name; }
    Type getType() const { return m_type; }
    auto &getFactory() { return m_factory; }
    
    bool operator<(const Requirement &o) const
    {
      return m_name < o.m_name;
    }
    
    bool isMetBy(const Value &value) const;
    
  protected:
    std::string m_name;
    int m_upperMultiplicity;
    int m_lowerMultiplicity;
    Type m_type;
    EntityFactoryPtr m_factory;
  };
  
  class Entity
  {
  public:
    using Properties = std::map<std::string, Value>;

    Entity(const std::string &name, const Properties &props)
      : m_name(name), m_properties(props)
    {
    }
    Entity(const Entity &entity) = default;
    virtual ~Entity()
    {
    }
    
    const std::string &getName() const { return m_name; }
    const Properties &getProperties() const
    {
      return m_properties;
    }
    const Value &getProperty(const std::string &n) const
    {
      auto it = m_properties.find(n);
      if (it == m_properties.end())
        return m_noValue;
      else
        return it->second;
    }

    // Entity Factory
  protected:
    std::string m_name;
    Properties m_properties;
    Value m_noValue{""};
  };
  
  using Requirements = std::list<Requirement>;

  class EntityFactory
  {
  public:
    using Function = std::function<std::shared_ptr<Entity>(const std::string &name,
                                                           Entity::Properties &)>;
    using RegexPair = std::pair<std::regex, EntityFactoryPtr>;
    using StringFactory = std::map<std::string, EntityFactoryPtr>;
    using RegexFactory = std::list<RegexPair>;

  public:

    // Factory Methods
    static auto createEntity(const std::string &name,
                             Entity::Properties &p)
    {
      return std::make_shared<Entity>(name, p);
    }
    
    EntityFactory(const EntityFactory &other) = delete;
    EntityFactory() : m_function(createEntity) {}
    ~EntityFactory() = default;
    EntityFactory(Requirements r)
    : m_requirements(r), m_function(createEntity)
    {
      registerEntityRequirements();
    }
    EntityFactory(Requirements r, Function f)
    : m_requirements(r), m_function(f)
    {
      registerEntityRequirements();
    }
    
    bool isSufficient(const Entity::Properties &properties) const
    {
      std::set<std::string> keys;
      std::transform(properties.begin(), properties.end(), std::inserter(keys, keys.end()),
                     [](const auto &v) { return v.first; });
      
      for (const auto &r : m_requirements)
      {
        const auto p = properties.find(r.getName());
        if (p == properties.end() && r.isRequired())
        {
          return false;
        }
        else
        {
          if (!r.isMetBy(p->second))
          {
            return false;
          }
          keys.erase(r.getName());
        }
      }
      
      // Check for additional properties
      if (!keys.empty())
      {
        
      }
      
      return true;
    }
    
    std::shared_ptr<Entity> operator()(const std::string &name,
                                       Entity::Properties &p) const
    {
      if (isSufficient(p))
        return m_function(name, p);
      else
        return nullptr;
    }

    
    // Factory
    bool registerFactory(const std::string &name, EntityFactoryPtr &factory)
    {
      m_stringFactory.emplace(make_pair(name, factory));
      return true;
    }
    bool registerFactory(const std::regex &exp, EntityFactoryPtr &factory)
    {
      m_regexFactory.emplace_back(make_pair(exp, factory));
      return true;
    }
    
    EntityFactoryPtr factoryFor(const std::string &name)
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
    
    std::shared_ptr<Entity> create(const std::string &name, Entity::Properties &a)
    {
      auto factory = factoryFor(name);
      if (factory)
        return (*factory)(name, a);
      else
        return nullptr;
    }
    
    void registerEntityRequirements()
    {
      for (auto &r : m_requirements)
      {
        auto factory = r.getFactory();
        if (factory && (r.getType() == Requirement::ENTITY ||
                        r.getType() == Requirement::ENTITY_LIST))
        {
            registerFactory(r.getName(), factory);
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
    Requirements m_requirements;
    Function m_function;
    
    StringFactory m_stringFactory;
    RegexFactory m_regexFactory;
  };

  
  // Inlines
  inline Requirement::Requirement(const Requirement &o)
  : m_name(o.m_name), m_upperMultiplicity(o.m_upperMultiplicity),
    m_lowerMultiplicity(o.m_lowerMultiplicity), m_type(o.m_type),
    m_factory(o.m_factory)
  {
  }
  
  inline Requirement::Requirement(const std::string &name, Type type, EntityFactoryPtr &f,
                                  bool required)
  : m_name(name), m_type(type), m_upperMultiplicity(1),
    m_lowerMultiplicity(required ? 1 : 0)
  {
    if (type == ENTITY_LIST)
      m_upperMultiplicity = Infinite;
    m_factory = f;
  }
 
  
  inline bool Requirement::isMetBy(const Value &value) const
  {
    if (value.index() != m_type)
      return false;
    
    if (std::holds_alternative<std::string>(value) &&
        std::get<std::string>(value).empty())
      return false;
    
    if ((m_type == ENTITY || m_type == ENTITY_LIST) && !m_factory)
      return false;
    
    if (m_type == ENTITY)
    {
      const auto &e = std::get<EntityPtr>(value);
      return e->getName() == m_name;
    }
    else if (m_type == ENTITY_LIST)
    {
      const auto &l = std::get<EntityList>(value);
      for (const auto &e : l)
      {
        const auto &f = m_factory->factoryFor(e->getName());
        if (!f)
          return false;
      }
    }

    return true;
  }

}
