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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <chrono>
#include <thread>

#include "mtconnect/observation/change_observer.hpp"

using namespace std::chrono_literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace mtconnect {
  using namespace observation;
  class ChangeObserverTest : public testing::Test
  {
  public:
    ChangeObserverTest() : m_strand(m_context) {}

  protected:
    void SetUp() override { m_signaler = std::make_unique<mtconnect::ChangeSignaler>(); }

    void TearDown() override { m_signaler.reset(); }

    boost::asio::io_context m_context;
    boost::asio::io_context::strand m_strand;
    std::unique_ptr<mtconnect::ChangeSignaler> m_signaler;
  };

  TEST_F(ChangeObserverTest, AddObserver)
  {
    mtconnect::ChangeObserver changeObserver(m_strand);

    ASSERT_FALSE(m_signaler->hasObserver(&changeObserver));
    m_signaler->addObserver(&changeObserver);
    ASSERT_TRUE(m_signaler->hasObserver(&changeObserver));
  }

  TEST_F(ChangeObserverTest, SignalObserver)
  {
    mtconnect::ChangeObserver changeObserver(m_strand);

    auto const expectedExeTime = 500ms;
    auto const expectedSeq = uint64_t {100};

    m_signaler->addObserver(&changeObserver);
    ASSERT_FALSE(changeObserver.wasSignaled());

    auto startTime = std::chrono::system_clock::now();
    std::chrono::milliseconds duration;
    ASSERT_TRUE(changeObserver.wait((expectedExeTime * 2), [&](boost::system::error_code ec) {
      EXPECT_EQ(boost::asio::error::operation_aborted, ec);
      duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - startTime);
      ASSERT_TRUE(changeObserver.wasSignaled());
    }));  // Wait to be signalled within twice expected time

    m_context.run_until(startTime + expectedExeTime);
    changeObserver.signal(expectedSeq);
    m_context.run_one();
    // The worker thread was put to sleep for 500 milli-seconds before signalling
    // observers, so at very least the duration should be greater than 500 milli-seconds.
    // The observer should also have received the sequence number 100
    ASSERT_TRUE(changeObserver.wasSignaled());
    ASSERT_LE(expectedExeTime, duration);
    ASSERT_GE(expectedExeTime * 2, duration);
    ASSERT_EQ(expectedSeq, changeObserver.getSequence());

    // Run the same test again but only wait for a shorter period than the
    // thread will take to execute. The observer should not be signalled
    // and the call should fail.
    changeObserver.reset();
    ASSERT_FALSE(changeObserver.wasSignaled());
    startTime = std::chrono::system_clock::now();
    duration = 0ms;
    auto waitResult = changeObserver.wait((expectedExeTime / 2), [&](boost::system::error_code ec) {
      EXPECT_FALSE(ec);
      duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - startTime);
      ASSERT_FALSE(changeObserver.wasSignaled());
    });  // Wait to be signalled within twice expected time
    // Only wait a maximum of 1 / 2 the expected time

    m_context.run_until(startTime + expectedExeTime);

    // We can be spuriously woken up, so check that the work was not finished
    if (waitResult && !changeObserver.wasSignaled())
      waitResult = false;

    ASSERT_FALSE(waitResult);
    ASSERT_FALSE(changeObserver.wasSignaled());
  }

  TEST_F(ChangeObserverTest, Cleanup)
  {
    mtconnect::ChangeObserver *changeObserver = nullptr;

    {
      changeObserver = new mtconnect::ChangeObserver(m_strand);
      m_signaler->addObserver(changeObserver);
      ASSERT_TRUE(m_signaler->hasObserver(changeObserver));
      delete changeObserver;  // Not setting to nullptr so we can test observer was removed
    }

    ASSERT_FALSE(m_signaler->hasObserver(changeObserver));
  }

  TEST_F(ChangeObserverTest, ChangeSequence)
  {
    mtconnect::ChangeObserver changeObserver(m_strand);

    m_signaler->addObserver(&changeObserver);
    ASSERT_FALSE(changeObserver.wasSignaled());

    auto const waitTime = 2000ms;

    bool called {false};
    ASSERT_TRUE(changeObserver.wait(waitTime, [&](boost::system::error_code ec) {
      EXPECT_EQ(boost::asio::error::operation_aborted, ec);
      ASSERT_TRUE(changeObserver.wasSignaled());
      called = true;
    }));  // Wait to be signalled within twice expected time

    m_context.run_until(std::chrono::system_clock::now() + 50ms);
    m_signaler->signalObservers(uint64_t {100});
    m_signaler->signalObservers(uint64_t {200});
    m_signaler->signalObservers(uint64_t {300});
    m_context.run_one();

    ASSERT_TRUE(called);
    ASSERT_TRUE(changeObserver.wasSignaled());

    ASSERT_EQ(uint64_t {100}, changeObserver.getSequence());
  }

  TEST_F(ChangeObserverTest, ChangeSequence2)
  {
    using namespace std::chrono_literals;
    mtconnect::ChangeObserver changeObserver(m_strand);

    m_signaler->addObserver(&changeObserver);

    auto const waitTime = 2000ms;
    bool called {false};
    ASSERT_TRUE(changeObserver.wait(waitTime, [&](boost::system::error_code ec) {
      EXPECT_EQ(boost::asio::error::operation_aborted, ec);
      ASSERT_TRUE(changeObserver.wasSignaled());
      called = true;
    }));  // Wait to be signalled within twice expected time

    m_context.run_until(std::chrono::system_clock::now() + 50ms);
    m_signaler->signalObservers(uint64_t {100});
    m_signaler->signalObservers(uint64_t {200});
    m_signaler->signalObservers(uint64_t {300});
    m_signaler->signalObservers(uint64_t {30});
    m_context.run_one();

    ASSERT_TRUE(called);
    ASSERT_TRUE(changeObserver.wasSignaled());
    ASSERT_EQ(uint64_t {30}, changeObserver.getSequence());
  }
}  // namespace mtconnect
