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

#include "mtconnect/config.hpp"
#include "mtconnect/observation/observation.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  /// @brief Period Filter implementing MTConnect DataItem period-filter behavior
  class AGENT_LIB_API PeriodFilter : public Transform
  {
  public:
    /// @brief Helper class to save information about the last observation for the period filter
    struct LastObservation
    {
      /// @brief Construct a Last Observation
      /// @param p the amount of time in the period
      /// @param st the strand to use for the time
      LastObservation(std::chrono::milliseconds p, boost::asio::io_context::strand &st)
        : m_timer(st.context()), m_period(p)
      {}

      /// @brief Make sure the timer is canceled.
      ~LastObservation() { m_timer.cancel(); }

      /// @brief The timestamp o the last observation or timestamp of the adjusted timestamp to
      /// the end of the last scheduled send time.
      Timestamp m_timestamp;

      /// @brief The delayed observation.
      observation::ObservationPtr m_observation;

      /// @brief A timer for delayed sends.
      boost::asio::steady_timer m_timer;

      /// @brief Store the data item period here.
      std::chrono::milliseconds m_period;

      /// @brief Time from the current obervation to the end of the period.
      std::chrono::milliseconds m_delta;
    };

    using LastObservationMap = std::unordered_map<std::string, LastObservation>;
    using LastObservationIterator = LastObservationMap::iterator;

    /// @brief A shared state variable containing the last observation
    struct State : TransformState
    {
      LastObservationMap m_lastObservation;
    };

    /// @brief Construct a period filter with a context
    /// @param context the context
    /// @param st strand for the timer
    PeriodFilter(PipelineContextPtr context, boost::asio::io_context::strand &st)
      : Transform("PeriodFilter"),
        m_state(context->getSharedState<State>(m_name)),
        m_contract(context->m_contract.get()),
        m_strand(st)
    {
      using namespace observation;
      constexpr static auto lambda = [](const Observation &s) {
        return bool(!s.isOrphan() && s.getDataItem()->getMinimumPeriod());
      };
      m_guard = LambdaGuard<Observation, TypeGuard<Event, Sample>>(lambda, RUN) ||
                TypeGuard<Observation>(SKIP);
    }
    ~PeriodFilter() override = default;

    entity::EntityPtr operator()(entity::EntityPtr &&entity) override
    {
      using namespace std;
      using namespace observation;
      using namespace entity;

      auto obs = std::dynamic_pointer_cast<Observation>(entity);
      {
        std::lock_guard<TransformState> guard(*m_state);

        if (obs->isOrphan())
          return EntityPtr();

        auto di = obs->getDataItem();
        auto &id = di->getId();

        if (obs->isUnavailable())
        {
          m_state->m_lastObservation.erase(id);
        }
        else
        {
          auto ts = obs->getTimestamp();

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
          if (filtered(last->second, id, obs, ts))
            return EntityPtr();
        }
      }

      return next(obs);
    }

  protected:
    // Returns true if the observation is filtered.
    bool filtered(LastObservation &last, const std::string &id, observation::ObservationPtr &obs,
                  const Timestamp &ts)
    {
      using namespace std;
      using namespace chrono;
      using namespace observation;

      auto delta = duration_cast<milliseconds>(ts - last.m_timestamp);
      if (delta.count() >= 0 && delta < last.m_period)
      {
        bool observed = bool(last.m_observation);
        last.m_observation = obs;
        last.m_delta = last.m_period - delta;

        // If we have not already observed something for this period,
        // set a timer, otherwise the current observation will replace the last
        // and be triggered when the timer expires. The end of the period is still the
        // same, so keep the timer as is.
        if (!observed)
          delayDelivery(last, id);

        // Filter this observation.
        return true;
      }
      else if (last.m_observation && delta >= last.m_period && delta < last.m_period * 2)
      {
        last.m_observation.swap(obs);

        // Similar to the delayed send, the last timestamp is computed as the end
        // of the previous period.
        last.m_timestamp = obs->getTimestamp() + last.m_delta;

        // Compute the distance to the next period and delay delivery of this observation.
        last.m_delta = last.m_period * 2 - delta;

        delayDelivery(last, id);

        // The observations will be swapped, so send the last onward.
        return false;
      }
      else
      {
        // If this observation is after the period has expired and there
        // is an existing observation, then we send the last observation.
        if (last.m_observation)
        {
          last.m_timer.cancel();
          next(last.m_observation);
          last.m_observation.reset();
        }

        // Set the timestamp of the last observation.
        last.m_timestamp = ts;

        // Send this observation. This may send two observations.
        return false;
      }
    }

    void delayDelivery(LastObservation &last, const std::string &id)
    {
      using std::placeholders::_1;

      // Set the timer to expire in the remaining time left in the period given
      // in last.m_delta
      last.m_timer.cancel();
      last.m_timer.expires_after(last.m_delta);

      // Bind the strand so we do not have races. Use the data item id so there are
      // no race conditions due to LastObservation lifecycle.
      last.m_timer.async_wait([this, id](boost::system::error_code ec) {
        boost::asio::dispatch(m_strand, boost::bind(&PeriodFilter::sendObservation, this, id, ec));
      });
    }

    void sendObservation(const std::string id, boost::system::error_code ec)
    {
      if (!ec)
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
            last->second.m_timestamp = obs->getTimestamp() + last->second.m_delta;
          }
        }

        // Send the observation onward
        if (obs)
        {
          next(obs);
        }
      }
    }

  protected:
    std::shared_ptr<State> m_state;
    PipelineContract *m_contract;
    boost::asio::io_context::strand &m_strand;
  };
}  // namespace mtconnect::pipeline
