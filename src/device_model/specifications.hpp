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

#include "component_configuration.hpp"
#include "globals.hpp"

#include <cmath>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace mtconnect
{
  class Specification
  {
  public:
    Specification(const Specification &s)
      : m_id(s.m_id),
        m_type(s.m_type),
        m_subType(s.m_subType),
        m_units(s.m_units),
        m_name(s.m_name),
        m_dataItemIdRef(s.m_dataItemIdRef),
        m_compositionIdRef(s.m_compositionIdRef),
        m_coordinateSystemIdRef(s.m_coordinateSystemIdRef),
        m_originator(s.m_originator),
        m_hasGroups(s.m_hasGroups),
        m_class(s.m_class)
    {
    }
    Specification(const std::string &klass)
    {
      m_hasGroups = klass == "ProcessSpecification";
      m_class = klass;
    }
    virtual ~Specification() = default;

  public:
    typedef std::map<std::string, double> Group;

    // Attributes
    std::string m_id;
    std::string m_type;
    std::string m_subType;
    std::string m_units;
    std::string m_name;
    std::string m_dataItemIdRef;
    std::string m_compositionIdRef;
    std::string m_coordinateSystemIdRef;
    std::string m_originator;

    bool addLimitForGroup(const std::string &group, const std::string &limit, double value);
    bool addLimit(const std::string &limit, double value)
    {
      return addLimitForGroup("Limits", limit, value);
    }

    const std::optional<Group> getGroup(const std::string &group) const
    {
      auto it = m_groups.find(group);
      if (it != m_groups.end())
        return it->second;
      else
        return {};
    }
    auto getLimits() const { return getGroup("Limits"); }
    std::set<std::string> getGroupKeys() const
    {
      std::set<std::string> keys;
      std::transform(m_groups.begin(), m_groups.end(), std::inserter(keys, keys.begin()),
                     [](const auto &kv) { return kv.first; });
      return keys;
    }

    const std::map<std::string, Group> &getGroups() const { return m_groups; }
    double getLimitForGroup(const std::string &group, const std::string &limit) const
    {
      auto it = m_groups.find(group);
      decltype(it->second.find(limit)) gi;
      if (it != m_groups.end() && (gi = it->second.find(limit)) != it->second.end())
        return gi->second;
      else
        return std::nan("1");
    }
    double getLimit(const std::string &limit) const { return getLimitForGroup("Limits", limit); }

    bool hasGroups() const { return m_hasGroups; }
    const std::string &getClass() const { return m_class; }

  protected:
    std::map<std::string, Group> m_groups;
    bool m_hasGroups;
    std::string m_class;
  };

  class Specifications : public ComponentConfiguration
  {
  public:
    Specifications() = default;
    virtual ~Specifications() = default;

    const std::list<std::unique_ptr<Specification>> &getSpecifications() const
    {
      return m_specifications;
    }
    void addSpecification(Specification *s) { m_specifications.emplace_back(std::move(s)); }
    void addSpecification(std::unique_ptr<Specification> &s)
    {
      m_specifications.emplace_back(std::move(s));
    }

  protected:
    std::list<std::unique_ptr<Specification>> m_specifications;
  };
}  // namespace mtconnect
