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

#include "observation/observation.hpp"
#include "transform.hpp"

namespace mtconnect {
  class Agent;
  namespace pipeline {
    class PeriodFilter : public Transform
    {
    public:
      struct LastObservation
      {
        LastObservation(std::chrono::milliseconds p, boost::asio::io_context::strand &st)
          : m_timer(st.context()), m_period(p)
        {}
        ~LastObservation() { m_timer.cancel(); }

        Timestamp m_timestamp;
        observation::ObservationPtr m_observation;
        boost::asio::steady_timer m_timer;
        std::chrono::milliseconds m_period;
      };

      using LastObservationMap = std::unordered_map<std::string, LastObservation>;
      using LastObservationIterator = LastObservationMap::iterator;

      struct State : TransformState
      {
        LastObservationMap m_lastObservation;
      };

      PeriodFilter(PipelineContextPtr context, boost::asio::io_context::strand &st)
        : Transform("PeriodFilter"),
          m_state(context->getSharedState<State>(m_name)),
          m_contract(context->m_contract.get()),
          m_strand(st)
      {
        using namespace observation;
        constexpr static auto lambda = [](const Observation &s) {
          return bool(s.getDataItem()->getMinimumPeriod());
        };
        m_guard = LambdaGuard<Observation, TypeGuard<Event, Sample>>(lambda, RUN) ||
                  TypeGuard<Observation>(SKIP);
      }
      ~PeriodFilter() override = default;

      const entity::EntityPtr operator()(const entity::EntityPtr entity) override
      {
        using namespace std;
        using namespace observation;
        using namespace entity;

        {
          std::lock_guard<TransformState> guard(*m_state);

          auto o = std::dynamic_pointer_cast<Observation>(entity);
          auto di = o->getDataItem();
          auto &id = di->getId();

          if (o->isUnavailable())
          {
            m_state->m_lastObservation.erase(id);
          }
          else
          {
            auto ts = o->getTimestamp();

            auto last = m_state->m_lastObservation.find(id);
            if (last == m_state->m_lastObservation.end())
            {
              auto period =
                  chrono::milliseconds(static_cast<int64_t>(*di->getMinimumPeriod() * 1000.0));
              auto res = m_state->m_lastObservation.try_emplace(id, period, m_strand);
              if (res.second)
                last = res.first;
              else
              {
                LOG(error) << "PeriodFilter cannot create last observation";
                return EntityPtr();
              }
            }

            // If filtered, return an empty entity.
            if (filtered(last->second, id, o, ts))
              return EntityPtr();
          }
        }

        return next(entity);
      }

    protected:
      // Returns true if the observation is filtered.
      bool filtered(LastObservation &last, const std::string &id,
                    observation::ObservationPtr observation, const Timestamp &ts)
      {
        using namespace std;

        auto lv = last.m_timestamp;
        auto delta = ts - lv;
        if (delta.count() > 0 && delta < last.m_period)
        {
          bool observed = bool(last.m_observation);
          last.m_observation = observation;

          // If we have not already observed something for this data item,
          // set a timer, otherwise the current observation will replace the last
          // and be triggered when the timer expires.
          if (!observed)
          {
            // Set timer for duration seconds to send the latest obsrvation
            using boost::placeholders::_1;

            // Set the timer to expire in the remaining time left in the period
            last.m_timer.expires_from_now(last.m_period - delta);
            last.m_timer.async_wait(boost::asio::bind_executor(
                m_strand, boost::bind(&PeriodFilter::sendObservation, this, id, _1)));
          }

          return true;
        }
        else
        {
          // Check if there was an observation queued for the timer. Clear the
          // observation and cancel the timer.
          if (last.m_observation)
          {
            last.m_timer.cancel();
            last.m_observation.reset();
          }

          // Set the timestamp of the last observation.
          last.m_timestamp = ts;

          return false;
        }
      }

      void sendObservation(const std::string id, boost::system::error_code ec)
      {
        using namespace std;
        using namespace observation;

        ObservationPtr obs;
        {
          std::lock_guard<TransformState> guard(*m_state);

          // Find the entry for this data item and make sure there is an observation
          auto last = m_state->m_lastObservation.find(id);
          if (last != m_state->m_lastObservation.end() && last->second.m_observation)
          {
            last->second.m_observation.swap(obs);
          }
        }

        // Send the observation onward
        if (obs)
        {
          next(obs);
        }
      }

    protected:
      std::shared_ptr<State> m_state;
      PipelineContract *m_contract;
      boost::asio::io_context::strand &m_strand;
    };
  }  // namespace pipeline
}  // namespace mtconnect
