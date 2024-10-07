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
#include <boost/thread/thread.hpp>

#include "mtconnect/config.hpp"
#include "mtconnect/logging.hpp"

namespace mtconnect::configuration {

  /// @brief Manages the boost asio context and allows for a syncronous
  ///        callback to execute when all the worker threads have stopped.
  class AGENT_LIB_API AsyncContext
  {
  public:
    using SyncCallback = std::function<void(AsyncContext &context)>;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    /// @brief creates an asio context and a guard to prevent it from
    ///        stopping
    AsyncContext() { m_guard.emplace(m_context.get_executor()); }
    /// @brief removes the copy constructor
    AsyncContext(const AsyncContext &) = delete;
    ~AsyncContext() {}

    /// @brief is the context running
    /// @returns running status
    auto isRunning() { return m_running; }

    /// @brief return the paused state
    /// @returns the paused state
    auto isPauased() { return m_paused; }

    /// @brief Testing only: method to remove the run guard from the context
    void removeGuard() { m_guard.reset(); }

    /// @brief get the boost asio context reference
    auto &get() { return m_context; }

    /// @brief operator() returns a reference to the io context
    /// @return the io context
    operator boost::asio::io_context &() { return m_context; }

    /// @brief sets the number of theads for asio thread pool
    /// @param[in] threads number of threads
    void setThreadCount(int threads) { m_threadCount = threads; }

    /// @brief start `m_threadCount` worker threads
    void start()
    {
      m_running = true;
      m_paused = false;
      do
      {
        for (int i = 0; i < m_threadCount; i++)
        {
          m_workers.emplace_back(boost::thread([this]() { m_context.run(); }));
        }
        auto &first = m_workers.front();
        while (m_running && !m_paused)
        {
          if (!first.try_join_for(boost::chrono::seconds(5)) && !m_running)
          {
            if (!first.try_join_for(boost::chrono::seconds(5)))
              m_context.stop();
          }
        }

        for (auto &w : m_workers)
        {
          w.join();
        }
        m_workers.clear();

        if (m_delayedStop.joinable())
          m_delayedStop.join();

        if (m_syncCallback)
        {
          m_syncCallback(*this);
          if (m_running)
          {
            m_syncCallback = nullptr;
            restart();
          }
        }

      } while (m_running);
    }

    /// @brief pause the worker threads. Sets a callback when the threads are paused.
    /// @param[in] callback the callback to call
    /// @param[in] safeStop stops by resetting the the guard, otherwise stop the
    ///            io context
    void pause(SyncCallback callback, bool safeStop = false)
    {
      m_paused = true;
      m_syncCallback = callback;
      if (safeStop && m_guard)
        m_guard.reset();
      else
        m_context.stop();
    }

    /// @brief stop the worker threads
    /// @param safeStop if `true` resets the guard or stops the context
    void stop(bool safeStop = true)
    {
      m_running = false;
      if (safeStop && m_guard)
        m_guard.reset();
      else
        m_context.stop();
    }

    /// @brief restarts the worker threads when paused
    void restart()
    {
      m_paused = false;
      if (!m_guard)
        m_guard.emplace(m_context.get_executor());
      m_context.restart();
    }

    /// @name Cover methods for asio io_context
    /// @{

    /// @brief io_context::run_for
    template <typename Rep, typename Period>
    auto run_for(const std::chrono::duration<Rep, Period> &rel_time)
    {
      return m_context.run_for(rel_time);
    }

    /// @brief io_context::run
    auto run() { return m_context.run(); }

    /// @brief io_context::run_one
    auto run_one() { return m_context.run_one(); }

    /// @brief io_context::run_one_for
    template <typename Rep, typename Period>
    auto run_one_for(const std::chrono::duration<Rep, Period> &rel_time)
    {
      return m_context.run_one_for(rel_time);
    }

    /// @brief io_context::run_one_until
    template <typename Clock, typename Duration>
    auto run_one_until(const std::chrono::time_point<Clock, Duration> &abs_time)
    {
      return m_context.run_one_for(abs_time);
    }

    /// @brief io_context::poll
    auto poll() { return m_context.poll(); }

    /// @brief io_context::poll
    auto get_executor() BOOST_ASIO_NOEXCEPT { return m_context.get_executor(); }

    /// @}

  private:
    void operator=(const AsyncContext &) {}

  protected:
    boost::asio::io_context m_context;
    std::list<boost::thread> m_workers;
    SyncCallback m_syncCallback;
    std::optional<WorkGuard> m_guard;
    std::thread m_delayedStop;

    int m_threadCount = 1;
    bool m_running = false;
    bool m_paused = false;
  };

}  // namespace mtconnect::configuration
