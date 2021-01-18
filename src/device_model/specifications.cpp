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

#include "specifications.hpp"

#include <dlib/logger.h>

#include <utility>

using namespace std;

namespace mtconnect
{
  static dlib::logger g_logger("configuration.specification");
  static map<string, set<string>> g_groups(
      {{"Limits",
        {"Maximum", "Minimum", "UpperLimit", "LowerLimit", "UpperWarning", "LowerWarning",
         "Nominal"}},
       {"SpecificationLimits", {"UpperLimit", "LowerLimit", "Nominal"}},
       {"ControlLimits", {"UpperLimit", "LowerLimit", "UpperWarning", "LowerWarning", "Nominal"}},
       {"AlarmLimits", {"UpperLimit", "LowerLimit", "UpperWarning", "LowerWarning"}}});

  static map<string, set<string>> g_specifications(
      {{"Specification", {"Limits"}},
       {"ProcessSpecification", {"SpecificationLimits", "ControlLimits", "AlarmLimits"}}});

  bool Specification::addLimitForGroup(const std::string &group, const std::string &limit,
                                       double value)
  {
    if (g_specifications.count(m_class) < 1)
    {
      g_logger << dlib::LWARN << "Invalid specification class: " << m_class;
      return false;
    }

    const auto &spec = g_specifications[m_class];
    if (spec.count(group) < 1)
    {
      g_logger << dlib::LWARN << "Invalid group " << group
               << " for specification class: " << m_class;
      return false;
    }

    if (g_groups[group].count(limit) < 1)
    {
      g_logger << dlib::LWARN << "Invalid limit " << limit << " for group " << group
               << " for specification class: " << m_class;
      return false;
    }

    m_groups[group][limit] = value;
    return true;
  }
}  // namespace mtconnect
