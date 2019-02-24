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

#include "change_observer_test.hpp"
#include <chrono>
#include <thread>


using namespace std;

namespace mtconnect {
  namespace test {
    // Registers the fixture into the 'registry'
    CPPUNIT_TEST_SUITE_REGISTRATION(ChangeObserverTest);
    
    void ChangeObserverTest::setUp()
    {
      m_signaler = make_unique<ChangeSignaler>();
    }
    
    
    void ChangeObserverTest::tearDown()
    {
      m_signaler.reset();
    }
    
    
    void ChangeObserverTest::testAddObserver()
    {
      ChangeObserver changeObserver;
      
      CPPUNIT_ASSERT(!m_signaler->hasObserver(&changeObserver));
      m_signaler->addObserver(&changeObserver);
      CPPUNIT_ASSERT(m_signaler->hasObserver(&changeObserver));
    }
    
    
    void ChangeObserverTest::testSignalObserver()
    {
      ChangeObserver changeObserver;
      
      auto const expectedExeTime = 500ms;
      auto const expectedSeq = uint64_t{100};
      auto threadLambda = [&expectedExeTime, &expectedSeq](ChangeSignaler* changeSignaler)
      {
        this_thread::sleep_for(expectedExeTime);
        changeSignaler->signalObservers(expectedSeq);
      };
      
      
      m_signaler->addObserver(&changeObserver);
      CPPUNIT_ASSERT(!changeObserver.wasSignaled());
      auto workerThread = thread{threadLambda, m_signaler.get()};
      
      auto startTime = chrono::system_clock::now();
      CPPUNIT_ASSERT(changeObserver.wait((expectedExeTime * 2).count())); // Wait to be signalled within twice expected time
      
      // The worker thread was put to sleep for 500 milli-seconds before signalling
      // observers, so at very least the duration should be greater than 500 milli-seconds.
      // The observer should also have received the sequence number 100
      auto durationMs = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startTime);
      try
      {
        CPPUNIT_ASSERT_GREATEREQUAL(expectedExeTime.count(), durationMs.count());
        CPPUNIT_ASSERT_EQUAL(expectedSeq, changeObserver.getSequence());
        CPPUNIT_ASSERT(changeObserver.wasSignaled());
        workerThread.join();
      }
      catch(...)
      {
        workerThread.join();
        throw;
      }
      
      // Run the same test again but only wait for a shorter period than the
      // thread will take to execute. The observer should not be signalled
      // and the call should fail.
      changeObserver.reset();
      CPPUNIT_ASSERT(!changeObserver.wasSignaled());
      auto workerThread2 = thread{threadLambda, m_signaler.get()};
      try
      {
        auto waitResult = changeObserver.wait((expectedExeTime / 2).count()); // Only wait a maximum of 1/2 the expected time
        
        // We can be spuriously woken up, so check that the work was not finished
        if(waitResult && !changeObserver.wasSignaled())
          waitResult = false;
        
        CPPUNIT_ASSERT(!waitResult);
        CPPUNIT_ASSERT(!changeObserver.wasSignaled());
        workerThread2.join();
      }
      catch(...)
      {
        workerThread2.join();
        throw;
      }
    }
    
    
    void ChangeObserverTest::testCleanup()
    {
      ChangeObserver *changeObserver = nullptr;
      
      {
        changeObserver = new ChangeObserver;
        m_signaler->addObserver(changeObserver);
        CPPUNIT_ASSERT(m_signaler->hasObserver(changeObserver));
        delete changeObserver; // Not setting to nullptr so we can test observer was removed
      }
      
      CPPUNIT_ASSERT(!m_signaler->hasObserver(changeObserver));
    }
    
    
    void ChangeObserverTest::testChangeSequence()
    {
      ChangeObserver changeObserver;
      
      m_signaler->addObserver(&changeObserver);
      CPPUNIT_ASSERT(!changeObserver.wasSignaled());
      
      auto threadLambda = [](ChangeSignaler* changeSignaler)
      {
        changeSignaler->signalObservers(uint64_t{100});
        changeSignaler->signalObservers(uint64_t{200});
        changeSignaler->signalObservers(uint64_t{300});
      };
      auto workerThread = thread{threadLambda, m_signaler.get()};
      try
      {
        CPPUNIT_ASSERT(changeObserver.wait(2000));
        CPPUNIT_ASSERT(changeObserver.wasSignaled());
        
        CPPUNIT_ASSERT_EQUAL(uint64_t{100}, changeObserver.getSequence());
        
        // Wait for things to clean up...
        workerThread.join();
      }
      catch(...)
      {
        workerThread.join();
        throw;
      }
    }
    
    
    void ChangeObserverTest::testChangeSequence2()
    {
      ChangeObserver changeObserver;
      
      m_signaler->addObserver(&changeObserver);
      
      auto threadLambda = [](ChangeSignaler* changeSignaler)
      {
        changeSignaler->signalObservers(uint64_t{100});
        changeSignaler->signalObservers(uint64_t{200});
        changeSignaler->signalObservers(uint64_t{300});
        changeSignaler->signalObservers(uint64_t{30});
      };
      auto workerThread = thread{threadLambda, m_signaler.get()};
      try
      {
        CPPUNIT_ASSERT(changeObserver.wait(2000));
        CPPUNIT_ASSERT(changeObserver.wasSignaled());
        this_thread::sleep_for(50ms);
        CPPUNIT_ASSERT_EQUAL(uint64_t{30}, changeObserver.getSequence());
        
        // Wait for things to clean up...
        workerThread.join();
      }
      catch(...)
      {
        workerThread.join();
        throw;
      }
    }
  }
}
