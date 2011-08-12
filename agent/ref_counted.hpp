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

#ifndef REF_COUNTED_HPP
#define REF_COUNTED_HPP

#include <cmath>
#include "globals.hpp"

template<class T>
class RefCountedPtr {
public:
  // Constructors
  RefCountedPtr() { mObject = NULL; }
  RefCountedPtr(const RefCountedPtr &aPtr, bool aTakeRef = false) {
    mObject = NULL;
    setObject(aPtr.getObject(), aTakeRef);
  }
  RefCountedPtr(T &aObject, bool aTakeRef = false) {
    mObject = NULL;
    setObject(&aObject, aTakeRef);
  }
  RefCountedPtr(T *aObject, bool aTakeRef = false) {
    mObject = NULL;
    setObject(aObject, aTakeRef);
  }
  
  // Destructor
  ~RefCountedPtr();
  
  // Getters
  T *getObject() const { return mObject; }
  T *operator->(void) const { return mObject; }
  operator T*(void ) const { return mObject; }
  
  // Setters
  T *setObject(T *aEvent, bool aTakeRef = false);
  T *operator=(T *aEvent) { return setObject(aEvent); }  
  T *operator=(RefCountedPtr<T> &aPtr) { return setObject(aPtr.getObject()); }
  
  bool operator==(const RefCountedPtr &aOther) {
    return *mObject == *(aOther.mObject);
  }
  
  bool operator<(const RefCountedPtr &aOther);


protected:
  T *mObject;
};

template<class T>
inline RefCountedPtr<T>::~RefCountedPtr()
{
  if (mObject)
    mObject->unrefer();
}

template<class T>
inline bool RefCountedPtr<T>::operator<(const RefCountedPtr<T> &aOther)
{
  return (*mObject) < (*aOther.mObject);
}

template<class T>
inline T *RefCountedPtr<T>::setObject(T *aEvent, bool aTakeRef) {
  if (mObject != NULL)
    mObject->unrefer();
  mObject = aEvent;
  if (aEvent != NULL && !aTakeRef)
    mObject->referTo();
  
  return aEvent;
}

class RefCounted 
{
public:
  RefCounted() {
    mRefCount = 1;
  }
  RefCounted(RefCounted &aRef) {
    mRefCount = 1;
  }
  
  virtual ~RefCounted();
  
  /* Reference count management */
  void referTo();
  void unrefer();
  
  unsigned int refCount() { return mRefCount; }
//  bool operator<(RefCounted &aOther) { return this < &aOther; }

protected:
  /* Reference count */
  AtomicInt mRefCount;
};

#endif

