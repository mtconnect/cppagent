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

