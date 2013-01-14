/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
  uint64_t getSequence() const {
    return mSequence;
  }
  bool wasSignaled() const {
    return mSequence != UINT64_MAX;
  }
  void reset() { 
    dlib::auto_mutex lock(mMutex); 
    mSequence = UINT64_MAX; 
  }
  
private:
  dlib::rmutex mMutex;
  dlib::rsignaler mSignal;
  std::vector<ChangeSignaler*> mSignalers;
  volatile uint64_t mSequence;
  
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
