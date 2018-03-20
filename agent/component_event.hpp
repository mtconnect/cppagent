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

#include <string>
#include <vector>
#include <cmath>

#include <dlib/array.h>

#include "component.hpp"
#include "globals.hpp"
#include "data_item.hpp"
#include "ref_counted.hpp"



typedef std::pair<const char *, std::string> AttributeItem;
typedef std::vector<AttributeItem> AttributeList;

class ComponentEvent;
typedef RefCountedPtr<ComponentEvent> ComponentEventPtr;
typedef dlib::array<ComponentEventPtr> ComponentEventPtrArray;


class ComponentEvent : public RefCounted
{

public:
	enum ELevel
	{
		NORMAL,
		WARNING,
		FAULT,
		UNAVAILABLE
	};

	static const unsigned int NumLevels = 4;
	static const std::string SLevels[];

public:
	// Initialize with the data item reference, sequence number, time and value
	ComponentEvent(
		DataItem &dataItem,
		uint64_t sequence,
		const std::string &time,
		const std::string &value
	);

	// Copy constructor
	ComponentEvent(const ComponentEvent &componentEvent);

	ComponentEvent *deepCopy();
	ComponentEvent *deepCopyAndRemove(ComponentEvent *old);

	// Extract the component event data into a map
	AttributeList *getAttributes();

	// Get the data item associated with this event
	DataItem *getDataItem() const {
		return m_dataItem; }

	// Get the value
	const std::string &getValue() const {
		return m_value; }
	ELevel getLevel();
	const std::string &getLevelString() {
		return SLevels[getLevel()]; }
	const std::string &getCode()
	{
		getAttributes();
		return m_code;
	}
	void normal();

	// Time series info...
	const std::vector<float> &getTimeSeries() const {
		return m_timeSeries; }
	bool isTimeSeries() const {
		return m_isTimeSeries; }
	int getSampleCount() const {
		return m_sampleCount; }

	uint64_t getSequence() const {
		return m_sequence; }

	ComponentEvent *getFirst();
	ComponentEvent *getPrev() {
		return m_prev; }
	void getList(std::list<ComponentEventPtr> &list);
	void appendTo(ComponentEvent *event);
	ComponentEvent *find(const std::string &nativeCode);
	bool replace(ComponentEvent *oldComponent, ComponentEvent *newComponent);

	bool operator<(ComponentEvent &another) const
	{
		if ((*m_dataItem) < (*another.m_dataItem))
			return true;
		else if (*m_dataItem == *another.m_dataItem)
			return m_sequence < another.m_sequence;
		else
			return false;
	}

protected:
	// Virtual destructor
	virtual ~ComponentEvent();

protected:
	// Holds the data item from the device
	DataItem *m_dataItem;

	// Sequence number of the event
	uint64_t m_sequence;
	std::string m_sequenceStr;

	// Timestamp of the event's occurence
	std::string m_time;
	std::string m_duration;

	// Hold the alarm data:  CODE|NATIVECODE|SEVERITY|STATE
	// or the Conditon data: LEVEL|NATIVE_CODE|NATIVE_SEVERITY|QUALIFIER
	// or the message data:  NATIVE_CODE
	// or the time series data
	std::string m_rest;
	ELevel m_level;

	// The value of the event, either as a float or a string
	std::string m_value;
	bool m_isFloat;
	bool m_isTimeSeries;
	std::vector<float> m_timeSeries;
	int m_sampleCount;

	// The attributes, created on demand
	bool m_hasAttributes;
	AttributeList m_attributes;

	// For condition tracking
	std::string m_code;

	// For reset triggered.
	std::string m_resetTriggered;

	// For back linking of condition
	ComponentEventPtr m_prev;

protected:
	// Convert the value to the agent unit standards
	void convertValue(const std::string &value);
};


inline ComponentEvent::ELevel ComponentEvent::getLevel()
{
	if (!m_hasAttributes)
		getAttributes();

	return m_level;
}


inline void ComponentEvent::appendTo(ComponentEvent *event)
{
	m_prev = event;
}

