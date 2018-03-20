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
#include "ref_counted.hpp"
#include "dlib/threads.h"
#ifdef __APPLE__
	#include <libkern/OSAtomic.h>
#endif

static dlib::rmutex sRefMutex;

RefCounted::~RefCounted()
{
}

void RefCounted::referTo()
{
#ifdef _WINDOWS
	InterlockedIncrement(&(this->m_refCount));
#else
#ifdef MACOSX
	OSAtomicIncrement32Barrier(&(this->m_refCount));
#else
	dlib::auto_mutex lock(sRefMutex);
	m_refCount++;
#endif
#endif
}

void RefCounted::unrefer()
{
#ifdef _WINDOWS

	if (InterlockedDecrement(&(this->m_refCount)) <= 0)
	{
	delete this;
	}

#else
#ifdef MACOSX

	if (OSAtomicDecrement32Barrier(&(this->m_refCount)) <= 0)
	{
	delete this;
	}

#else
	dlib::auto_mutex lock(sRefMutex);

	if (--m_refCount <= 0)
	{
	delete this;
	}

#endif
#endif
}



