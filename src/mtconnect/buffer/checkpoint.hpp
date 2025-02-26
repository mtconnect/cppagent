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

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "mtconnect/config.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/utilities.hpp"

/// @brief Internal storage of observations
namespace mtconnect::buffer {
  /// @brief A point in time snapshot of all data items with a optional filter
  class AGENT_LIB_API Checkpoint
  {
  public:
    /// @brief create an empty checkpoint
    Checkpoint() = default;

    /// @brief Copy constructor for a checkpoint
    /// @param[in] checkpoint the previous checkpoint
    /// @param[in] filterSet an optional set of data item ids for filtering
    Checkpoint(const Checkpoint &checkpoint, const FilterSetOpt &filterSet = std::nullopt);
    ~Checkpoint();

    /// @brief Add an observation to the checkpoint
    /// @param[in] observation an observation
    void addObservation(observation::ObservationPtr observation);

    /// @brief If this is a data set event, diff the value
    /// @param[in] observation the data set observation
    /// @param[in] old the previous value of the data set
    /// @return The observation or a copy  if the data set changed
    observation::ObservationPtr dataSetDifference(
        const observation::ObservationPtr &observation,
        const observation::ConstObservationPtr &old) const;

    /// @brief Checks if the observation is a duplicate with existing observations
    /// @param[in] obs the observation
    /// @return an observation, possibly changed if it is not a duplicate. `nullptr` if it is a
    /// duplicate..
    const observation::ObservationPtr checkDuplicate(const observation::ObservationPtr &obs) const
    {
      using namespace observation;
      using namespace std;

      auto di = obs->getDataItem();
      const auto &id = di->getId();
      auto old = m_observations.find(id);

      if (old != m_observations.end())
      {
        auto &oldObs = old->second;
        // Filter out unavailable duplicates, only allow through changed
        // state. If both are unavailable, disregard.
        if (obs->isUnavailable() != oldObs->isUnavailable())
          return obs;
        else if (obs->isUnavailable())
          return nullptr;

        if (di->isCondition())
        {
          auto *cond = dynamic_cast<Condition *>(obs.get());
          auto *oldCond = dynamic_cast<Condition *>(oldObs.get());

          // Check for normal resetting all conditions. If there are
          // no active conditions, then this is a duplicate normal
          if (cond->getLevel() == Condition::NORMAL && cond->getCode().empty())
          {
            if (oldCond->getLevel() == Condition::NORMAL && oldCond->getCode().empty())
              return nullptr;
            else
              return obs;
          }

          // If there is already an active condition with this code,
          // then check if nothing has changed between activations.
          if (const auto &e = oldCond->find(cond->getCode()))
          {
            if (cond->getLevel() != e->getLevel())
              return obs;

            if ((cond->hasValue() != e->hasValue()) ||
                (cond->hasValue() && cond->getValue() != e->getValue()))
              return obs;

            if ((cond->hasProperty("qualifier") != e->hasProperty("qualifier")) ||
                (cond->hasProperty("qualifier") &&
                 cond->get<string>("qualifier") != e->get<string>("qualifier")))
              return obs;

            if ((cond->hasProperty("nativeSeverity") != e->hasProperty("nativeSeverity")) ||
                (cond->hasProperty("nativeSeverity") &&
                 cond->get<string>("nativeSeverity") != e->get<string>("nativeSeverity")))
              return obs;

            return nullptr;
          }
          else if (cond->getLevel() == Condition::NORMAL)
          {
            return nullptr;
          }
          else
          {
            return obs;
          }
        }
        else if (!di->isDiscrete())
        {
          if (di->isDataSet())
          {
            return dataSetDifference(obs, oldObs);
          }
          else
          {
            auto &value = obs->getValue();
            auto &oldValue = oldObs->getValue();

            if (value == oldValue)
              return nullptr;
            else
              return obs;
          }
        }
      }
      return obs;
    }

    /// @brief copy another checkpoint to this checkpoint
    /// @param[in] checkpoint a checkpoint to copy
    /// @param[in] filterSet an optional filter set
    void copy(Checkpoint const &checkpoint, const FilterSetOpt &filterSet = std::nullopt);

    /// @brief clear the contents of this checkpoint
    void clear();

    /// @brief Add a filter to the checkpoint
    void filter(const FilterSet &filterSet);
    /// @brief does this checkpoint have a filter?
    /// @return `true` if a checkpoint exists
    bool hasFilter() const { return bool(m_filter); }

    /// @brief get a map of data item id to observation shared pointers
    /// @return a map of ids to observations
    const std::unordered_map<std::string, observation::ObservationPtr> &getObservations() const
    {
      return m_observations;
    }

    /// @brief updates the data item reference of an observation in a checkpoint
    ///
    /// Used when the device model is modified and data items may have been removed or
    /// changed. The new data item shared pointer will replace the old.
    ///
    /// @param[in] diMap the map of data ids to data item pointers
    void updateDataItems(std::unordered_map<std::string, WeakDataItemPtr> &diMap)
    {
      auto iter = m_observations.begin();
      while (iter != m_observations.end())
      {
        auto item = *iter;
        if (item.second->isOrphan())
        {
          iter = m_observations.erase(iter);
        }
        else
        {
          item.second->updateDataItem(diMap);
          iter++;
        }
      }
    }

    /// @brief Get a list of observations from the checkpoint
    /// @param[in,out] list the list to add the observations to
    /// @param[in] filter an optional filter for the observations
    void getObservations(observation::ObservationList &list,
                         const FilterSetOpt &filter = std::nullopt) const;

    /// @brief Get an observation for a data item id
    /// @param[in] id the data item id
    /// @return shared pointer to the observation if it exists
    observation::ObservationPtr getObservation(const std::string &id) const
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
