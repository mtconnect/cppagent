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
#include <algorithm>


ChangeObserver::~ChangeObserver()
{
	for(const auto signaler : m_signalers)
		signaler->removeObserver(this);
}


void ChangeObserver::addSignaler(ChangeSignaler *sig)
{
	m_signalers.push_back(sig);
}


bool ChangeObserver::removeSignaler(ChangeSignaler *sig)
{
	auto newEndPos = std::remove(m_signalers.begin(), m_signalers.end(), sig);
	if( newEndPos == m_signalers.end() )
		return false;

	m_signalers.erase(newEndPos);
	return true;
}


// Signaler Management
ChangeSignaler::~ChangeSignaler()
{
	dlib::auto_mutex lock(m_observerMutex);

	for(const auto observer : m_observers)
		observer->removeSignaler(this);
}


void ChangeSignaler::addObserver(ChangeObserver *observer)
{
	dlib::auto_mutex lock(m_observerMutex);
	m_observers.push_back(observer);
	observer->addSignaler(this);
}


bool ChangeSignaler::removeObserver(ChangeObserver *observer)
{
	dlib::auto_mutex lock(m_observerMutex);

	auto newEndPos = std::remove(m_observers.begin(), m_observers.end(), observer);
	if( newEndPos == m_observers.end() )
		return false;

	m_observers.erase(newEndPos);
	return true;
}


bool ChangeSignaler::hasObserver(ChangeObserver *observer)
{
	dlib::auto_mutex lock(m_observerMutex);

	auto foundPos = std::find(m_observers.begin(), m_observers.end(), observer);
	return foundPos != m_observers.end();
}


void ChangeSignaler::signalObservers(uint64_t sequence)
{
	dlib::auto_mutex lock(m_observerMutex);

	for(const auto observer : m_observers)
		observer->signal(sequence);
}
