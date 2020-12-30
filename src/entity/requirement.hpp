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
#include <atomic>

#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>


namespace mtconnect
{
  namespace entity
  {
    class Entity;
    class Factory;
    using EntityPtr = std::shared_ptr<Entity>;
    using EntityList = std::list<std::shared_ptr<Entity>>;
    using Vector = std::vector<double>;
    using Value = std::variant<std::monostate, EntityPtr, EntityList,
                              std::string, int64_t, double, bool,
                              Vector, nullptr_t>;
    using FactoryPtr = std::shared_ptr<Factory>;
    using ControlledVocab = std::list<std::string>;
    using Pattern = std::optional<std::regex>;
    
    enum ValueType {
      EMPTY = 0,
      ENTITY = 1,
      ENTITY_LIST = 2,
      STRING = 3,
      INTEGER = 4,
      DOUBLE = 5,
      BOOL = 6,
      VECTOR = 7,
      NULL_VALUE = 8
    };

    bool ConvertValueToType(Value &value, ValueType type);

    class EntityError : public std::logic_error
    {
    public:
      using std::logic_error::logic_error;
    };
    class PropertyError : public EntityError
    {
    public:
      using EntityError::EntityError;
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
    class PropertyValueError : public PropertyError
    {
    public:
      using PropertyError::PropertyError;
    };

    
    using ErrorList = std::list<EntityError>;
    
    struct Matcher
    {
      virtual bool matches(const std::string &s) const = 0;
    };
    
    using MatcherPtr = std::weak_ptr<Matcher>;

    class Requirement
    {
    public:
      
      const static auto Infinite { std::numeric_limits<int>::max() };

    public:
      Requirement(const std::string &name, ValueType type, bool required = true)
        : m_name(name), m_type(type), m_upperMultiplicity(1),
          m_lowerMultiplicity(required ? 1 : 0)
      {
      }
      Requirement(const std::string &name, bool required, ValueType type = STRING)
      : m_name(name), m_type(type), m_upperMultiplicity(1),
        m_lowerMultiplicity(required ? 1 : 0)
      {
      }
      Requirement(const std::string &name, ValueType type, int lower, int upper)
      : m_name(name), m_type(type), m_lowerMultiplicity(lower),
        m_upperMultiplicity(upper)
      {
      }
      Requirement(const std::string &name, ValueType type, FactoryPtr &o,
                  bool required = true);
      Requirement(const std::string &name, ValueType type, FactoryPtr &o,
                  int lower, int upper);
      Requirement(const std::string &name, const ControlledVocab &vocab,
                  bool required = true)
      : m_name(name), m_type(STRING), m_lowerMultiplicity(required ? 1 : 0),
        m_upperMultiplicity(1)
      {
        std::stringstream str;
        for (auto &s : vocab)
          str << s << "|";
        str.seekp(-1, std::ios_base::end);
        m_pattern = std::make_optional<std::regex>(str.str());
      }
      Requirement(const std::string &name, const std::regex &pattern,
                  bool required = true)
      : m_name(name), m_type(STRING), m_lowerMultiplicity(required ? 1 : 0),
        m_upperMultiplicity(1), m_pattern(pattern)
      {
      }

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
      ValueType getType() const { return m_type; }
      auto &getFactory() const { return m_factory; }
      void setFactory(FactoryPtr &f) { m_factory = f; }
      void setMultiplicity(int lower, int upper)
      {
        m_upperMultiplicity = upper;
        m_lowerMultiplicity = lower;
      }
      void makeRequired() { m_lowerMultiplicity = 1; }
            
      bool convertType(Value &v) const { return ConvertValueToType(v, m_type); }
      bool hasMatcher() const { return m_matcher.use_count() > 0; }
      bool isMetBy(const Value &value, bool isList) const;
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
      ValueType m_type;
      MatcherPtr m_matcher;
      FactoryPtr m_factory;
      Pattern m_pattern;
    };

        // Inlines
  }
}
