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

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

#include <condition_variable>
#include <mutex>
#include <vector>

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::observation {
  class ChangeSignaler;

  /// @brief A class to observe a data item and signal when data changes
  class AGENT_LIB_API ChangeObserver
  {
  public:
    /// @brief Create a change observer that runs in a strand
    /// @param[in] strand the strand
    ChangeObserver(boost::asio::io_context::strand &strand)
      : m_strand(strand), m_timer(strand.context())
    {}

    virtual ~ChangeObserver();

    /// @brief wait for a change to occur asynchronously
    /// @param duration the duration to wait
    /// @param handler the handler to call back
    /// @return `true` if successful
    bool wait(std::chrono::milliseconds duration,
              std::function<void(boost::system::error_code)> handler)
    {
      std::unique_lock<std::recursive_mutex> lock(m_mutex);

      if (m_sequence != UINT64_MAX)
      {
        boost::asio::post(boost::asio::bind_executor(
            m_strand, boost::bind(handler, boost::system::error_code {})));
      }
      else
      {
        m_timer.expires_from_now(duration);
        m_timer.async_wait(handler);
      }
      return true;
    }

    /// @brief single all waiting observers if this sequence number is greater than the last
    ///
    /// also cancel the timer
    /// @param[in] sequence the sequence number of the observation
    void signal(uint64_t sequence)
    {
      std::lock_guard<std::recursive_mutex> scopedLock(m_mutex);

      if (m_sequence > sequence && sequence)
        m_sequence = sequence;

      m_timer.cancel();
    }
    /// @brief get the last sequence number signaled
    /// @return the sequence number
    uint64_t getSequence() const { return m_sequence; }
    /// @brief check if signaled
    /// @return `true` if it was signaled
    bool wasSignaled() const { return m_sequence != UINT64_MAX; }
    /// @brief reset the signaled state
    void reset()
    {
      std::lock_guard<std::recursive_mutex> scopedLock(m_mutex);
      m_sequence = UINT64_MAX;
    }

  private:
    boost::asio::io_context::strand &m_strand;
    mutable std::recursive_mutex m_mutex;
    boost::asio::steady_timer m_timer;

    std::list<ChangeSignaler *> m_signalers;
    volatile uint64_t m_sequence = UINT64_MAX;

  protected:
    friend class ChangeSignaler;
    void addSignaler(ChangeSignaler *sig);
    bool removeSignaler(ChangeSignaler *sig);
  };

  /// @brief A signaler of waiting observers
  class AGENT_LIB_API ChangeSignaler
  {
  public:
    /// @brief add an observer to the list
    /// @param[in] observer an observer
    void addObserver(ChangeObserver *observer);
    /// @brief remove an observer
    /// @param[in] observer an observer
    /// @return `true` if the observer was removed
    bool removeObserver(ChangeObserver *observer);
    /// @brief check if an observer is in the list
    /// @param[in] observer an observer
    /// @return `true` if the observer is in the list
    bool hasObserver(ChangeObserver *observer) const;
    /// @brief singal observers with a sequence number
    /// @param[in] sequence the sequence number
    void signalObservers(uint64_t sequence) const;

    virtual ~ChangeSignaler();

  protected:
    // Observer Lists
    mutable std::recursive_mutex m_observerMutex;
    std::list<ChangeObserver *> m_observers;
  };
}  // namespace mtconnect::observation
