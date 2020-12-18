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
  namespace entity
  {
    class Entity;
    class Factory;
    using EntityPtr = std::shared_ptr<Entity>;
    using EntityList = std::list<std::shared_ptr<Entity>>;
    using Vector = std::vector<double>;
    using Value = std::variant<EntityPtr, EntityList, std::string, int64_t, double,
                              Vector, nullptr_t>;
    using FactoryPtr = std::shared_ptr<Factory>;

    class PropertyError : public std::logic_error
    {
    public:
      using std::logic_error::logic_error;
    };
    class MissingPropertyError : public PropertyError
    {
    public:
      using PropertyError::PropertyError;
    };
    class ExtraPropertyError : public PropertyError
    {
    public:
      using PropertyError::PropertyError;
    };
    class PropertyTypeError : public PropertyError
    {
    public:
      using PropertyError::PropertyError;
    };
    class PropertyRequirementError : public PropertyError
    {
    public:
      using PropertyError::PropertyError;
    };
    class PropertyConversionError : public PropertyError
    {
    public:
      using PropertyError::PropertyError;
    };
    
    using ErrorList = std::list<PropertyError>;
    
    struct Matcher
    {
      virtual bool matches(const std::string &s) const = 0;
    };
    
    using MatcherPtr = std::weak_ptr<Matcher>;

    class Requirement
    {
    public:
      enum Type {
        ENTITY = 0,
        ENTITY_LIST = 1,
        STRING = 2,
        INTEGER = 3,
        DOUBLE = 4,
        VECTOR = 5,
        NO_VALUE = 6
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
      Requirement(const std::string &name, Type type, FactoryPtr &o,
                  bool required = true);
      Requirement(const std::string &name, Type type, FactoryPtr &o,
                  int lower, int upper);
      
      Requirement() = default;
      Requirement(const Requirement &o) = default;
      ~Requirement() = default;
      
      Requirement &operator =(const Requirement &o)
      {
        m_type = o.m_type;
        m_lowerMultiplicity = o.m_lowerMultiplicity;
        m_upperMultiplicity = o.m_upperMultiplicity;
        m_factory = o.m_factory;
        m_matcher = o.m_matcher;
        return *this;
      }
      
      bool isRequired() const { return m_lowerMultiplicity > 0; }
      bool isOptional() const { return !isRequired(); }
      int getUpperMultiplicity() const { return m_upperMultiplicity; }
      int getLowerMultiplicity() const { return m_lowerMultiplicity; }
      const auto &getMatcher() const { return m_matcher; }
      void setMatcher(MatcherPtr m)
      {
        m_matcher = m;
      }
      const std::string &getName() const { return m_name; }
      Type getType() const { return m_type; }
      auto &getFactory() { return m_factory; }
            
      bool hasMatcher() const { return m_matcher.use_count() > 0; }
      bool isMetBy(const Value &value, bool isList) const;
      bool convertType(Value &value) const;
      bool matches(const std::string &s) const
      {
        if (auto m = m_matcher.lock())
        {
          return m->matches(s);
        }
        else
        {
          return m_name == s;
        }
      }
      
    protected:
      std::string m_name;
      int m_upperMultiplicity;
      int m_lowerMultiplicity;
      Type m_type;
      MatcherPtr m_matcher;
      FactoryPtr m_factory;
    };

        // Inlines
  }
}
