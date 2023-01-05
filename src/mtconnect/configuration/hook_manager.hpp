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

#include <list>
#include <optional>
#include <utility>

#include "mtconnect/config.hpp"

namespace mtconnect::configuration {
  template <typename T>
  class AGENT_LIB_API HookManager
  {
  public:
    using Hook = std::function<void(T &)>;
    using HookEntry = std::pair<std::optional<std::string>, Hook>;
    using HookList = std::list<HookEntry>;

    HookManager() {}
    ~HookManager() {}

    // Add hooks without name, cannot be removed
    void add(Hook &hook) { m_hooks.emplace_back(std::make_pair(std::nullopt, hook)); }
    void add(Hook &&hook) { m_hooks.emplace_back(std::make_pair(std::nullopt, std::move(hook))); }
    void addFirst(Hook &hook) { m_hooks.emplace_front(std::make_pair(std::nullopt, hook)); }
    void addFirst(Hook &&hook)
    {
      m_hooks.emplace_front(std::make_pair(std::nullopt, std::move(hook)));
    }

    // Add by name
    void add(std::string &name, Hook &hook)
    {
      m_hooks.emplace_back(std::make_pair(std::nullopt, hook));
    }
    void add(std::string &name, Hook &&hook)
    {
      m_hooks.emplace_back(std::make_pair(std::nullopt, std::move(hook)));
    }
    void addFirst(std::string &name, Hook &hook)
    {
      m_hooks.emplace_front(std::make_pair(std::nullopt, hook));
    }
    void addFirst(std::string &name, Hook &&hook)
    {
      m_hooks.emplace_front(std::make_pair(std::nullopt, std::move(hook)));
    }

    // Remove by name
    bool remove(const std::string &name)
    {
      auto v = m_hooks.remove_if([&name](const auto &v) { return v.first && *v.first == name; });
      return v > 0;
    }

    // Execute hooks9i
    void exec(T &obj) const
    {
      for (const auto &h : m_hooks)
        h.second(obj);
    }

  protected:
    HookList m_hooks;
  };
}  // namespace mtconnect::configuration
