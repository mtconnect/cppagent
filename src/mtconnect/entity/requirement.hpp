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

/// @file requirement.hpp
/// @brief Entity Requirement types and classes

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
#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::entity {
  class Entity;
  using EntityPtr = std::shared_ptr<Entity>;
  using ConstEntityPtr = std::shared_ptr<const Entity>;
  /// @brief List of shared entities
  using EntityList = std::list<std::shared_ptr<Entity>>;
  /// @brief Vector of doubles
  using Vector = std::vector<double>;

  /// @brief Entity Value variant
  using Value = std::variant<std::monostate, EntityPtr, EntityList, std::string, int64_t, double,
                             bool, Vector, DataSet, Timestamp, std::nullptr_t>;

  /// @brief Value type enumeration
  enum class ValueType : std::uint16_t
  {
    EMPTY = 0x0,              ///< monostate for no value
    ENTITY = 0x1,             ///< shared entity pointer
    ENTITY_LIST = 0x2,        ///< list of entities
    STRING = 0x3,             ///< string value
    INTEGER = 0x4,            ///< int64 value
    DOUBLE = 0x5,             ///< double value
    BOOL = 0x6,               ///< bool value
    VECTOR = 0x7,             ///< Vector of doubles
    DATA_SET = 0x8,           ///< DataSet of key value pairs
    TIMESTAMP = 0x9,          ///< Timestamp in microseconds
    NULL_VALUE = 0xA,         ///< null pointer
    USTRING = 0x10 | STRING,  ///< Upper case string (represented as string)
    QSTRING = 0x20 | STRING,  ///< Qualified Name String (represented as string)
    TABLE = 0x10 | DATA_SET   ///< Table (represented as data set)
  };

  /// @brief Mask for value types
  const std::uint16_t VALUE_TYPE_BASE = 0x0F;
  constexpr const ValueType BaseValueType(const ValueType value)
  {
    return ValueType(std::uint16_t(value) & VALUE_TYPE_BASE);
  }

  class Factory;
  using FactoryPtr = std::shared_ptr<Factory>;
  using ControlledVocab = std::list<std::string>;
  using Pattern = std::optional<std::regex>;
  using VocabSet = std::optional<std::unordered_set<std::string>>;

  /// @brief Convert a `Value` to a given type
  /// @param value The value to convert
  /// @param type the target type
  /// @param table special treatment if a table (data sets of data set)
  /// @return `true` if conversion was successful
  bool AGENT_LIB_API ConvertValueToType(Value &value, ValueType type, bool table = false);

  /// @brief Error class when an error occurred
  class AGENT_LIB_API EntityError : public std::logic_error
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

    /// @brief an error related to an entity
    /// @return the error text
    virtual const char *what() const noexcept override
    {
      if (m_text.empty())
      {
        auto *t = const_cast<EntityError *>(this);
        t->m_text = m_entity + ": " + std::logic_error::what();
      }
      return m_text.c_str();
    }
    /// @brief set the entity text
    /// @param[in] s the entity text
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

  /// @brief an error related to an entity property
  class AGENT_LIB_API PropertyError : public EntityError
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

  /// @brief A simple class that can be subclassed to customize matching
  struct Matcher
  {
    virtual ~Matcher() = default;
    virtual bool matches(const std::string &s) const = 0;
  };

  using MatcherPtr = std::weak_ptr<Matcher>;

  /// @brief A requirement for a an entity property
  class AGENT_LIB_API Requirement
  {
  public:
    /// @brief tag to use for limitless occurrences
    const static auto Infinite {std::numeric_limits<int>::max()};

  public:
    /// @brief property requirement with a type that can be optional
    /// @param name the property key
    /// @param type the data type
    /// @param required `true` if the property is required
    Requirement(const std::string &name, ValueType type, bool required = true)
      : m_name(name), m_upperMultiplicity(1), m_lowerMultiplicity(required ? 1 : 0), m_type(type)
    {}
    /// @brief property requirement with a type that can be optional
    /// @param name the property key
    /// @param required `true` if the property is required
    /// @param type the data type defaulted to `STRING`
    Requirement(const std::string &name, bool required, ValueType type = ValueType::STRING)
      : m_name(name), m_upperMultiplicity(1), m_lowerMultiplicity(required ? 1 : 0), m_type(type)
    {}
    /// @brief property that can occur mode than once
    /// @param name the property key
    /// @param type they data type
    /// @param lower a lower bound occurrence
    /// @param upper an upper bound occurrence
    Requirement(const std::string &name, ValueType type, int lower, int upper)
      : m_name(name), m_upperMultiplicity(upper), m_lowerMultiplicity(lower), m_type(type)
    {}
    /// @brief property with required vector size
    /// @param name the property key
    /// @param type the data type
    /// @param size the size of the value
    /// @param required `true` if the property is required
    Requirement(const std::string &name, ValueType type, int size, bool required = true)
      : m_name(name),
        m_upperMultiplicity(1),
        m_lowerMultiplicity(required ? 1 : 0),
        m_size(size),
        m_type(type)
    {}
    /// @brief property requirement for an entity or entity list with a factory
    /// @param name the property key
    /// @param type the data type. `ENTITY` or `ENTITY_LIST`
    /// @param o the entity factory
    /// @param required `true` if the property is required
    Requirement(const std::string &name, ValueType type, FactoryPtr o, bool required = true);
    /// @brief property requirement for an entity or entity list
    /// @param name the property key
    /// @param type the data type. `ENTITY` or `ENTITY_LIST`
    /// @param o the entity factory
    /// @param lower lower bound for multiplicity
    /// @param upper upper bound for multiplicity
    Requirement(const std::string &name, ValueType type, FactoryPtr o, int lower, int upper);
    /// @brief property requirement for a string value that must match a controlled vocabulary
    /// @param name the property key
    /// @param vocab the set of possible values
    /// @param required `true` if the property is required
    Requirement(const std::string &name, const ControlledVocab &vocab, bool required = true)
      : m_name(name),
        m_upperMultiplicity(1),
        m_lowerMultiplicity(required ? 1 : 0),
        m_type(ValueType::STRING)
    {
      m_vocabulary.emplace();
      for (auto &s : vocab)
        m_vocabulary->emplace(s);
    }
    /// @brief propery requirement where the text must match a regex pattern
    /// @param name the property key
    /// @param pattern the regex
    /// @param required `true` if the property is required
    Requirement(const std::string &name, const std::regex &pattern, bool required = true)
      : m_name(name),
        m_upperMultiplicity(1),
        m_lowerMultiplicity(required ? 1 : 0),
        m_type(ValueType::STRING),
        m_pattern(pattern)
    {}

    Requirement() = default;
    Requirement(const Requirement &o) = default;
    ~Requirement() = default;

    /// @brief Check if two requires are the same
    /// @param o another requirement
    /// @return `true` if they are equal
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

    /// @brief gets required state
    /// @return `true` if property is required
    bool isRequired() const { return m_lowerMultiplicity > 0; }
    /// @brief get optional state
    /// @return `true` if property is options
    bool isOptional() const { return !isRequired(); }
    /// @brief gets the upper multiplicity
    /// @return upper multiplicity
    int getUpperMultiplicity() const { return m_upperMultiplicity; }
    /// @brief gets the lower multiplicity
    /// @return lower multiplicity
    int getLowerMultiplicity() const { return m_lowerMultiplicity; }
    /// @brief gets the size if set
    /// @return the size
    std::optional<int> getSize() const { return m_size; }
    /// @brief gets the matcher
    /// @return the matcher
    const auto &getMatcher() const { return m_matcher; }
    /// @brief sets the matcher for this requirement
    /// @param[in] m a shared pointer to the matcher
    void setMatcher(MatcherPtr m) { m_matcher = m; }
    /// @brief gets the name of the requirement
    /// @return the name for the property key
    const std::string &getName() const { return m_name; }
    /// @brief gets the value type for the requirement
    /// @return the value type
    ValueType getType() const { return m_type; }
    /// @brief gets the factory for elements and element lists
    /// @return the factory
    auto &getFactory() const { return m_factory; }
    /// @brief sets the factory for an entity and entity list
    /// @param f the factory
    void setFactory(FactoryPtr &f) { m_factory = f; }
    /// @brief  set the multiplicity
    /// @param lower the upper multiplicity
    /// @param upper the lower multiplicity
    void setMultiplicity(int lower, int upper)
    {
      m_upperMultiplicity = upper;
      m_lowerMultiplicity = lower;
    }
    /// @brief makes this requirement required
    void makeRequired() { m_lowerMultiplicity = 1; }

    /// @brief convert a given value to the requirement type
    /// @param v the value
    /// @param table if this is a table conversion
    /// @return `true` if it is successful
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
    /// @brief does this have a given matcher
    /// @return `true` if it has a matcher
    bool hasMatcher() const { return m_matcher.use_count() > 0; }
    /// @brief does a value meet the requirement
    /// @param value the value
    /// @return `true` if the requirement is met
    bool isMetBy(const Value &value) const;
    /// @brief checks if a string matches the requirement
    /// @param s the string to check
    /// @return `true` if it matches
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

}  // namespace mtconnect::entity
