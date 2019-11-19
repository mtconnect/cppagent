//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "change_observer.hpp"

namespace {

class ChangeObserverTest : public testing::Test {
 protected:
  void SetUp() override { m_signaler = std::make_unique<mtconnect::ChangeSignaler>(); }

  void TearDown() override { m_signaler.reset(); }

  std::unique_ptr<mtconnect::ChangeSignaler> m_signaler;
};

TEST_F(ChangeObserverTest, AddObserver) {
  mtconnect::ChangeObserver changeObserver;

  ASSERT_FALSE(m_signaler->hasObserver(&changeObserver));
  m_signaler->addObserver(&changeObserver);
  ASSERT_TRUE(m_signaler->hasObserver(&changeObserver));
}

TEST_F(ChangeObserverTest, SignalObserver) {
  using namespace std::chrono_literals;
  mtconnect::ChangeObserver changeObserver;

  auto const expectedExeTime = 500ms;
  auto const expectedSeq = uint64_t{100};
  auto threadLambda = [&expectedExeTime, &expectedSeq](mtconnect::ChangeSignaler *changeSignaler) {
    std::this_thread::sleep_for(expectedExeTime);
    changeSignaler->signalObservers(expectedSeq);
  };

  m_signaler->addObserver(&changeObserver);
  ASSERT_FALSE(changeObserver.wasSignaled());
  auto workerThread = std::thread{threadLambda, m_signaler.get()};

  auto startTime = std::chrono::system_clock::now();
  ASSERT_TRUE(changeObserver.wait(
      (expectedExeTime * 2).count()));  // Wait to be signalled within twice expected time

  // The worker thread was put to sleep for 500 milli-seconds before signalling
  // observers, so at very least the duration should be greater than 500 milli-seconds.
  // The observer should also have received the sequence number 100
  auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now() - startTime);
  workerThread.join();
  ASSERT_LE(expectedExeTime.count(), durationMs.count());
  ASSERT_EQ(expectedSeq, changeObserver.getSequence());
  ASSERT_TRUE(changeObserver.wasSignaled());

  // Run the same test again but only wait for a shorter period than the
  // thread will take to execute. The observer should not be signalled
  // and the call should fail.
  changeObserver.reset();
  ASSERT_FALSE(changeObserver.wasSignaled());
  auto workerThread2 = std::thread{threadLambda, m_signaler.get()};
  try {
    auto waitResult = changeObserver.wait(
        (expectedExeTime / 2).count());  // Only wait a maximum of 1 / 2 the expected time

    // We can be spuriously woken up, so check that the work was not finished
    if (waitResult && !changeObserver.wasSignaled()) waitResult = false;

    ASSERT_FALSE(waitResult);
    ASSERT_FALSE(changeObserver.wasSignaled());
    workerThread2.join();
  } catch (...) {
    workerThread2.join();
    throw;
  }
}

TEST_F(ChangeObserverTest, Cleanup) {
  mtconnect::ChangeObserver *changeObserver = nullptr;

  {
    changeObserver = new mtconnect::ChangeObserver;
    m_signaler->addObserver(changeObserver);
    ASSERT_TRUE(m_signaler->hasObserver(changeObserver));
    delete changeObserver;  // Not setting to nullptr so we can test observer was removed
  }

  ASSERT_FALSE(m_signaler->hasObserver(changeObserver));
}

TEST_F(ChangeObserverTest, ChangeSequence) {
  mtconnect::ChangeObserver changeObserver;

  m_signaler->addObserver(&changeObserver);
  ASSERT_FALSE(changeObserver.wasSignaled());

  auto threadLambda = [](mtconnect::ChangeSignaler *changeSignaler) {
    changeSignaler->signalObservers(uint64_t{100});
    changeSignaler->signalObservers(uint64_t{200});
    changeSignaler->signalObservers(uint64_t{300});
  };
  auto workerThread = std::thread{threadLambda, m_signaler.get()};
  try {
    ASSERT_TRUE(changeObserver.wait(2000));
    ASSERT_TRUE(changeObserver.wasSignaled());

    ASSERT_EQ(uint64_t{100}, changeObserver.getSequence());

    // Wait for things to clean up...
    workerThread.join();
  } catch (...) {
    workerThread.join();
    throw;
  }
}

TEST_F(ChangeObserverTest, ChangeSequence2) {
  using namespace std::chrono_literals;
  mtconnect::ChangeObserver changeObserver;

  m_signaler->addObserver(&changeObserver);

  auto threadLambda = [](mtconnect::ChangeSignaler *changeSignaler) {
    changeSignaler->signalObservers(uint64_t{100});
    changeSignaler->signalObservers(uint64_t{200});
    changeSignaler->signalObservers(uint64_t{300});
    changeSignaler->signalObservers(uint64_t{30});
  };
  auto workerThread = std::thread{threadLambda, m_signaler.get()};
  try {
    ASSERT_TRUE(changeObserver.wait(2000));
    ASSERT_TRUE(changeObserver.wasSignaled());
    std::this_thread::sleep_for(50ms);
    ASSERT_EQ(uint64_t{30}, changeObserver.getSequence());

    // Wait for things to clean up...
    workerThread.join();
  } catch (...) {
    workerThread.join();
    throw;
  }
}
}
