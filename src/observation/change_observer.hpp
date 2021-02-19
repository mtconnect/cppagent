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

#include "utilities.hpp"
#include <condition_variable>

#include <mutex>
#include <vector>

namespace mtconnect
{
  namespace observation
  {
    class ChangeSignaler;
    class ChangeObserver
    {
    public:
      ChangeObserver() = default;

      virtual ~ChangeObserver();

      bool wait(unsigned long timeout) const
      {
        std::unique_lock<std::recursive_mutex> lock(m_mutex);

        if (m_sequence != UINT64_MAX)
          return true;

        return m_cv.wait_for(m_mutex, std::chrono::milliseconds{timeout},
                             [this]() { return m_sequence != UINT64_MAX; });
      }

      void signal(uint64_t sequence)
      {
        std::lock_guard<std::recursive_mutex> scopedLock(m_mutex);

        if (m_sequence > sequence && sequence)
          m_sequence = sequence;

        m_cv.notify_one();
      }

      uint64_t getSequence() const { return m_sequence; }

      bool wasSignaled() const { return m_sequence != UINT64_MAX; }

      void reset()
      {
        std::lock_guard<std::recursive_mutex> scopedLock(m_mutex);
        m_sequence = UINT64_MAX;
      }

    private:
      mutable std::recursive_mutex m_mutex;
      mutable std::condition_variable_any m_cv;

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
