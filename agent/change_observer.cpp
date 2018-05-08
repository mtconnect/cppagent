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
#include "change_observer.hpp"

using namespace std;

// Observer

ChangeObserver::~ChangeObserver()
{
	for (vector<ChangeSignaler *>::iterator i = m_signalers.begin(); i != m_signalers.end(); i++)
	(*i)->removeObserver(this);
}

void ChangeObserver::addSignaler(ChangeSignaler *aSig)
{
	m_signalers.push_back(aSig);
}

bool ChangeObserver::removeSignaler(ChangeSignaler *aSig)
{
	for (vector<ChangeSignaler *>::iterator i = m_signalers.begin(); i != m_signalers.end(); i++)
	{
	if (*i == aSig)
	{
		m_signalers.erase(i);
		return true;
	}
	}

	return false;
}

// Signaler Management
ChangeSignaler::~ChangeSignaler()
{
	dlib::auto_mutex lock(m_observerMutex);

	for (vector<ChangeObserver *>::iterator i = m_observers.begin(); i != m_observers.end(); i++)
	(*i)->removeSignaler(this);
}

void ChangeSignaler::addObserver(ChangeObserver *aObserver)
{
	dlib::auto_mutex lock(m_observerMutex);
	m_observers.push_back(aObserver);
	aObserver->addSignaler(this);
}

bool ChangeSignaler::removeObserver(ChangeObserver *aObserver)
{
	dlib::auto_mutex lock(m_observerMutex);

	for (vector<ChangeObserver *>::iterator i = m_observers.begin(); i != m_observers.end(); i++)
	{
	if (*i == aObserver)
	{
		m_observers.erase(i);
		return true;
	}
	}

	return false;
}

bool ChangeSignaler::hasObserver(ChangeObserver *aObserver)
{
	dlib::auto_mutex lock(m_observerMutex);

	for (vector<ChangeObserver *>::iterator i = m_observers.begin(); i != m_observers.end(); i++)
	if (*i == aObserver)
		return true;

	return false;
}

void ChangeSignaler::signalObservers(uint64_t aSequence)
{
	dlib::auto_mutex lock(m_observerMutex);

	for (vector<ChangeObserver *>::iterator i = m_observers.begin(); i != m_observers.end(); i++)
	(*i)->signal(aSequence);
}


