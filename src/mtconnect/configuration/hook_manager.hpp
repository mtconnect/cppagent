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

#include <list>
#include <optional>
#include <utility>

#include "mtconnect/config.hpp"

namespace mtconnect::configuration {

  /// @brief Manages a list of callbacks. Callbacks can be named.
  /// @tparam The type of the object reference to pass to the callback.
  template <typename T>
  class HookManager
  {
  public:
    using Hook = std::function<void(T &)>;
    using HookEntry = std::pair<std::optional<std::string>, Hook>;
    using HookList = std::list<HookEntry>;

    /// @brief Create a hook manager
    HookManager() {}
    ~HookManager() {}

    /// @brief Add a hook to the end of the list as a rvalue without a name
    /// @param[in] hook The callback
    void add(Hook &hook) { m_hooks.emplace_back(std::make_pair(std::nullopt, hook)); }
    /// @brief Add a hook to the end of the list as a lvalue without a name
    /// @param[in] hook The callback
    void add(Hook &&hook) { m_hooks.emplace_back(std::make_pair(std::nullopt, std::move(hook))); }
    /// @brief Add a hook to the beginning of the list as a rvalue without a name
    /// @param[in] hook The callback
    void addFirst(Hook &hook) { m_hooks.emplace_front(std::make_pair(std::nullopt, hook)); }
    /// @brief Add a hook to the beginning of the list as a lvalue without a name
    /// @param[in] hook The callback
    void addFirst(Hook &&hook)
    {
      m_hooks.emplace_front(std::make_pair(std::nullopt, std::move(hook)));
    }

    /// @brief Add a hook to the end of the list as a rvalue
    /// @param[in] name The name of the callback
    /// @param[in] hook The callback
    void add(std::string &name, Hook &hook)
    {
      m_hooks.emplace_back(std::make_pair(std::nullopt, hook));
    }
    /// @brief Add a hook to the end of the list as a rvalue
    /// @param[in] name The name of the callback
    /// @param[in] hook The callback
    void add(std::string &name, Hook &&hook)
    {
      m_hooks.emplace_back(std::make_pair(std::nullopt, std::move(hook)));
    }
    /// @brief Add a hook to the beginning of the list as a rvalue
    /// @param[in] name The name of the callback
    /// @param[in] hook The callback
    void addFirst(std::string &name, Hook &hook)
    {
      m_hooks.emplace_front(std::make_pair(std::nullopt, hook));
    }
    /// @brief Add a hook to the beginning of the list as a lvalue
    /// @param[in] name The name of the callback
    /// @param[in] hook The callback
    void addFirst(std::string &name, Hook &&hook)
    {
      m_hooks.emplace_front(std::make_pair(std::nullopt, std::move(hook)));
    }

    /// @brief remove a named callback from the list
    bool remove(const std::string &name)
    {
      auto v = m_hooks.remove_if([&name](const auto &v) { return v.first && *v.first == name; });
      return v > 0;
    }

    /// @brief call each of the hooks in order with an object
    /// @param obj the object to pass to each callback
    void exec(T &obj) const
    {
      for (const auto &h : m_hooks)
        h.second(obj);
    }

  protected:
    HookList m_hooks;
  };
}  // namespace mtconnect::configuration
