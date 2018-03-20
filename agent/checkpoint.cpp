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
#include "checkpoint.hpp"
#include "data_item.hpp"

using namespace std;

Checkpoint::Checkpoint()
	: m_hasFilter(false)
{
}

Checkpoint::Checkpoint(Checkpoint &aCheck, std::set<std::string> *aFilter)
{
	if (aFilter == NULL && aCheck.m_hasFilter)
	{
	aFilter = &aCheck.m_filter;
	}
	else
	{
	m_hasFilter = false;
	}

	copy(aCheck, aFilter);
}

void Checkpoint::clear()
{
	map<string, ComponentEventPtr *>::iterator it;

	for (it = m_events.begin(); it != m_events.end(); it++)
	{
	delete (*it).second;
	}

	m_events.clear();
}


Checkpoint::~Checkpoint()
{
	clear();
}

void Checkpoint::addComponentEvent(ComponentEvent *anEvent)
{
	if (m_hasFilter)
	{
	if (m_filter.count(anEvent->getDataItem()->getId()) == 0)
		return;
	}

	DataItem *item = anEvent->getDataItem();
	string id = item->getId();
	ComponentEventPtr *ptr = m_events[id];

	if (ptr != NULL)
	{
	bool assigned = false;

	if (item->isCondition())
	{
		// Chain event only if it is normal or unavailable and the
		// previous condition was not normal or unavailable
		if ((*ptr)->getLevel() != ComponentEvent::NORMAL &&
		anEvent->getLevel() != ComponentEvent::NORMAL &&
		(*ptr)->getLevel() != ComponentEvent::UNAVAILABLE &&
		anEvent->getLevel() != ComponentEvent::UNAVAILABLE
		   )
		{
		// Check to see if the native code matches an existing
		// active condition
		ComponentEvent *e = (*ptr)->find(anEvent->getCode());

		if (e != NULL)
		{
			// Replace in chain.
			ComponentEvent *n = (*ptr)->deepCopyAndRemove(e);
			// Check if this is the only event...
			(*ptr) = n;

			if (n != NULL)
			{
			n->unrefer();
			}
		}

		// Chain the event
		if (ptr->getObject() != NULL)
			anEvent->appendTo(*ptr);

		}
		else  if (anEvent->getLevel() == ComponentEvent::NORMAL)
		{
		// Check for a normal that clears an active condition by code
		if (anEvent->getCode()[0] != '\0')
		{
			ComponentEvent *e = (*ptr)->find(anEvent->getCode());

			if (e != NULL)
			{
			// Clear the one condition by removing it from the chain
			ComponentEvent *n = (*ptr)->deepCopyAndRemove(e);
			(*ptr) = n;

			if (n != NULL)
			{
				n->unrefer();
			}
			else
			{
				// Need to put a normal event in with no code since this
				// is the last one.
				n = new ComponentEvent(*anEvent);
				n->normal();
				(*ptr) = n;
				n->unrefer();
			}
			}
			else
			{
			// Not sure if we should register code specific normals if
			// previous normal was not found
			// (*ptr) = anEvent;
			}

			assigned = true;
		}
		}
	}

	if (!assigned)
	{
		(*ptr) = anEvent;
	}
	}
	else
	{
	m_events[id] = new ComponentEventPtr(anEvent);
	}
}

void Checkpoint::copy(Checkpoint &aCheck, std::set<std::string> *aFilter)
{
	clear();

	if (aFilter != NULL)
	{
	m_hasFilter = true;
	m_filter = *aFilter;
	}
	else if (m_hasFilter)
	{
	aFilter = &m_filter;
	}

	map<string, ComponentEventPtr *>::iterator it;

	for (it = aCheck.m_events.begin(); it != aCheck.m_events.end(); ++it)
	{
	if (aFilter == NULL || aFilter->count(it->first) > 0)
		m_events[(*it).first] = new ComponentEventPtr((*it).second->getObject());
	}
}

void Checkpoint::getComponentEvents(ComponentEventPtrArray &aList,
					std::set<string> *aFilter)
{
	map<string, ComponentEventPtr *>::iterator it;

	for (it = m_events.begin(); it != m_events.end(); ++it)
	{
	ComponentEventPtr e = *((*it).second);

	if (aFilter == NULL || (e.getObject() != NULL && aFilter->count(e->getDataItem()->getId()) > 0))
	{
		while (e.getObject() != NULL)
		{
		ComponentEventPtr p = e->getPrev();
		aList.push_back(e);
		e = p;
		}
	}
	}
}

void Checkpoint::filter(std::set<std::string> &aFilter)
{
	m_filter = aFilter;

	if (!aFilter.empty())
	{
	map<string, ComponentEventPtr *>::iterator it = m_events.begin();

	while (it != m_events.end())
	{
		if (m_filter.count(it->first) == 0)
		{
#ifdef _WINDOWS
		it = m_events.erase(it);
#else
		map<string, ComponentEventPtr *>::iterator pos = it++;
		m_events.erase(pos);
#endif
		}
		else
		{
		++it;
		}
	}
	}
}
