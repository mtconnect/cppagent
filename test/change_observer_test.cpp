/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "change_observer_test.hpp"
#include <dlib/misc_api.h>

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(ChangeObserverTest);

using namespace std;

void ChangeObserverTest::setUp()
{
  mSignaler = new ChangeSignaler;
}

void ChangeObserverTest::tearDown()
{
  delete mSignaler;
}

void ChangeObserverTest::testAddObserver()
{
  ChangeObserver obj;
  
  CPPUNIT_ASSERT(!mSignaler->hasObserver(&obj));
  mSignaler->addObserver(&obj);
  CPPUNIT_ASSERT(mSignaler->hasObserver(&obj));
}

static void signaler(void *aObj)
{
  ChangeSignaler *s = (ChangeSignaler*) aObj;
  dlib::sleep(1000);
  s->signalObservers(100);
}

void ChangeObserverTest::testSignalObserver()
{
  ChangeObserver obj;
  
  mSignaler->addObserver(&obj);
  dlib::create_new_thread(signaler, mSignaler);
  CPPUNIT_ASSERT(obj.wait(2000));
  
  dlib::create_new_thread(signaler, mSignaler);
  CPPUNIT_ASSERT(!obj.wait(500)); 
  
  // Wait for things to clean up...
  dlib::sleep(1000);
}

void ChangeObserverTest::testCleanup()
{
  ChangeObserver *obj;

  {
    obj = new ChangeObserver;
    mSignaler->addObserver(obj);
    CPPUNIT_ASSERT(mSignaler->hasObserver(obj));
    delete obj;
  }
  
  CPPUNIT_ASSERT(!mSignaler->hasObserver(obj));
}

static void signaler2(void *aObj)
{
  ChangeSignaler *s = (ChangeSignaler*) aObj;
  s->signalObservers(100);
  s->signalObservers(200);
  s->signalObservers(300);
}

void ChangeObserverTest::testChangeSequence()
{
  ChangeObserver obj;
  
  mSignaler->addObserver(&obj);
  dlib::create_new_thread(signaler2, mSignaler);
  CPPUNIT_ASSERT(obj.wait(2000));
  
  CPPUNIT_ASSERT_EQUAL(100ull, obj.getSequence());
  
  // Wait for things to clean up...
  dlib::sleep(1000);
}

static void signaler3(void *aObj)
{
  ChangeSignaler *s = (ChangeSignaler*) aObj;
  s->signalObservers(100);
  s->signalObservers(200);
  s->signalObservers(300);
  s->signalObservers(30);
}

void ChangeObserverTest::testChangeSequence2()
{
  ChangeObserver obj;
  
  mSignaler->addObserver(&obj);
  dlib::create_new_thread(signaler3, mSignaler);
  CPPUNIT_ASSERT(obj.wait(2000));
  dlib::sleep(500);

  CPPUNIT_ASSERT_EQUAL(30ull, obj.getSequence());
  
  // Wait for things to clean up...
  dlib::sleep(1000);
}
