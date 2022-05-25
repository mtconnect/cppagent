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

#include "entity/entity.hpp"

namespace mtconnect {
  namespace pipeline {
    enum GuardAction
    {
      CONTINUE,
      RUN,
      SKIP
    };

    using Guard = std::function<GuardAction(const entity::EntityPtr entity)>;
    class GuardCls
    {
    public:
      GuardCls(GuardAction match) : m_match(match) {}
      GuardCls(const GuardCls &) = default;

      GuardAction operator()(const entity::EntityPtr entity) { return m_match; }

      void setAlternative(Guard &alt) { m_alternative = alt; }

      GuardAction check(bool matched, const entity::EntityPtr entity)
      {
        if (matched)
          return m_match;
        else if (m_alternative)
          return m_alternative(entity);
        else
          return CONTINUE;
      }

      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }

    protected:
      Guard m_alternative;
      GuardAction m_match;
    };

    template <typename... Ts>
    class TypeGuard : public GuardCls
    {
    public:
      using GuardCls::GuardCls;

      template <typename T, typename... R>
      constexpr bool match(const entity::Entity *ep)
      {
        if constexpr ((sizeof...(R)) == 0)
          return dynamic_cast<const T *>(ep);
        else
          return dynamic_cast<const T *>(ep) != nullptr || match<R...>(ep);
      }

      constexpr bool matches(const entity::EntityPtr &entity) { return match<Ts...>(entity.get()); }

      GuardAction operator()(const entity::EntityPtr entity)
      {
        return check(matches(entity), entity);
      }

      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }
    };

    template <typename... Ts>
    class ExactTypeGuard : public GuardCls
    {
    public:
      using GuardCls::GuardCls;

      template <typename T, typename... R>
      constexpr bool match(const std::type_info &ti)
      {
        if constexpr ((sizeof...(R)) == 0)
          return typeid(T) == ti;
        else
          return typeid(T) == ti || match<R...>(ti);
      }

      constexpr bool matches(const entity::EntityPtr &entity)
      {
        auto &e = *entity.get();
        auto &ti = typeid(e);
        return match<Ts...>(ti);
      }

      GuardAction operator()(const entity::EntityPtr entity)
      {
        return check(matches(entity), entity);
      }
      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }
    };

    class EntityNameGuard : public GuardCls
    {
    public:
      EntityNameGuard(const std::string &name, GuardAction match) : GuardCls(match), m_name(name) {}

      bool matches(const entity::EntityPtr &entity) { return entity->getName() == m_name; }

      GuardAction operator()(const entity::EntityPtr entity)
      {
        return check(matches(entity), entity);
      }
      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }

    protected:
      std::string m_name;
    };

    template <typename L, typename B>
    class LambdaGuard : public B
    {
    public:
      using Lambda = std::function<bool(const L &)>;

      LambdaGuard(Lambda guard, GuardAction match) : B(match), m_lambda(guard) {}
      LambdaGuard(const LambdaGuard &) = default;
      ~LambdaGuard() = default;

      bool matches(const entity::EntityPtr &entity)
      {
        bool matched = B::matches(entity);
        if (matched)
        {
          auto o = dynamic_cast<const L *>(entity.get());
          matched = o != nullptr && m_lambda(*o);
        }

        return matched;
      }

      GuardAction operator()(const entity::EntityPtr entity)
      {
        return B::check(matches(entity), entity);
      }
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
