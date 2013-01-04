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

#include "change_observer.hpp"

using namespace std;

// Observer

ChangeObserver::~ChangeObserver()
{
  for (vector<ChangeSignaler*>::iterator i = mSignalers.begin(); i != mSignalers.end(); i++)
    (*i)->removeObserver(this);
}

void ChangeObserver::addSignaler(ChangeSignaler *aSig)
{
  mSignalers.push_back(aSig);
}

bool ChangeObserver::removeSignaler(ChangeSignaler *aSig)
{
  for (vector<ChangeSignaler*>::iterator i = mSignalers.begin(); i != mSignalers.end(); i++)
  {
    if (*i == aSig)
    {
      mSignalers.erase(i);
      return true;
    }
  }
  return false;
}

/* Signaler Management */
ChangeSignaler::~ChangeSignaler()
{
  dlib::auto_mutex lock(mObserverMutex);
  for (vector<ChangeObserver*>::iterator i = mObservers.begin(); i != mObservers.end(); i++)
    (*i)->removeSignaler(this);
}

void ChangeSignaler::addObserver(ChangeObserver *aObserver)
{
  dlib::auto_mutex lock(mObserverMutex);
  mObservers.push_back(aObserver);
  aObserver->addSignaler(this);
}  

bool ChangeSignaler::removeObserver(ChangeObserver *aObserver)
{
  dlib::auto_mutex lock(mObserverMutex);
  for (vector<ChangeObserver*>::iterator i = mObservers.begin(); i != mObservers.end(); i++)
  {
    if (*i == aObserver)
    {
      mObservers.erase(i);
      return true;
    }
  }
  return false;
}

bool ChangeSignaler::hasObserver(ChangeObserver *aObserver)
{
  dlib::auto_mutex lock(mObserverMutex);
  for (vector<ChangeObserver*>::iterator i = mObservers.begin(); i != mObservers.end(); i++)
    if (*i == aObserver)
      return true;
  
  return false;
}

void ChangeSignaler::signalObservers(uint64_t aSequence)
{
  dlib::auto_mutex lock(mObserverMutex);
  for (vector<ChangeObserver*>::iterator i = mObservers.begin(); i != mObservers.end(); i++)
    (*i)->signal(aSequence);
}


