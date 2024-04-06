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

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"

namespace mtconnect {
  namespace pipeline {
    /// @brief Actions for taken for the guard
    enum GuardAction
    {
      CONTINUE,  ///< Continue on to the next transform in the list
      RUN,       ///< Run this transform
      SKIP       ///< Skip the transform and move to the next
    };

    /// @brief Guard is a lambda function returning a `GuardAction` taking an entity
    using Guard = std::function<GuardAction(const entity::Entity *entity)>;

    /// @brief A simple GuardClass returning a simple match
    ///
    /// allows for chaining of guards
    class GuardCls
    {
    public:
      /// @brief Construct a GuardCls
      /// @param match the action if matched
      GuardCls(GuardAction action) : m_action(action) {}
      GuardCls(const GuardCls &) = default;

      GuardAction operator()(const entity::Entity *entity) { return m_action; }

      /// @brief set the alternative guard
      /// @param alt alternative
      void setAlternative(Guard &alt) { m_alternative = alt; }

      /// @brief check the matched state and if matched then return action.
      /// @param matched if `true` return the action otherwise check an alternative
      /// @param entity an entity
      /// @return the guard action
      GuardAction check(bool matched, const entity::Entity *entity)
      {
        if (matched)
          return m_action;
        else if (m_alternative)
          return m_alternative(entity);
        else
          return CONTINUE;
      }

      /// @brief set the alternative to the other
      /// @param other a guard
      /// @return this
      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }
      /// @brief Set the alternative to a static action
      /// @param other the guard action
      /// @return this
      auto &operator||(GuardAction other)
      {
        m_alternative = GuardCls(other);
        return *this;
      }

    protected:
      Guard m_alternative;
      GuardAction m_action;
    };

    /// @brief A guard that checks if the entity is one of the types or sub-types
    /// @tparam ...Ts the list of types
    template <typename... Ts>
    class TypeGuard : public GuardCls
    {
    public:
      using GuardCls::GuardCls;

      /// @brief recursive match
      ///
      /// Uses dynamic cast to check if entity can be cast as one of the types
      /// @tparam T the type
      /// @tparam ...R the rest of the types
      /// @param ti the type info we're checking
      /// @return `true` if matches
      template <typename T, typename... R>
      constexpr bool match(const entity::Entity *ep)
      {
        if constexpr ((sizeof...(R)) == 0)
          return dynamic_cast<const T *>(ep);
        else
          return dynamic_cast<const T *>(ep) != nullptr || match<R...>(ep);
      }

      /// @brief constexpr expanded type match
      /// @param entity the entity
      /// @return `true` if matches
      constexpr bool matches(const entity::Entity *entity) { return match<Ts...>(entity); }

      /// @brief Check if the entity matches one of the types
      /// @param[in] entity pointer to the entity
      /// @returns the actionn to take if the types match
      GuardAction operator()(const entity::Entity *entity)
      {
        return check(matches(entity), entity);
      }

      /// @brief set the alternative action if this guard does not match
      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }
    };

    /// @brief A guard that checks if the entity that matches one of the types
    /// @tparam ...Ts the list of types
    template <typename... Ts>
    class ExactTypeGuard : public GuardCls
    {
    public:
      using GuardCls::GuardCls;

      /// @brief recursive match
      /// @tparam T the type
      /// @tparam ...R the rest of the types
      /// @param ti the type info we're checking
      /// @return `true` if matches
      template <typename T, typename... R>
      constexpr bool match(const std::type_info &ti)
      {
        if constexpr ((sizeof...(R)) == 0)
          return typeid(T) == ti;
        else
          return typeid(T) == ti || match<R...>(ti);
      }

      /// @brief constexpr expanded type match
      /// @param entity the entity
      /// @return `true` if matches
      constexpr bool matches(const entity::Entity *entity)
      {
        auto &e = *entity;
        auto &ti = typeid(e);
        return match<Ts...>(ti);
      }

      /// @brief Check if the entity exactly matches one of the types
      /// @param[in] entity pointer to the entity
      /// @returns the action to take if the types match
      GuardAction operator()(const entity::Entity *entity)
      {
        return check(matches(entity), entity);
      }

      /// @brief set the alternative action if this guard does not match
      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }
    };

    /// @brief Match on the entity name
    class EntityNameGuard : public GuardCls
    {
    public:
      EntityNameGuard(const std::string &name, GuardAction match) : GuardCls(match), m_name(name) {}

      bool matches(const entity::Entity *entity) { return entity->getName() == m_name; }

      /// @brief Check if the entity name matches
      /// @param[in] entity pointer to the entity
      /// @returns the action to take if the types match
      GuardAction operator()(const entity::Entity *entity)
      {
        return check(matches(entity), entity);
      }

      /// @brief set the alternative action if this guard does not match
      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }

    protected:
      std::string m_name;
    };

    /// @brief Use a lambda expression to match lambda
    /// @tparam L lamba argument type
    /// @tparam B class with a matches method to match the entity
    template <typename L, typename B>
    class LambdaGuard : public B
    {
    public:
      using Lambda = std::function<bool(const L &)>;

      /// @brief Construct a lambda guard with a function returning bool and an action
      /// @param guard the lambda function
      /// @param match the action if the lambda returns true
      LambdaGuard(Lambda guard, GuardAction match) : B(match), m_lambda(guard) {}
      LambdaGuard(const LambdaGuard &) = default;
      ~LambdaGuard() = default;

      /// @brief call the `B::matches()` method with the entity
      /// @param entity the entity
      /// @return `true` if matched
      bool matches(const entity::Entity *entity)
      {
        bool matched = B::matches(entity);
        if (matched)
        {
          auto o = dynamic_cast<const L *>(entity);
          matched = o != nullptr && m_lambda(*o);
        }

        return matched;
      }

      /// @brief Check if the entity name matches the base guard and the lambda
      /// @param[in] entity pointer to the entity
      /// @returns the action to take if the types match
      GuardAction operator()(const entity::Entity *entity)
      {
        return B::check(matches(entity), entity);
      }

      /// @brief set the alternative action if this guard does not match
      auto &operator||(Guard other)
      {
        B::m_alternative = other;
        return *this;
      }

    protected:
      Lambda m_lambda;
    };

  }  // namespace pipeline
}  // namespace mtconnect
