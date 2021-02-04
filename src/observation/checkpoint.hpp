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

#include "globals.hpp"
#include "observation.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace mtconnect
{
  namespace observation
  {
    using FilterSet = std::set<std::string>;
    using FilterSetOpt = std::optional<FilterSet>;

    class Checkpoint
    {
    public:
      Checkpoint() = default;
      Checkpoint(const Checkpoint &checkpoint, const FilterSetOpt &filterSet = std::nullopt);
      ~Checkpoint();

      void addObservation(ObservationPtr event);
      bool dataSetDifference(ObservationPtr event) const;
      void copy(Checkpoint const &checkpoint, const FilterSetOpt &filterSet = std::nullopt);
      void clear();
      void filter(const FilterSet &filterSet);
      bool hasFilter() const { return bool(m_filter); }

      const std::map<std::string, ObservationPtr> &getObservations() const { return m_observations; }

      void getObservations(ObservationList &list, const FilterSetOpt &filter = std::nullopt) const;

      ObservationPtr getEventPtr(const std::string &id)
      {
        auto pos = m_observations.find(id);
        if (pos != m_observations.end())
          return pos->second;
        return nullptr;
      }

    protected:
      void addObservation(ConditionPtr event, ObservationPtr &&old);
      void addObservation(const DataSetEventPtr event, ObservationPtr &&old);

    protected:
      std::map<std::string, ObservationPtr> m_observations;
      FilterSetOpt m_filter;
    };
  }  // namespace observation
}  // namespace mtconnect
