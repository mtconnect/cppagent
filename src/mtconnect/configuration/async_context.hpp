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
#include <boost/thread/thread.hpp>

#include "logging.hpp"

namespace mtconnect::configuration {

  // Manages the boost asio context and allows for a syncronous
  // callback to execute when all the worker threads have stopped.
  class AsyncContext
  {
  public:
    using SyncCallback = std::function<void(AsyncContext &context)>;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    AsyncContext() { m_guard.emplace(m_context.get_executor()); }
    AsyncContext(const AsyncContext &) = delete;
    ~AsyncContext() {}

    auto &getContext() { return m_context; }
    operator boost::asio::io_context &() { return m_context; }

    void setThreadCount(int threads) { m_threadCount = threads; }

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

    void pause(SyncCallback callback, bool safeStop = false)
    {
      m_paused = true;
      m_syncCallback = callback;
      if (safeStop && m_guard)
        m_guard.reset();
      else
        m_context.stop();
    }

    void stop(bool safeStop = true)
    {
      m_running = false;
      if (safeStop && m_guard)
        m_guard.reset();
      else
        m_context.stop();
    }

    void restart()
    {
      m_paused = false;
      if (!m_guard)
        m_guard.emplace(m_context.get_executor());
      m_context.restart();
    }

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
