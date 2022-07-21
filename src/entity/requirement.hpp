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

#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "data_set.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace entity {
    class Entity;
    using EntityPtr = std::shared_ptr<Entity>;
    using EntityList = std::list<std::shared_ptr<Entity>>;
    using Vector = std::vector<double>;

    using Value = std::variant<std::monostate, EntityPtr, EntityList, std::string, int64_t, double,
                               bool, Vector, DataSet, Timestamp, std::nullptr_t>;

    enum ValueType : int16_t
    {
      EMPTY = 0x0,
      ENTITY = 0x1,
      ENTITY_LIST = 0x2,
      STRING = 0x3,
      INTEGER = 0x4,
      DOUBLE = 0x5,
      BOOL = 0x6,
      VECTOR = 0x7,
      DATA_SET = 0x8,
      TIMESTAMP = 0x9,
      NULL_VALUE = 0xA,
      USTRING = 0x10 | STRING,
      QSTRING = 0x20 | STRING,
      TABLE = 0x10 | DATA_SET
    };

    const int16_t VALUE_TYPE_BASE = 0x0F;

    class Factory;
    using FactoryPtr = std::shared_ptr<Factory>;
    using ControlledVocab = std::list<std::string>;
    using Pattern = std::optional<std::regex>;
    using VocabSet = std::optional<std::unordered_set<std::string>>;

    bool ConvertValueToType(Value &value, ValueType type, bool table = false);

    class EntityError : public std::logic_error
    {
    public:
      explicit EntityError(const std::string &s, const std::string &e = "")
        : std::logic_error(s), m_entity(e)
      {}

      explicit EntityError(const char *s, const std::string &e = "")
        : std::logic_error(s), m_entity(e)
      {}

      EntityError(const EntityError &o) noexcept : std::logic_error(o), m_entity(o.m_entity) {}
      ~EntityError() override = default;

      virtual const char *what() const noexcept override
      {
        if (m_text.empty())
        {
          auto *t = const_cast<EntityError *>(this);
          t->m_text = m_entity + ": " + std::logic_error::what();
        }
        return m_text.c_str();
      }
      void setEntity(const std::string &s)
      {
        m_text.clear();
        m_entity = s;
      }
      virtual EntityError *dup() const noexcept { return new EntityError(*this); }
      const std::string &getEntity() const { return m_entity; }

    protected:
      std::string m_text;
      std::string m_entity;
    };
    class PropertyError : public EntityError
    {
    public:
      explicit PropertyError(const std::string &s, const std::string &p = "",
                             const std::string &e = "")
        : EntityError(s, e), m_property(p)
      {}

      explicit PropertyError(const char *s, const std::string &p = "", const std::string &e = "")
        : EntityError(s, e), m_property(p)
      {}

      PropertyError(const PropertyError &o) noexcept : EntityError(o), m_property(o.m_property) {}
      ~PropertyError() override = default;

      virtual const char *what() const noexcept override
      {
        if (m_text.empty())
        {
          auto *t = const_cast<PropertyError *>(this);
          t->m_text = m_entity + "(" + m_property + "): " + std::logic_error::what();
        }
        return m_text.c_str();
      }
      void setProperty(const std::string &p)
      {
        m_text.clear();
        m_property = p;
      }
      EntityError *dup() const noexcept override { return new PropertyError(*this); }
      const std::string &getProperty() const { return m_property; }

    protected:
      std::string m_property;
    };

    using ErrorList = std::list<std::unique_ptr<EntityError>>;

    struct Matcher
    {
      virtual ~Matcher() = default;
      virtual bool matches(const std::string &s) const = 0;
    };

    using MatcherPtr = std::weak_ptr<Matcher>;

    class Requirement
    {
    public:
      const static auto Infinite {std::numeric_limits<int>::max()};

    public:
      Requirement(const std::string &name, ValueType type, bool required = true)
        : m_name(name), m_upperMultiplicity(1), m_lowerMultiplicity(required ? 1 : 0), m_type(type)
      {}
      Requirement(const std::string &name, bool required, ValueType type = STRING)
        : m_name(name), m_upperMultiplicity(1), m_lowerMultiplicity(required ? 1 : 0), m_type(type)
      {}
      Requirement(const std::string &name, ValueType type, int lower, int upper)
        : m_name(name), m_upperMultiplicity(upper), m_lowerMultiplicity(lower), m_type(type)
      {}
      Requirement(const std::string &name, ValueType type, int size, bool required = true)
        : m_name(name),
          m_upperMultiplicity(1),
          m_lowerMultiplicity(required ? 1 : 0),
          m_size(size),
          m_type(type)
      {}
      Requirement(const std::string &name, ValueType type, FactoryPtr o, bool required = true);
      Requirement(const std::string &name, ValueType type, FactoryPtr o, int lower, int upper);
      Requirement(const std::string &name, const ControlledVocab &vocab, bool required = true)
        : m_name(name),
          m_upperMultiplicity(1),
          m_lowerMultiplicity(required ? 1 : 0),
          m_type(STRING)
      {
        m_vocabulary.emplace();
        for (auto &s : vocab)
          m_vocabulary->emplace(s);
      }
      Requirement(const std::string &name, const std::regex &pattern, bool required = true)
        : m_name(name),
          m_upperMultiplicity(1),
          m_lowerMultiplicity(required ? 1 : 0),
          m_type(STRING),
          m_pattern(pattern)
      {}

      Requirement() = default;
      Requirement(const Requirement &o) = default;
      ~Requirement() = default;

      Requirement &operator=(const Requirement &o)
      {
        m_type = o.m_type;
        m_lowerMultiplicity = o.m_lowerMultiplicity;
        m_upperMultiplicity = o.m_upperMultiplicity;
        m_factory = o.m_factory;
        m_matcher = o.m_matcher;
        m_size = o.m_size;
        return *this;
      }

      bool isRequired() const { return m_lowerMultiplicity > 0; }
      bool isOptional() const { return !isRequired(); }
      int getUpperMultiplicity() const { return m_upperMultiplicity; }
      int getLowerMultiplicity() const { return m_lowerMultiplicity; }
      std::optional<int> getSize() const { return m_size; }
      const auto &getMatcher() const { return m_matcher; }
      void setMatcher(MatcherPtr m) { m_matcher = m; }
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

      bool convertType(Value &v, bool table = false) const
      {
        try
        {
          return ConvertValueToType(v, m_type, table);
        }
        catch (PropertyError &e)
        {
          e.setProperty(m_name);
          throw e;
        }
        return false;
      }
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
      std::optional<int> m_size;
      ValueType m_type;
      MatcherPtr m_matcher;
      FactoryPtr m_factory;
      Pattern m_pattern;
      VocabSet m_vocabulary;
    };

    // Inlines
  }  // namespace entity
}  // namespace mtconnect
