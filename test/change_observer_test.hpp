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

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <memory>
#include "change_observer.hpp"

namespace mtconnect {
  namespace test {
    class ChangeObserverTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(ChangeObserverTest);
      CPPUNIT_TEST(testAddObserver);
      CPPUNIT_TEST(testSignalObserver);
      CPPUNIT_TEST(testChangeSequence);
      CPPUNIT_TEST(testChangeSequence2);
      CPPUNIT_TEST(testCleanup);
      CPPUNIT_TEST_SUITE_END();
      
    protected:
      std::unique_ptr<ChangeSignaler> m_signaler;
      
    public:
      void testAddObserver();
      void testSignalObserver();
      void testCleanup();
      void testChangeSequence();
      void testChangeSequence2();
      
      void setUp();
      void tearDown();
    };
  }
}
