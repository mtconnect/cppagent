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

#include "component_event.hpp"
#include <map>
#include <string>
#include <vector>
#include <set>


class Checkpoint
{
public:
	Checkpoint();
  Checkpoint(const Checkpoint &checkpoint);
	Checkpoint(Checkpoint &checkpoint, std::set<std::string> *filterSet = nullptr);
	~Checkpoint();

	void addComponentEvent(ComponentEvent *event);
	bool dataSetDifference(ComponentEvent *event) const;
	void copy(Checkpoint const &checkpoint, std::set<std::string> *filterSet = nullptr);
	void clear();
	void filter(std::set<std::string> const &filterSet);

	const std::map<std::string, ComponentEventPtr *> &getEvents() const {
		return m_events; }

	void getComponentEvents(ComponentEventPtrArray &list,
		std::set<std::string> const *filterSet = nullptr) const;

	ComponentEventPtr *getEventPtr(const std::string &id)
	{
		auto pos = m_events.find(id);
		if(pos != m_events.end())
			return pos->second;
		return nullptr;
	}

protected:
	std::map<std::string, ComponentEventPtr *> m_events;
	std::set<std::string> m_filter;
	bool m_hasFilter;
};

