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

#include "change_observer.hpp"

#include <boost/asio/io_context.hpp>

#include <algorithm>
#include <thread>

#include "mtconnect/sink/sink.hpp"

using namespace std;

namespace mtconnect::observation {
  ChangeObserver::~ChangeObserver()
  {
    for (const auto signaler : m_signalers)
      signaler->removeObserver(this);
  }

  void ChangeObserver::addSignaler(ChangeSignaler *sig) { m_signalers.emplace_back(sig); }

  bool ChangeObserver::removeSignaler(ChangeSignaler *sig)
  {
    auto newEndPos = std::remove(m_signalers.begin(), m_signalers.end(), sig);
    if (newEndPos == m_signalers.end())
      return false;

    m_signalers.erase(newEndPos);
    return true;
  }

  // Signaler Management
  ChangeSignaler::~ChangeSignaler()
  {
    std::lock_guard<std::recursive_mutex> lock(m_observerMutex);

    for (const auto observer : m_observers)
      observer->removeSignaler(this);
  }

  void ChangeSignaler::addObserver(ChangeObserver *observer)
  {
    std::lock_guard<std::recursive_mutex> lock(m_observerMutex);

    m_observers.emplace_back(observer);
    observer->addSignaler(this);
  }

  bool ChangeSignaler::removeObserver(ChangeObserver *observer)
  {
    std::lock_guard<std::recursive_mutex> lock(m_observerMutex);

    auto newEndPos = std::remove(m_observers.begin(), m_observers.end(), observer);
    if (newEndPos == m_observers.end())
      return false;

    m_observers.erase(newEndPos);
    return true;
  }

  bool ChangeSignaler::hasObserver(ChangeObserver *observer) const
  {
    std::lock_guard<std::recursive_mutex> lock(m_observerMutex);

    auto foundPos = std::find(m_observers.begin(), m_observers.end(), observer);
    return foundPos != m_observers.end();
  }

  void ChangeSignaler::signalObservers(uint64_t sequence) const
  {
    std::lock_guard<std::recursive_mutex> lock(m_observerMutex);

    for (const auto observer : m_observers)
      observer->signal(sequence);
  }

  AsyncObserver::AsyncObserver(boost::asio::io_context::strand &strand,
                               buffer::CircularBuffer &buffer, FilterSet &&filter,
                               std::chrono::milliseconds interval,
                               std::chrono::milliseconds heartbeat)
    : m_interval(interval),
      m_heartbeat(heartbeat),
      m_last(std::chrono::system_clock::now()),
      m_filter(std::move(filter)),
      m_timer(strand.context()),
      m_strand(strand),
      m_observer(strand),
      m_buffer(buffer)
  {}

  void AsyncObserver::observe(const std::optional<SequenceNumber_t> &from, Resolver resolver)
  {
    // This object will automatically clean up all the observer from the
    // signalers in an exception proof manor.
    // Add observers
    for (const auto &item : m_filter)
    {
      auto cs = resolver(item);
      if (cs)
        cs->addObserver(&m_observer);
    }

    SequenceNumber_t firstSeq, next;
    {
      std::lock_guard<buffer::CircularBuffer> lock(m_buffer);
      firstSeq = m_buffer.getFirstSequence();
      next = m_buffer.getSequence();
    }

    // If we are starting from the beginning of the buffer, signal the handler
    // to set the sequence to the fisrt sequence in the buffer to avoid a race
    // condition.
    if (!from || *from < firstSeq)
      m_sequence = 0;
    else
      m_sequence = *from;

    m_endOfBuffer = from >= next;
  }

  void AsyncObserver::handlerCompleted()
  {
    m_last = std::chrono::system_clock::now();
    if (m_endOfBuffer)
    {
      using std::placeholders::_1;
      m_observer.wait(m_heartbeat, boost::bind(&AsyncObserver::handleObservations, getptr(), _1));
    }
    else
    {
      handleObservations(boost::system::error_code {});
    }
  }

  void AsyncObserver::handleObservations(boost::system::error_code ec)
  {
    using namespace buffer;

    using std::placeholders::_1;
    using std::placeholders::_2;

    if (!isRunning())
    {
      LOG(warning)
          << "AsyncObserver::handleObservations: Trying to send chunk when service has stopped";
      fail(boost::beast::http::status::internal_server_error,
           "Agent shutting down, aborting stream");
      return;
    }

    if (ec && ec != boost::asio::error::operation_aborted)
    {
      LOG(warning) << "Unexpected error AsyncObserver::handleObservations, aborting";
      LOG(warning) << ec.category().message(ec.value()) << ": " << ec.message();
      fail(boost::beast::http::status::internal_server_error,
           "Unexpected error in async observer, aborting");
      return;
    }

    {
      std::lock_guard<CircularBuffer> lock(m_buffer);

      // Check if we are streaming chunks rapidly to catch up to the end of
      // buffer. We will not delay between chunks in this case and write as
      // rapidly as possible
      if (m_endOfBuffer)
      {
        if (!m_observer.wasSignaled())
        {
          // If nothing came out during the last wait, we may have still have advanced
          // the sequence number. We should reset the start to something closer to the
          // current sequence. If we lock the sequence lock, we can check if the observer
          // was signaled between the time the wait timed out and the mutex was locked.
          // Otherwise, nothing has arrived and we set to the next sequence number to
          // the next sequence number to be allocated and continue.
          m_sequence = m_buffer.getSequence();
        }
        else
        {
          // The observer can be signaled before the interval has expired. If this occurs, then
          // Wait the remaining duration of the interval.
          auto delta =
              chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - m_last);
          if (delta < m_interval)
          {
            m_timer.expires_from_now(m_interval - delta);
            m_timer.async_wait(boost::asio::bind_executor(
                m_strand, boost::bind(&AsyncObserver::handleObservations, getptr(), _1)));
            return;
          }

          // Get the sequence # signaled in the observer when the earliest event arrived.
          // This will allow the next set of data to be pulled. Any later events will have
          // greater sequence numbers, so this should not cause a problem. Also, signaled
          // sequence numbers can only decrease, never increase.
          m_sequence = m_observer.getSequence();
          m_observer.reset();
        }
      }
      else if (m_sequence == 0)
      {
        // The first time we are starting from the beginning of the buffer, make
        // sure we don't have a race condition where the first sequence had already
        // rolled off the buffer before we have a chance to send anything.
        m_sequence = m_buffer.getFirstSequence();
      }

      // Check if we're falling too far behind. If we are, generate an
      // MTConnectError and return.
      if (m_sequence < m_buffer.getFirstSequence())
      {
        LOG(warning) << "Client fell too far behind, disconnecting";
        fail(boost::beast::http::status::not_found, "Client fell too far behind, disconnecting");
        return;
      }

      // End of buffer is set in the handler
      m_sequence = m_handler(getptr());
      m_endOfBuffer = m_sequence >= m_buffer.getSequence();
    }
  }

}  // namespace mtconnect::observation
