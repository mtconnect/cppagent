//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

namespace mtconnect
{
  namespace pipeline
  {
    enum GuardAction {
      DISPARATE,
      RUN,
      SKIP
    };

    using Guard = std::function<GuardAction(const entity::EntityPtr entity)>;
    
    class _TypeGuard
    {
    public:
      _TypeGuard(GuardAction match = RUN)
      : m_match(match)
      {
      }
      _TypeGuard(const _TypeGuard &) = default;

      GuardAction operator()(const entity::EntityPtr entity)
      {
        return DISPARATE;
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
    
    template<typename ... Ts>
    class TypeGuard : public _TypeGuard
    {
    public:
      using _TypeGuard::_TypeGuard;
      
      template<typename T, typename ... R>
      constexpr bool match(const entity::Entity *ep)
      {
        if constexpr ((sizeof...(R)) == 0)
          return dynamic_cast<const T*>(ep);
        else
          return dynamic_cast<const T*>(ep) != nullptr || match<R ...>(ep);
      }
      
      GuardAction operator()(const entity::EntityPtr entity)
      {
        if (match<Ts...>(entity.get()))
          return m_match;
        else if (m_alternative)
          return m_alternative(entity);
        else
          return DISPARATE;
      }
      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }
    };
    
    template<typename ... Ts>
    class ExactTypeGuard : public _TypeGuard
    {
    public:
      using _TypeGuard::_TypeGuard;
      
      template<typename T, typename ... R>
      constexpr bool match(const std::type_info &ti)
      {
        if constexpr ((sizeof...(R)) == 0)
          return typeid(T) == ti;
        else
          return typeid(T) == ti || match<R ...>(ti);
      }
      
      GuardAction operator()(const entity::EntityPtr entity)
      {
        auto &e = *entity.get();
        auto &ti = typeid(e);
        if (match<Ts...>(ti))
          return m_match;
        else if (m_alternative)
          return m_alternative(entity);
        else
          return DISPARATE;
      }
      auto &operator||(Guard other)
      {
        m_alternative = other;
        return *this;
      }
    };

  }
}
