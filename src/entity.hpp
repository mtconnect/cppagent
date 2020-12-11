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
  using EntityPtr = std::shared_ptr<Entity>;
  using EntityList = std::list<std::shared_ptr<Entity>>;
  using Value = std::variant<EntityPtr, EntityList, std::string, int, double>;
  
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

    Requirement() = default;
    Requirement(const Requirement &) = default;
    ~Requirement() = default;
    
    bool isRequired() const { return m_lowerMultiplicity > 0; }
    bool isOptional() const { return !isRequired(); }
    int getUpperMultiplicity() const { return m_upperMultiplicity; }
    int getLowerMultiplicity() const { return m_lowerMultiplicity; }
    const std::string &getName() const { return m_name; }
    Type getType() const { return m_type; }
    
    bool operator<(const Requirement &o) const
    {
      return m_name < o.m_name;
    }
    
    bool isMetBy(const Value &value) const
    {
      if (value.index() != m_type)
        return false;
      
      if (std::holds_alternative<std::string>(value) &&
          std::get<std::string>(value).empty())
        return false;
      
      return true;
    }
    
  protected:
    std::string m_name;
    int m_upperMultiplicity;
    int m_lowerMultiplicity;
    Type m_type;
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
  
  class Factory
  {
  public:
    using Requirements = std::list<Requirement>;
    using Function = std::function<std::shared_ptr<Entity>(const std::string &name,
                                                           Entity::Properties &)>;

  public:
    
    static auto createEntity(const std::string &name,
                      Entity::Properties &p)
    {
      return std::make_shared<Entity>(name, p);
    }
    
    
    Factory() : m_function(createEntity) {}
    Factory(const Factory &) = default;
    ~Factory() = default;
    Factory(Requirements r)
    : m_requirements(r), m_function(createEntity)
    {
    }
    Factory(Requirements r, Function f)
    : m_requirements(r), m_function(f)
    {
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
    
  protected:
    Requirements m_requirements;
    Function m_function;
  };

  class EntityFactory
  {
  public:
    using RegexPair = std::pair<std::regex, Factory>;
    EntityFactory() = delete;

    static bool registerFactory(const std::string &name, const Factory &factory)
    {
      if (!m_stringFactory)
        createFactories();
      m_stringFactory->emplace(make_pair(name, factory));
      return true;
    }
    static bool registerFactory(const std::regex &exp, const Factory &factory)
    {
      if (!m_regexFactory)
        createFactories();
      m_regexFactory->emplace_back(make_pair(exp, factory));
      return true;
    }
    
    inline static std::shared_ptr<Entity> create(const std::string name, Entity::Properties &a)
    {
      if (m_stringFactory)
      {
        const auto it = m_stringFactory->find(name);
        if (it != m_stringFactory->end())
          return it->second(name, a);
        else
        {
          for (const auto &r : *m_regexFactory)
          {
            if (std::regex_match(name, r.first))
              return r.second(name, a);
          }
        }
      }
      return nullptr;
    }
    
    // For testing
    static void clear()
    {
      m_stringFactory->clear();
      m_regexFactory->clear();
    }
    
  protected:
    static void createFactories();
    
  protected:
    static std::unique_ptr<std::map<std::string, Factory>> m_stringFactory;
    static std::unique_ptr<std::list<RegexPair>> m_regexFactory;
  };

}
