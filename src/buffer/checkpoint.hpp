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

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "observation/observation.hpp"
#include "utilities.hpp"

namespace mtconnect::buffer {
  class Checkpoint
  {
  public:
    Checkpoint() = default;
    Checkpoint(const Checkpoint &checkpoint, const FilterSetOpt &filterSet = std::nullopt);
    ~Checkpoint();

    void addObservation(observation::ObservationPtr event);
    bool dataSetDifference(observation::ObservationPtr event) const;
    void copy(Checkpoint const &checkpoint, const FilterSetOpt &filterSet = std::nullopt);
    void clear();
    void filter(const FilterSet &filterSet);
    bool hasFilter() const { return bool(m_filter); }

    const std::unordered_map<std::string, observation::ObservationPtr> &getObservations() const
    {
      return m_observations;
    }

    void updateDataItems(std::unordered_map<std::string, WeakDataItemPtr> &diMap)
    {
      for (auto &o : m_observations)
      {
        o.second->updateDataItem(diMap);
      }
    }

    void getObservations(observation::ObservationList &list,
                         const FilterSetOpt &filter = std::nullopt) const;

    observation::ObservationPtr getObservation(const std::string &id)
    {
      auto pos = m_observations.find(id);
      if (pos != m_observations.end())
        return pos->second;
      return nullptr;
    }

  protected:
    void addObservation(observation::ConditionPtr event, observation::ObservationPtr &&old);
    void addObservation(const observation::DataSetEventPtr event,
                        observation::ObservationPtr &&old);

  protected:
    std::unordered_map<std::string, observation::ObservationPtr> m_observations;
    FilterSetOpt m_filter;
  };
}  // namespace mtconnect::buffer
