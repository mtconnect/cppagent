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

#include <boost/asio.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/bind/bind.hpp>

#include <condition_variable>
#include <mutex>
#include <vector>

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::buffer {
  class CircularBuffer;
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

    /// @brief dispatch handler
    ///
    /// this is only necessary becase of issue with windows DLLs
    ///
    /// @param ec the error code from the callback
    void handler(boost::system::error_code ec);

    /// @brief wait for a signal to occur asynchronously. If it is already signaled, call the
    /// callback immediately.
    /// @param duration the duration to wait
    /// @param handler the handler to call back
    /// @return `true` if successful
    bool waitForSignal(std::chrono::milliseconds duration)
    {
      std::unique_lock<std::recursive_mutex> lock(m_mutex);

      m_noCancelOnSignal = false;
      if (m_sequence != UINT64_MAX)
      {
        if (m_timer.cancel() == 0)
        {
          handler(boost::system::error_code {});
        }
      }
      else
      {
        m_timer.expires_after(duration);
        using std::placeholders::_1;
        auto bound = boost::bind(&ChangeObserver::handler, this, _1);
        m_timer.async_wait(bound);
      }
      return true;
    }

    /// @brief wait a period of time where signals will not cancle the timer
    /// @param duration the duration to wait
    /// @param handler the handler to call back
    /// @return `true` if successful
    bool waitFor(std::chrono::milliseconds duration)
    {
      m_noCancelOnSignal = true;
      m_timer.expires_after(duration);
      using std::placeholders::_1;
      auto bound = boost::bind(&ChangeObserver::handler, this, _1);
      m_timer.async_wait(bound);

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

      if (!m_noCancelOnSignal)
      {
        if (m_timer.cancel() == 0)
        {
          // LOG(trace) << "Cannot cancel timer";
        }
      }
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
      m_noCancelOnSignal = false;
    }

    /// @brief handler for the callback
    boost::function<void(boost::system::error_code)> m_handler;

    /// @name Mutex lock  management
    ///@{

    /// @brief lock the mutex
    auto lock() { return m_mutex.lock(); }
    /// @brief unlock the mutex
    auto unlock() { return m_mutex.unlock(); }
    /// @brief try to lock the mutex
    auto try_lock() { return m_mutex.try_lock(); }
    ///@}

    /// @brief clear the observer information.
    void clear();

  private:
    boost::asio::io_context::strand &m_strand;
    mutable std::recursive_mutex m_mutex;
    boost::asio::steady_timer m_timer;

    std::list<ChangeSignaler *> m_signalers;
    volatile uint64_t m_sequence = UINT64_MAX;
    bool m_noCancelOnSignal {false};

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

  /// @brief Abstract class for things asynchronouos timers
  class AGENT_LIB_API AsyncResponse : public std::enable_shared_from_this<AsyncResponse>
  {
  public:
    AsyncResponse(std::chrono::milliseconds interval) : m_interval(interval) {}
    virtual ~AsyncResponse() = default;

    virtual bool cancel() = 0;

    /// @brief method to determine if the sink is running
    virtual bool isRunning() = 0;

    /// @brief get the request id for webservices
    const auto &getRequestId() const { return m_requestId; }

    /// @brief sets the optonal request id for webservices.
    void setRequestId(const std::optional<std::string> &id) { m_requestId = id; }

    /// @brief Get the interval
    const auto &getInterval() const { return m_interval; }

  protected:
    std::chrono::milliseconds m_interval {
        0};  //! the minimum amout of time to wait before calling the handler
    std::optional<std::string> m_requestId;  //! request id
  };

  /// @brief Asyncronous change context for waiting for changes
  ///
  /// This class must be subclassed and provide a fail and isRunning method.
  /// The caller first calls observe to resolve the filter ids to the signalers. This must be done
  /// before the first handlerComplete is called asyncronously. The observer handles calling the
  /// handler whenever a new observation is available or the heartbeat has timed out keeping track
  /// of the sequence number of the last signaled observation or if the observer is still at the end
  /// of the buffer and nothing is signaled.
  ///
  /// The handler and sequence numbers are handled inside the circular buffer lock to prevent race
  /// conditions with incoming data.
  class AGENT_LIB_API AsyncObserver : public AsyncResponse
  {
  public:
    /// @Brief callback when observations are ready
    using Handler = std::function<SequenceNumber_t(std::shared_ptr<AsyncObserver>)>;

    /// @brief Resolve a string to a change signaler
    using Resolver = std::function<ChangeSignaler *(const std::string &id)>;

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
    auto getptr() { return std::dynamic_pointer_cast<AsyncObserver>(shared_from_this()); }

    /// @brief sets up the `ChangeObserver` using the filter and initializes the references to the
    /// buffer
    /// @param from optional starting point. If not specified, defaults to the beginning of the
    /// buffer
    /// @param resolver resolve an id to a signaler
    void observe(const std::optional<SequenceNumber_t> &from, Resolver resolver);

    /// @brief handle the operation completion after the handler is called
    ///
    /// Bound as the completion routine for async writes and actions. Proceeds to the next
    /// handleObservations.
    void handlerCompleted();

    /// @brief abstract call to failure handler
    virtual void fail(boost::beast::http::status status, const std::string &message) = 0;

    /// @brief Stop all timers and release resources.
    bool cancel() override
    {
      m_observer.clear();
      return true;
    }

    /// @brief handler callback when an action needs to be taken
    ///
    /// @tparam AsyncObserver shared point to this
    /// @returns the ending sequence number
    Handler m_handler;

    ///@{
    ///@name getters

    auto getSequence() const { return m_sequence; }
    auto isEndOfBuffer() const { return m_endOfBuffer; }
    const auto &getFilter() const { return m_filter; }
    ///@}

    mutable bool m_endOfBuffer {false};  //! Public indicator that we are at the end of the buffer

  protected:
    /// @brief asyncronous callback when observations arrive or heartbeat times out.
    ///
    /// Callback when observations are ready to be written or heartbeat timer has expired.
    ///
    /// @params ec boost error code to detect when the timer is aborted
    void handleSignal(boost::system::error_code ec);

  protected:
    SequenceNumber_t m_sequence {0};  //! the current sequence number
    std::chrono::milliseconds m_heartbeat {
        0};  //! the maximum amount of time to wait before sending a heartbeat
    std::chrono::system_clock::time_point m_last;  //! the last time the handler completed
    FilterSet m_filter;                            //! The data items to be observed
    boost::asio::io_context::strand m_strand;      //! Strand to use for aync dispatch

    ChangeObserver m_observer;                    //! the change observer
    mtconnect::buffer::CircularBuffer &m_buffer;  //! reference to the circular buffer
  };
}  // namespace mtconnect::observation
