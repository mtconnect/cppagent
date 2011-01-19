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

#include "checkpoint.hpp"
#include "data_item.hpp"

using namespace std;

Checkpoint::Checkpoint()
  : mHasFilter(false)
{
}

Checkpoint::Checkpoint(Checkpoint &aCheck, std::set<std::string> *aFilter)
{
  if (aFilter == NULL && aCheck.mHasFilter) {
    aFilter = &aCheck.mFilter;
  } else {
    mHasFilter = false;
  }
  copy(aCheck, aFilter);
}

void Checkpoint::clear()
{
  map<string, ComponentEventPtr*>::iterator it;
  for (it = mEvents.begin(); it != mEvents.end(); it++)
  {
    delete (*it).second;
  }
  mEvents.clear();
}


Checkpoint::~Checkpoint()
{
  clear();
}

void Checkpoint::addComponentEvent(ComponentEvent *anEvent)
{
  if (mHasFilter)
  {
    if (mFilter.count(anEvent->getDataItem()->getId()) == 0)
      return;
  }
  
  DataItem *item = anEvent->getDataItem();
  string id = item->getId();
  ComponentEventPtr *ptr = mEvents[id];
  if (ptr != NULL) {
    if (item->isCondition()) {
      // Chain event only if it is normal or unavailable and the
      // previous condition was not normal or unavailable
      if ((*ptr)->getLevel() != ComponentEvent::NORMAL &&
	  anEvent->getLevel() != ComponentEvent::NORMAL &&
	  (*ptr)->getLevel() != ComponentEvent::UNAVAILABLE &&
	  anEvent->getLevel() != ComponentEvent::UNAVAILABLE
	) {
	// Check to see if the native code matches an existing
	// active condition
	ComponentEvent *e = (*ptr)->find(anEvent->getCode());
	if (e != NULL) {
	  // Replace in chain.
	  ComponentEvent *n = (*ptr)->deepCopyAndRemove(e);
	  (*ptr) = n;
	  n->unrefer();
	}

	anEvent->appendTo(*ptr);
      }
    }

    if (anEvent->getLevel() == ComponentEvent::NORMAL &&
	anEvent->getCode()[0] != '\0') {
      ComponentEvent *e = (*ptr)->find(anEvent->getCode());
      if (e != NULL) {
	ComponentEvent *n = (*ptr)->deepCopyAndRemove(e);
	(*ptr) = n;
	n->unrefer();
      } else {
	// Not sure if we should register code specific normals if
	// previous normal was not found
	// (*ptr) = anEvent;
      }
    } else {
      (*ptr) = anEvent;
    }
  } else {
    mEvents[id] = new ComponentEventPtr(anEvent);
  }
}

void Checkpoint::copy(Checkpoint &aCheck, std::set<std::string> *aFilter)
{
  clear();
  if (aFilter != NULL) {
    mHasFilter = true;
    mFilter = *aFilter;
  } else if (mHasFilter) {
    aFilter = &mFilter;
  }
    
  map<string, ComponentEventPtr*>::iterator it;
  for (it = aCheck.mEvents.begin(); it != aCheck.mEvents.end(); ++it)
  {
    if (aFilter == NULL || aFilter->count(it->first) > 0)
      mEvents[(*it).first] = new ComponentEventPtr((*it).second->getObject());
  }
}

void Checkpoint::getComponentEvents(vector<ComponentEventPtr> &aList,
				    std::set<string> *aFilter)
{
  map<string, ComponentEventPtr*>::iterator it;
  for (it = mEvents.begin(); it != mEvents.end(); ++it)
  {
    ComponentEvent *e = *((*it).second);
    if (aFilter == NULL || aFilter->count(e->getDataItem()->getId()) > 0)
    {
      while (e != NULL)
      {
	aList.push_back(e);
	e = e->getPrev();
      }
    }
  }
}

void Checkpoint::filter(std::set<std::string> &aFilter)
{
  mFilter = aFilter;
  if (!aFilter.empty()) {
    map<string, ComponentEventPtr*>::iterator it = mEvents.begin();
    while (it != mEvents.end())
    {
#ifdef WIN32
      if (mFilter.count(it->first) == 0) {
        it = mEvents.erase(it);
      } else {
        ++it;
      }
#else
      if (mFilter.count(it->first) == 0)
          mEvents.erase(it);        
        ++it;
#endif
    }
  }
}
