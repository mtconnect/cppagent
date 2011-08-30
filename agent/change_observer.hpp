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

#ifndef CHANGE_OBSERVER_HPP
#define CHANGE_OBSERVER_HPP

#include <vector>
#include "dlib/threads.h"
#include "globals.hpp"

class ChangeSignaler;

class ChangeObserver
{  
public:
  ChangeObserver() : mSignal(mMutex), mSequence(UINT64_MAX) { }
  virtual ~ChangeObserver();

  bool wait(unsigned long aTimeout) {
    dlib::auto_mutex lock(mMutex);
    if (mSequence == UINT64_MAX)
      return mSignal.wait_or_timeout(aTimeout); 
    else
      return true;
  }
  void signal(uint64_t aSequence) { 
    dlib::auto_mutex lock(mMutex);
    if (mSequence > aSequence && aSequence != 0)
      mSequence = aSequence;
    mSignal.signal(); 
  }
  uint64_t getSequence() { return mSequence; }
  bool wasSignaled() { return mSequence != UINT64_MAX; }
  void reset() { 
    dlib::auto_mutex lock(mMutex); 
    mSequence = UINT64_MAX; 
  }
  
private:
  dlib::rmutex mMutex;
  dlib::rsignaler mSignal;
  std::vector<ChangeSignaler*> mSignalers;
  uint64_t mSequence;
  
protected:
  friend class ChangeSignaler;
  void addSignaler(ChangeSignaler *aSig);
  bool removeSignaler(ChangeSignaler *aSig);
};

class ChangeSignaler 
{
public:
  /* Observer Management */
  void addObserver(ChangeObserver *aObserver);
  bool removeObserver(ChangeObserver *aObserver);
  bool hasObserver(ChangeObserver *aObserver);
  void signalObservers(uint64_t aSequence);
  
  virtual ~ChangeSignaler();
  
protected:
  /* Observer Lists */
  dlib::rmutex mObserverMutex;
  std::vector<ChangeObserver*> mObservers;
};

#endif
