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


