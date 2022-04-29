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

#include "utilities.hpp"

namespace mtconnect {
  namespace observation {
    class ChangeSignaler;
    class ChangeObserver
    {
    public:
      ChangeObserver(boost::asio::io_context::strand &strand)
        : m_strand(strand), m_timer(strand.context())
      {}

      virtual ~ChangeObserver();

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

      void signal(uint64_t sequence)
      {
        std::lock_guard<std::recursive_mutex> scopedLock(m_mutex);

        if (m_sequence > sequence && sequence)
          m_sequence = sequence;

        m_timer.cancel();
      }

      uint64_t getSequence() const { return m_sequence; }

      bool wasSignaled() const { return m_sequence != UINT64_MAX; }

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

    class ChangeSignaler
    {
    public:
      // Observer Management
      void addObserver(ChangeObserver *observer);
      bool removeObserver(ChangeObserver *observer);
      bool hasObserver(ChangeObserver *observer) const;
      void signalObservers(uint64_t sequence) const;

      virtual ~ChangeSignaler();

    protected:
      // Observer Lists
      mutable std::recursive_mutex m_observerMutex;
      std::list<ChangeObserver *> m_observers;
    };
  }  // namespace observation
}  // namespace mtconnect
