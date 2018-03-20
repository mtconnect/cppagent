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

Checkpoint::Checkpoint() :
	m_hasFilter(false)
{
}


Checkpoint::Checkpoint(Checkpoint &checkpoint, std::set<std::string> *filterSet)
{
	if (!filterSet && checkpoint.m_hasFilter)
		filterSet = &checkpoint.m_filter;
	else
		m_hasFilter = false;

	copy(checkpoint, filterSet);
}


void Checkpoint::clear()
{
	for(const auto event : m_events)
		delete event.second;

	m_events.clear();
}


Checkpoint::~Checkpoint()
{
	clear();
}


void Checkpoint::addComponentEvent(ComponentEvent *event)
{
	if (m_hasFilter &&
		!m_filter.count(event->getDataItem()->getId()))
	{
		return;
	}

	auto item = event->getDataItem();
	const auto & id = item->getId();
	auto ptr = m_events[id];

	if (ptr)
	{
		bool assigned = false;

		if (item->isCondition())
		{
			// Chain event only if it is normal or unavailable and the
			// previous condition was not normal or unavailable
			if ((*ptr)->getLevel() != ComponentEvent::NORMAL &&
				event->getLevel() != ComponentEvent::NORMAL &&
				(*ptr)->getLevel() != ComponentEvent::UNAVAILABLE &&
				event->getLevel() != ComponentEvent::UNAVAILABLE )
			{
				// Check to see if the native code matches an existing
				// active condition
				auto e = (*ptr)->find(event->getCode());

				if (e)
				{
					// Replace in chain.
					auto n = (*ptr)->deepCopyAndRemove(e);
					// Check if this is the only event...
					(*ptr) = n;
					if (n)
						n->unrefer();
				}

				// Chain the event
				if (ptr->getObject())
					event->appendTo(*ptr);
			}
			else  if (event->getLevel() == ComponentEvent::NORMAL)
			{
				// Check for a normal that clears an active condition by code
				if (event->getCode()[0] != '\0')
				{
					auto e = (*ptr)->find(event->getCode());

					if (e)
					{
						// Clear the one condition by removing it from the chain
						auto n = (*ptr)->deepCopyAndRemove(e);
						(*ptr) = n;

						if (n)
							n->unrefer();
						else
						{
							// Need to put a normal event in with no code since this
							// is the last one.
							n = new ComponentEvent(*event);
							n->normal();
							(*ptr) = n;
							n->unrefer();
						}
					}
					else
					{
						// Not sure if we should register code specific normals if
						// previous normal was not found
						// (*ptr) = event;
					}

					assigned = true;
				}
			}
		}

		if (!assigned)
			(*ptr) = event;
	}
	else
		m_events[id] = new ComponentEventPtr(event);

}


void Checkpoint::copy(Checkpoint &checkpoint, std::set<std::string> *filterSet)
{
	clear();

	if (filterSet)
	{
		m_hasFilter = true;
		m_filter = *filterSet;
	}
	else if (m_hasFilter)
		filterSet = &m_filter;

	for (const auto &event : checkpoint.m_events)
	{
		if (!filterSet || filterSet->count(event.first) > 0)
			m_events[event.first] = new ComponentEventPtr(event.second->getObject());
	}
}


void Checkpoint::getComponentEvents(ComponentEventPtrArray &list, std::set<string> *filterSet)
{
	for (const auto &event : m_events)
	{
		auto e = *(event.second);

		if (!filterSet ||
			(e.getObject() && filterSet->count(e->getDataItem()->getId()) > 0) )
		{
			while (e.getObject())
			{
				auto p = e->getPrev();
				list.push_back(e);
				e = p;
			}
		}
	}
}


void Checkpoint::filter(std::set<std::string> &filterSet)
{
	m_filter = filterSet;

	if (filterSet.empty())
		return;

	auto it = m_events.begin();
	while (it != m_events.end())
	{
		if (!m_filter.count(it->first))
		{
#ifdef _WINDOWS
			it = m_events.erase(it);
#else
			auto pos = it++;
			m_events.erase(pos);
#endif
		}
		else
		{
			++it;
		}
	}
}
