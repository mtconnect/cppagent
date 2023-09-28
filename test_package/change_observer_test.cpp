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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <chrono>
#include <thread>

#include "mtconnect/buffer/circular_buffer.hpp"
#include "mtconnect/device_model/component.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/change_observer.hpp"

using namespace std::chrono_literals;
using namespace std;
using namespace std::literals;
using namespace date::literals;

using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

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
    ChangeObserverTest() {}

  protected:
    void SetUp() override
    {
      m_context = make_unique<boost::asio::io_context>();
      m_strand = make_unique < boost::asio::io_context::strand>(*m_context);
      m_signaler = std::make_unique<mtconnect::ChangeSignaler>();
      m_guard.emplace(m_context->get_executor());
    }

    void TearDown() override
    {
      m_signaler.reset();
      m_guard.reset();
      m_strand.reset();
      m_context.reset();
    }

    std::unique_ptr<boost::asio::io_context> m_context;
    std::unique_ptr<boost::asio::io_context::strand> m_strand;
    std::unique_ptr<mtconnect::ChangeSignaler> m_signaler;
    std::optional<WorkGuard> m_guard;
  };

  TEST_F(ChangeObserverTest, AddObserver)
  {
    mtconnect::ChangeObserver changeObserver(*m_strand);

    ASSERT_FALSE(m_signaler->hasObserver(&changeObserver));
    m_signaler->addObserver(&changeObserver);
    ASSERT_TRUE(m_signaler->hasObserver(&changeObserver));
  }

  TEST_F(ChangeObserverTest, SignalObserver)
  {
    mtconnect::ChangeObserver changeObserver(*m_strand);

    auto const expectedExeTime = 500ms;
    auto const expectedSeq = uint64_t {100};

    m_signaler->addObserver(&changeObserver);
    ASSERT_FALSE(changeObserver.wasSignaled());

    auto startTime = std::chrono::system_clock::now();
    std::chrono::milliseconds duration;
    
    changeObserver.m_handler = [&](boost::system::error_code ec) {
      EXPECT_EQ(boost::asio::error::operation_aborted, ec);
      duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - startTime);
      ASSERT_TRUE(changeObserver.wasSignaled());
    };  // Wait to be signalled within twice expected time
    ASSERT_TRUE(changeObserver.waitForSignal((expectedExeTime * 2)));
    m_context->run_for(expectedExeTime);
    changeObserver.signal(expectedSeq);
    m_context->run_for(50ms);
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
    changeObserver.m_handler = [&](boost::system::error_code ec) {
      EXPECT_FALSE(ec);
      duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - startTime);
      ASSERT_FALSE(changeObserver.wasSignaled());
    };
    auto waitResult = changeObserver.waitForSignal((expectedExeTime / 2));  // Wait to be signalled within twice expected time
    // Only wait a maximum of 1 / 2 the expected time

    m_context->run_until(startTime + expectedExeTime);

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
      changeObserver = new mtconnect::ChangeObserver(*m_strand);
      m_signaler->addObserver(changeObserver);
      ASSERT_TRUE(m_signaler->hasObserver(changeObserver));
      delete changeObserver;  // Not setting to nullptr so we can test observer was removed
    }

    ASSERT_FALSE(m_signaler->hasObserver(changeObserver));
  }

  TEST_F(ChangeObserverTest, ChangeSequence)
  {
    mtconnect::ChangeObserver changeObserver(*m_strand);

    m_signaler->addObserver(&changeObserver);
    ASSERT_FALSE(changeObserver.wasSignaled());

    auto const waitTime = 2000ms;

    bool called {false};
    changeObserver.m_handler = [&](boost::system::error_code ec) {
      EXPECT_EQ(boost::asio::error::operation_aborted, ec);
      ASSERT_TRUE(changeObserver.wasSignaled());
      called = true;
    };
    ASSERT_TRUE(changeObserver.waitForSignal(waitTime));  // Wait to be signalled within twice expected time

    m_context->run_for(50ms);
    m_signaler->signalObservers(uint64_t {100});
    m_signaler->signalObservers(uint64_t {200});
    m_signaler->signalObservers(uint64_t {300});
    m_context->run_for(100ms);

    ASSERT_TRUE(called);
    ASSERT_TRUE(changeObserver.wasSignaled());

    ASSERT_EQ(uint64_t {100}, changeObserver.getSequence());
  }

  TEST_F(ChangeObserverTest, ChangeSequence2)
  {
    using namespace std::chrono_literals;
    mtconnect::ChangeObserver changeObserver(*m_strand);

    m_signaler->addObserver(&changeObserver);

    auto const waitTime = 2000ms;
    bool called {false};
    changeObserver.m_handler = [&](boost::system::error_code ec) {
     EXPECT_EQ(boost::asio::error::operation_aborted, ec);
     ASSERT_TRUE(changeObserver.wasSignaled());
     called = true;
    };
    ASSERT_TRUE(changeObserver.waitForSignal(waitTime));  // Wait to be signalled within twice expected time

    m_context->run_for(50ms);
    m_signaler->signalObservers(uint64_t {100});
    m_signaler->signalObservers(uint64_t {200});
    m_signaler->signalObservers(uint64_t {300});
    m_signaler->signalObservers(uint64_t {30});
    m_context->run_for(100ms);

    ASSERT_TRUE(called);
    ASSERT_TRUE(changeObserver.wasSignaled());
    ASSERT_EQ(uint64_t {30}, changeObserver.getSequence());
  }

  class MockObserver : public AsyncObserver
  {
  public:
    using AsyncObserver::AsyncObserver;
    void fail(boost::beast::http::status status, const std::string &message) override
    {
      LOG(error) << message;
    };
    bool isRunning() override { return m_running; };
    bool m_running {true};
  };

  using namespace entity;
  using namespace device_model;
  using namespace device_model::data_item;
  namespace asio = boost::asio;

  class AsyncObserverTest : public ChangeObserverTest
  {
  public:
    void SetUp() override
    {
      ChangeObserverTest::SetUp();

      ErrorList errors;
      Properties d1 {
          {"id", "1"s}, {"name", "DeviceTest1"s}, {"uuid", "UnivUniqId1"s}, {"iso841Class", "4"s}};
      m_device = dynamic_pointer_cast<Device>(Device::getFactory()->make("Device", d1, errors));

      Properties c1 {{"id", "2"s}, {"name", "Comp1"s}};
      m_comp = Component::make("Comp1", {{"id", "2"s}, {"name", "Comp1"s}}, errors);
      m_device->addChild(m_comp, errors);
      m_dataItem1 = DataItem::make(
          {{"id", "a"s}, {"type", "LOAD"s}, {"category", "SAMPLE"s}, {"name", "DI1"s}}, errors);
      m_comp->addDataItem(m_dataItem1, errors);
      m_dataItem2 = DataItem::make(
          {{"id", "b"s}, {"type", "LOAD"s}, {"category", "SAMPLE"s}, {"name", "DI2"s}}, errors);
      m_comp->addDataItem(m_dataItem2, errors);

      m_signalers.emplace("a", m_dataItem1);
      m_signalers.emplace("b", m_dataItem2);
    }

    void TearDown() override { ChangeObserverTest::TearDown(); }

    SequenceNumber_t addObservations(int count)
    {
      SequenceNumber_t start = m_buffer.getSequence(), last;
      ErrorList errors;
      for (int i = 0; i < count; i++)
      {
        auto o = observation::Observation::make(m_dataItem1, m_value, m_time, errors);
        last = m_buffer.addToBuffer(o);
        EXPECT_EQ(start + i, last);
        EXPECT_EQ(0, errors.size());
      }

      return last;
    }

    bool waitFor(function<bool()> pred, const int count = 50)
    {
      for (int i = 0; !pred() && i < count; i++)
        m_context->run_one_for(50ms);

      return pred();
    }

    buffer::CircularBuffer m_buffer {8, 4};
    map<string, DataItemPtr> m_signalers;

    entity::Properties m_value {{"VALUE", "123"s}};
    Timestamp m_time {std::chrono::system_clock::now()};

    DataItemPtr m_dataItem1;
    DataItemPtr m_dataItem2;
    DevicePtr m_device;
    ComponentPtr m_comp;
  };

  TEST_F(AsyncObserverTest, async_observer_should_call_handler)
  {
    FilterSet filter {"a", "b"};
    shared_ptr<MockObserver> observer {
        make_shared<MockObserver>(*m_strand, m_buffer, std::move(filter), 500ms, 1000ms)};

    auto expected = addObservations(3);
    observer->observe(4, [this](const string &id) { return m_signalers[id].get(); });

    bool called {false};
    observer->m_handler = [&](std::shared_ptr<AsyncObserver> obs) {
      called = true;
      EXPECT_EQ(expected, obs->getSequence());
      return obs->getSequence();
    };

    observer->handlerCompleted();
    ASSERT_FALSE(called);

    m_context->run_for(100ms);
    ASSERT_FALSE(called);

    expected = addObservations(1);
    ASSERT_EQ(4ull, expected);  

    m_context->run_for(200ms);
    ASSERT_FALSE(called);

    m_context->run_for(200ms);
    waitFor([&called]() { return called; });
    ASSERT_TRUE(called);
  }

  TEST_F(AsyncObserverTest, if_not_at_end_should_call_immediately)
  {
    FilterSet filter {"a", "b"};
    shared_ptr<MockObserver> observer {
        make_shared<MockObserver>(*m_strand, m_buffer, std::move(filter), 250ms, 500ms)};

    addObservations(3);
    observer->observe(2, [this](const string &id) { return m_signalers[id].get(); });

    ASSERT_FALSE(observer->isEndOfBuffer());

    bool called {false};
    SequenceNumber_t expected = 2;
    bool end = false;
    observer->m_handler = [&](std::shared_ptr<AsyncObserver> obs) {
      called = true;
      EXPECT_EQ(expected, obs->getSequence());
      EXPECT_EQ(end, obs->isEndOfBuffer());
      asio::post(*m_strand, boost::bind(&AsyncObserver::handlerCompleted, obs));
      return m_buffer.getSequence();
    };

    observer->handlerCompleted();
    ASSERT_TRUE(called);
    ASSERT_TRUE(observer->isEndOfBuffer());

    end = true;
    called = false;
    m_context->run_for(100ms);
    ASSERT_FALSE(called);

    expected = addObservations(1);
    ASSERT_EQ(4ull, expected);

    m_context->run_for(100ms);
    ASSERT_FALSE(called);
    
    waitFor([&called]() { return called; });
    ASSERT_TRUE(called);
  }

  TEST_F(AsyncObserverTest, process_observations_in_small_chunks)
  {
    FilterSet filter {"a", "b"};
    shared_ptr<MockObserver> observer {
        make_shared<MockObserver>(*m_strand, m_buffer, std::move(filter), 200ms, 500ms)};

    addObservations(3);
    observer->observe(1, [this](const string &id) { return m_signalers[id].get(); });

    ASSERT_FALSE(observer->isEndOfBuffer());

    bool called {false};
    SequenceNumber_t expected = 1;
    bool end = false;
    observer->m_handler = [&](std::shared_ptr<AsyncObserver> obs) {
      called = true;
      EXPECT_EQ(expected, obs->getSequence());
      EXPECT_EQ(end, obs->isEndOfBuffer());
      return expected + 1;
    };

    observer->handlerCompleted();
    ASSERT_TRUE(called);
    ASSERT_FALSE(observer->isEndOfBuffer());
    asio::post(*m_strand, boost::bind(&AsyncObserver::handlerCompleted, observer));

    called = false;
    expected = 2;
    m_context->run_for(50ms);
    ASSERT_TRUE(called);
    ASSERT_EQ(3, observer->getSequence());
    ASSERT_FALSE(observer->isEndOfBuffer());
    asio::post(*m_strand, boost::bind(&AsyncObserver::handlerCompleted, observer));

    called = false;
    expected = 3;
    m_context->run_for(50ms);
    ASSERT_TRUE(called);
    ASSERT_EQ(4, observer->getSequence());
    ASSERT_TRUE(observer->isEndOfBuffer());
    asio::post(*m_strand, boost::bind(&AsyncObserver::handlerCompleted, observer));

    end = true;
    called = false;
    expected = 4;
    m_context->run_for(50ms);
    ASSERT_FALSE(called);
    ASSERT_EQ(4, observer->getSequence());
    ASSERT_TRUE(observer->isEndOfBuffer());

    auto s = addObservations(3);
    ASSERT_EQ(6ull, s);

    called = false;
    expected = 4;
    waitFor([&] { return called; });
    ASSERT_TRUE(called);
    ASSERT_EQ(5, observer->getSequence());
    ASSERT_FALSE(observer->isEndOfBuffer());  }

  TEST_F(AsyncObserverTest, should_call_handler_with_heartbeat)
  {
    FilterSet filter {"a", "b"};
    shared_ptr<MockObserver> observer {
        make_shared<MockObserver>(*m_strand, m_buffer, std::move(filter), 100ms, 200ms)};

    addObservations(3);

    observer->observe(4, [this](const string &id) { return m_signalers[id].get(); });

    ASSERT_TRUE(observer->isEndOfBuffer());

    bool called {false};
    SequenceNumber_t expected = 1;
    bool end = false;
    observer->m_handler = [&](std::shared_ptr<AsyncObserver> obs) {
      called = true;
      EXPECT_EQ(expected, obs->getSequence());
      EXPECT_EQ(end, obs->isEndOfBuffer());
      asio::post(*m_strand, boost::bind(&AsyncObserver::handlerCompleted, obs));
      return expected;
    };

    observer->handlerCompleted();
    ASSERT_FALSE(called);

    called = false;
    expected = 4;
    end = true;
    waitFor([&] { return called; });
    ASSERT_TRUE(called);
    ASSERT_EQ(4, observer->getSequence());
    ASSERT_TRUE(observer->isEndOfBuffer());
  }

  TEST_F(AsyncObserverTest, should_stop_if_not_running)
  {
    FilterSet filter {"a", "b"};
    shared_ptr<MockObserver> observer {
        make_shared<MockObserver>(*m_strand, m_buffer, std::move(filter), 100ms, 200ms)};

    addObservations(3);

    observer->observe(4, [this](const string &id) { return m_signalers[id].get(); });

    ASSERT_TRUE(observer->isEndOfBuffer());

    bool called {false};
    SequenceNumber_t expected = 1;
    bool end = false;
    observer->m_handler = [&](std::shared_ptr<AsyncObserver> obs) {
      called = true;
      EXPECT_EQ(expected, obs->getSequence());
      EXPECT_EQ(end, obs->isEndOfBuffer());
      asio::post(*m_strand, boost::bind(&AsyncObserver::handlerCompleted, obs));
      return expected;
    };

    observer->handlerCompleted();
    ASSERT_FALSE(called);

    called = false;
    expected = 4;
    end = true;
    waitFor([&] { return called; });
    ASSERT_TRUE(called);
    ASSERT_EQ(4, observer->getSequence());
    ASSERT_TRUE(observer->isEndOfBuffer());

    observer->m_running = false;
    called = false;
    expected = 4;
    end = true;
    waitFor([&] { return called; });
    ASSERT_FALSE(called);
  }
}  // namespace mtconnect
