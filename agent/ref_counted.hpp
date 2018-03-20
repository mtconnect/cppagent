//
// Copyright Copyright 2012, System Insights, Inc.
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

#include <cmath>
#include "globals.hpp"

template<class T>
class RefCountedPtr
{
public:
	// Constructors
	RefCountedPtr() { m_object = NULL; }
	RefCountedPtr(const RefCountedPtr &aPtr, bool aTakeRef = false)
	{
	m_object = NULL;
	setObject(aPtr.getObject(), aTakeRef);
	}
	RefCountedPtr(T &aObject, bool aTakeRef = false)
	{
	m_object = NULL;
	setObject(&aObject, aTakeRef);
	}
	RefCountedPtr(T *aObject, bool aTakeRef = false)
	{
	m_object = NULL;
	setObject(aObject, aTakeRef);
	}

	// Destructor
	~RefCountedPtr();

	// Getters
	T *getObject() const { return m_object; }
	T *operator->(void) const { return m_object; }
	operator T *(void) const { return m_object; }

	// Setters
	T *setObject(T *aEvent, bool aTakeRef = false);
	T *operator=(T *aEvent) { return setObject(aEvent); }
	T *operator=(RefCountedPtr<T> &aPtr) { return setObject(aPtr.getObject()); }

	bool operator==(const RefCountedPtr &aOther)
	{
	return *m_object == *(aOther.m_object);
	}

	bool operator<(const RefCountedPtr &aOther);


protected:
	T *m_object;
};

template<class T>
inline RefCountedPtr<T>::~RefCountedPtr()
{
	if (m_object)
	m_object->unrefer();
}

template<class T>
inline bool RefCountedPtr<T>::operator<(const RefCountedPtr<T> &aOther)
{
	return (*m_object) < (*aOther.m_object);
}

template<class T>
inline T *RefCountedPtr<T>::setObject(T *aEvent, bool aTakeRef)
{
	if (m_object != NULL)
	m_object->unrefer();

	m_object = aEvent;

	if (aEvent != NULL && !aTakeRef)
	m_object->referTo();

	return aEvent;
}

class RefCounted
{
public:
	RefCounted()
	{
	m_refCount = 1;
	}
	RefCounted(RefCounted &aRef)
	{
	m_refCount = 1;
	}

	virtual ~RefCounted();

	/* Reference count management */
	void referTo();
	void unrefer();

	unsigned int refCount() { return m_refCount; }
//  bool operator<(RefCounted &aOther) { return this < &aOther; }

protected:
	/* Reference count */
	AtomicInt m_refCount;
};

