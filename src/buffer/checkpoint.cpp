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

#include "checkpoint.hpp"

#include "device_model/data_item/data_item.hpp"

using namespace std;

namespace mtconnect {
  using namespace observation;
  using namespace entity;
  namespace buffer {
    Checkpoint::Checkpoint(const Checkpoint &checkpoint, const FilterSetOpt &filterSet)
    {
      FilterSetOpt filter;
      if (!filterSet && checkpoint.hasFilter())
        filter = checkpoint.m_filter;
      else
        filter = filterSet;

      copy(checkpoint, filter);
    }

    void Checkpoint::clear() { m_observations.clear(); }

    Checkpoint::~Checkpoint() { clear(); }

    void Checkpoint::addObservation(ConditionPtr event, ObservationPtr &&old)
    {
      bool assign = true;
      Condition *cond = dynamic_cast<Condition *>(old.get());
      if (cond->getLevel() != Condition::NORMAL && event->getLevel() != Condition::NORMAL &&
          cond->getLevel() != Condition::UNAVAILABLE && event->getLevel() != Condition::UNAVAILABLE)
      {
        // Check to see if the native code matches an existing
        // active condition
        if (auto e = cond->find(event->getCode()))
        {
          // Replace in chain.
          old = cond->deepCopyAndRemove(e);
          // Check if this is the only event...
          // ??
        }

        // Chain the event
        if (old)
          event->appendTo(dynamic_pointer_cast<Condition>(old));
      }
      else if (event->getLevel() == Condition::NORMAL)
      {
        // Check for a normal that clears an active condition by code
        if (!event->getCode().empty())
        {
          if (auto e = cond->find(event->getCode()))
          {
            // Clear the one condition by removing it from the chain
            old = cond->deepCopyAndRemove(e);

            if (!old)
            {
              // Need to put a normal event in with no code since this
              // is the last one.
              auto n = make_shared<Condition>(*event);
              n->normal();
              old = n;
            }
            assign = false;
          }
          else
          {
            // Not sure if we should register code specific normals if
            // previous normal was not found
            // (*ptr) = event;
          }
        }
      }
      if (assign)
        old = event;
    }

    void Checkpoint::addObservation(const DataSetEventPtr event, ObservationPtr &&old)
    {
      if (!event->isUnavailable() && !old->isUnavailable() && !event->hasProperty("resetTriggered"))
      {
        // Get the existing data set from the existing event
        DataSet set = old->getValue<DataSet>();

        // For data sets merge the maps together
        for (auto &e : event->getValue<DataSet>())
        {
          const auto &oe = set.find(e);
          if (oe != set.end())
            set.erase(oe);
          if (!e.m_removed)
            set.insert(e);
        }

        // Replace the old event with a copy of the new event with sets merged
        // Do not modify the new event.
        auto n = make_shared<DataSetEvent>(*event);
        n->setDataSet(set);
        old = n;
      }
      else
      {
        old = event;
      }
    }

    void Checkpoint::addObservation(ObservationPtr obs)
    {
      if (obs->isOrphan() || (m_filter && m_filter->count(obs->getDataItem()->getId()) == 0))
      {
        return;
      }

      auto item = obs->getDataItem();
      const auto &id = item->getId();
      auto old = m_observations.find(id);

      if (old != m_observations.end())
      {
        if (item->isCondition())
        {
          auto cond = dynamic_pointer_cast<Condition>(obs);
          // Chain event only if it is normal or unavailable and the
          // previous condition was not normal or unavailable
          addObservation(cond, std::forward<ObservationPtr>(old->second));
        }
        else if (item->isDataSet())
        {
          auto set = dynamic_pointer_cast<DataSetEvent>(obs);
          addObservation(set, std::forward<ObservationPtr>(old->second));
        }
        else
        {
          old->second = obs;
        }
      }
      else
      {
        m_observations[id] = dynamic_pointer_cast<Observation>(obs->getptr());
      }
    }

    void Checkpoint::copy(const Checkpoint &checkpoint, const FilterSetOpt &filterSet)
    {
      clear();

      if (filterSet)
      {
        m_filter = filterSet;
      }

      for (const auto &event : checkpoint.m_observations)
      {
        if (!m_filter || m_filter->count(event.first) > 0)
          m_observations[event.first] = dynamic_pointer_cast<Observation>(event.second->getptr());
      }
    }

    void Checkpoint::getObservations(ObservationList &list, const FilterSetOpt &filterSet) const
    {
      for (const auto &obs : m_observations)
      {
        auto e = obs.second;
        if (!e->isOrphan())
        {
          if (!filterSet || (e && filterSet->count(e->getDataItem()->getId()) > 0))
          {
            if (e->getDataItem()->isCondition())
            {
              for (auto ev = dynamic_pointer_cast<Condition>(e); ev; ev = ev->getPrev())
              {
                list.push_back(ev);
              }
            }
            else
            {
              list.push_back(e);
            }
          }
        }
      }
    }

    void Checkpoint::filter(const FilterSet &filterSet)
    {
      m_filter = filterSet;

      if (m_filter->empty())
        return;

      auto it = m_observations.begin();
      while (it != m_observations.end())
      {
        if (!m_filter->count(it->first))
        {
#ifdef _WINDOWS
          it = m_observations.erase(it);
#else
          auto pos = it++;
          m_observations.erase(pos);
#endif
        }
        else
        {
          ++it;
        }
      }
    }

    bool Checkpoint::dataSetDifference(ObservationPtr obs) const
    {
      if (obs->isOrphan())
        return false;

      auto setEvent = dynamic_pointer_cast<DataSetEvent>(obs);
      auto item = obs->getDataItem();
      if (item->isDataSet() && !setEvent->getDataSet().empty() &&
          !obs->hasProperty("resetTriggered"))
      {
        const auto &id = item->getId();
        const auto ptr = m_observations.find(id);

        if (ptr != m_observations.end() && !ptr->second->isUnavailable())
        {
          const auto old = dynamic_pointer_cast<DataSetEvent>(ptr->second);
          auto &set = old->getDataSet();

          DataSet eventSet = setEvent->getDataSet();
          bool changed = false;

          for (auto it = eventSet.begin(); it != eventSet.end();)
          {
            const auto v = set.find(*it);
            if (v == set.end() || !v->same(*it))
            {
              it++;
            }
            else
            {
              changed = true;
              eventSet.erase(it++);
            }
          }

          if (changed)
          {
            setEvent->setDataSet(eventSet);
          }

          return !eventSet.empty();
        }
      }

      return true;
    }

  }  // namespace buffer
}  // namespace mtconnect
