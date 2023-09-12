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
#include <boost/beast/http/status.hpp>
#include <boost/bind/bind.hpp>

#include <condition_variable>
#include <mutex>
#include <vector>

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::sink {
  class SinkContract;
  class Sink;
}  // namespace mtconnect::sink

namespace mtconnect::buffer {
  class CircularBuffer;
}

namespace mtconnect::device_model::data_item {
  class DataItem;
}

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

  /// @brief Asyncronous change context for waiting for changes
  class AGENT_LIB_API AsyncObserver : public std::enable_shared_from_this<AsyncObserver>
  {
  public:
    /// @brief create async observer to manage data item callbacks
    /// @param contract the sink contract to use to get the buffer information
    /// @param strand the strand to handle the async actions
    /// @param interval minimum amount of time to wait for observations
    /// @param heartbeat maximum amount of time to wait before sending a heartbeat
    AsyncObserver(boost::asio::io_context::strand &strand,
                  mtconnect::buffer::CircularBuffer &buffer, FilterSet &&filter,
                  std::chrono::milliseconds interval, std::chrono::milliseconds heartbeat);
    /// @brief default destructor
    virtual ~AsyncObserver() = default;

    /// @brief Get a shared pointed
    auto getptr() const { return const_cast<AsyncObserver *>(this)->shared_from_this(); }

    /// @brief sets up the `ChangeObserver` using the filter and initializes the references to the
    /// buffer
    /// @param from optional starting point. If not specified, defaults to the beginning of the
    /// buffer
    void observe(const std::optional<SequenceNumber_t> &from,
                 std::function<std::shared_ptr<mtconnect::device_model::data_item::DataItem>(
                     const std::string &id)>
                     resolver);

    /// @brief handle the operation completion after the handler is called
    ///
    /// Bound as the completion routine for async writes and actions. Proceeds to the next handleObservations.
    void handlerCompleted();

    /// @brief asyncronous callback when observations arrive or heartbeat times out.
    ///
    /// Callback when observations are ready to be written or heartbeat timer has expired.
    ///
    /// @params ec boost error code to detect when the timer is aborted
    void handleObservations(boost::system::error_code ec);

    /// @brief abstract call to failure handler
    virtual void fail(boost::beast::http::status status, const std::string &message) = 0;

    /// @brief method to determine if the sink is running
    virtual bool isRunning() = 0;

    /// @brief handler callback when an action needs to be taken
    ///
    /// This method may modify the `m_endOfBuffer` flag.
    ///
    /// @tparam AsyncObserver shared point to this
    /// @tparam error_code a boost error code
    /// @returns the ending sequence number
    std::function<std::pair<SequenceNumber_t,bool>(std::shared_ptr<AsyncObserver>)> m_handler;

    ///@{
    ///@name getters

    auto getSequence() const { return m_sequence; }
    const auto &getFilter() const { return m_filter; }

    ///@}

  protected:
    SequenceNumber_t m_sequence {0};  //! the current sequence number
    std::chrono::milliseconds m_interval {
        0};  //! the minimum amout of time to wait before calling the handler
    std::chrono::milliseconds m_heartbeat {
        0};  //! the maximum amount of time to wait before sending a heartbeat
    std::chrono::system_clock::time_point m_last;  //! the last time the handler completed
    FilterSet m_filter;                            //! The data items to be observed
    boost::asio::steady_timer m_timer;             //! async timer to call back
    boost::asio::io_context::strand m_strand;      //! Strand to use for aync dispatch

    bool m_endOfBuffer {false};  //! Indicator that we are at the end of the buffer
    ChangeObserver m_observer;   //! the change observer
    mtconnect::buffer::CircularBuffer &m_buffer; //! reference to the circular buffer
  };
}  // namespace mtconnect::observation
