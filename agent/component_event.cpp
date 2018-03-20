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
#include "component_event.hpp"
#include "data_item.hpp"
#include "dlib/threads.h"

#ifdef _WINDOWS
	#define strcasecmp stricmp
	#define strncasecmp strnicmp
	#define strtof strtod
#endif

using namespace std;
static dlib::rmutex sAttributeMutex;


const string ComponentEvent::SLevels[NumLevels] =
{
	"Normal",
	"Warning",
	"Fault",
	"Unavailable"
};

inline static bool splitValue(string &value, string &reset)
{
	size_t found = value.find_first_of(':');

	if (found == string::npos)
	{
	return false;
	}
	else
	{
	reset = value.substr(found + 1);
	value.erase(found);
	return true;
	}
}


ComponentEvent::ComponentEvent(
	DataItem &dataItem,
	uint64_t sequence,
	const string &time,
	const string &value) :
	m_hasAttributes(false)
{
	m_dataItem = &dataItem;
	m_isTimeSeries = m_dataItem->isTimeSeries();
	m_sequence = sequence;
	size_t pos = time.find('@');

	if (pos != string::npos)
	{
		m_time = time.substr(0, pos);
		m_duration = time.substr(pos + 1);
	}
	else
		m_time = time;

	if (m_dataItem->hasResetTrigger())
	{
		string v = value, reset;
		if (splitValue(v, reset))
			m_resetTriggered = reset;
		convertValue(v);
	}
	else
		convertValue(value);
}

ComponentEvent::ComponentEvent(ComponentEvent &componentEvent)
{
	m_dataItem = componentEvent.getDataItem();
	m_time = componentEvent.m_time;
	m_duration = componentEvent.m_duration;
	m_sequence = componentEvent.m_sequence;
	m_rest = componentEvent.m_rest;
	m_value = componentEvent.m_value;
	m_hasAttributes = false;
	m_code = componentEvent.m_code;
	m_isTimeSeries = componentEvent.m_isTimeSeries;

	if (m_isTimeSeries)
	{
		m_timeSeries = componentEvent.m_timeSeries;
		m_sampleCount = componentEvent.m_sampleCount;
	}
}


ComponentEvent::~ComponentEvent()
{
}


AttributeList *ComponentEvent::getAttributes()
{
	if (!m_hasAttributes)
	{
		dlib::auto_mutex lock(sAttributeMutex);

		m_attributes.push_back(AttributeItem("dataItemId", m_dataItem->getId()));
		m_attributes.push_back(AttributeItem("timestamp", m_time));

		if (!m_dataItem->getName().empty())
			m_attributes.push_back(AttributeItem("name", m_dataItem->getName()));

		if (!m_dataItem->getCompositionId().empty())
			m_attributes.push_back(AttributeItem("compositionId", m_dataItem->getCompositionId()));

		m_sequenceStr = int64ToString(m_sequence);
		m_attributes.push_back(AttributeItem("sequence", m_sequenceStr));

		if (!m_dataItem->getSubType().empty())
			m_attributes.push_back(AttributeItem("subType", m_dataItem->getSubType()));

		if (!m_dataItem->getStatistic().empty())
			m_attributes.push_back(AttributeItem("statistic", m_dataItem->getStatistic()));

		if (!m_duration.empty())
			m_attributes.push_back(AttributeItem("duration", m_duration));

		if (!m_resetTriggered.empty())
			m_attributes.push_back(AttributeItem("resetTriggered", m_resetTriggered));

		if (m_dataItem->isCondition())
		{
			// Conditon data: LEVEL|NATIVE_CODE|NATIVE_SEVERITY|QUALIFIER
			istringstream toParse(m_rest);
			string token;

			getline(toParse, token, '|');

			if (strcasecmp(token.c_str(), "normal") == 0)
				m_level = NORMAL;
			else if (strcasecmp(token.c_str(), "warning") == 0)
				m_level = WARNING;
			else if (strcasecmp(token.c_str(), "fault") == 0)
				m_level = FAULT;
			else // Assume unavailable
				m_level = UNAVAILABLE;


			if (!toParse.eof())
			{
				getline(toParse, token, '|');

				if (!token.empty())
				{
					m_code = token;
					m_attributes.push_back(AttributeItem("nativeCode", token));
				}
			}

			if (!toParse.eof())
			{
				getline(toParse, token, '|');

				if (!token.empty())
					m_attributes.push_back(AttributeItem("nativeSeverity", token));
			}

			if (!toParse.eof())
			{
				getline(toParse, token, '|');

				if (!token.empty())
					m_attributes.push_back(AttributeItem("qualifier", token));
			}

			m_attributes.push_back(AttributeItem("type", m_dataItem->getType()));
		}
		else if (m_dataItem->isTimeSeries())
		{
			istringstream toParse(m_rest);
			string token;

			getline(toParse, token, '|');

			if (token.empty())
				token = "0";

			m_attributes.push_back(AttributeItem("sampleCount", token));
			m_sampleCount = atoi(token.c_str());

			getline(toParse, token, '|');

			if (!token.empty())
				m_attributes.push_back(AttributeItem("sampleRate", token));
		}
		else if (m_dataItem->isMessage())
		{
			// Format to parse: NATIVECODE
			if (!m_rest.empty())
				m_attributes.push_back(AttributeItem("nativeCode", m_rest));
		}
		else if (m_dataItem->isAlarm())
		{
			// Format to parse: CODE|NATIVECODE|SEVERITY|STATE
			istringstream toParse(m_rest);
			string token;

			getline(toParse, token, '|');
			m_attributes.push_back(AttributeItem("code", token));

			getline(toParse, token, '|');
			m_attributes.push_back(AttributeItem("nativeCode", token));

			getline(toParse, token, '|');
			m_attributes.push_back(AttributeItem("severity", token));

			getline(toParse, token, '|');
			m_attributes.push_back(AttributeItem("state", token));
		}
		else if (m_dataItem->isAssetChanged() || m_dataItem->isAssetRemoved())
			m_attributes.push_back(AttributeItem("assetType", m_rest));

		m_hasAttributes = true;
	}

	return &m_attributes;
}

void ComponentEvent::normal()
{
	if (m_dataItem->isCondition())
	{
		m_attributes.clear();
		m_code.clear();
		m_hasAttributes = false;
		m_rest = "normal|||";
		getAttributes();
	}
}


void ComponentEvent::convertValue(const string &value)
{
	// Check if the type is an alarm or if it doesn't have units
	if (value == "UNAVAILABLE")
		m_value = value;
	else if (m_isTimeSeries ||
		m_dataItem->isCondition() ||
		m_dataItem->isAlarm() ||
		m_dataItem->isMessage() ||
		m_dataItem->isAssetChanged() ||
		m_dataItem->isAssetRemoved())
	{
		string::size_type lastPipe = value.rfind('|');

		// Alarm data = CODE|NATIVECODE|SEVERITY|STATE
		// Conditon data: SEVERITY|NATIVE_CODE|[SUB_TYPE]
		// Asset changed: type|id
		m_rest = value.substr(0, lastPipe);

		// sValue = DESCRIPTION
		if (m_isTimeSeries)
		{
			const char *cp = value.c_str();
			cp += lastPipe + 1;

			// Check if conversion is required...
			char *np;

			while (cp != nullptr && *cp != '\0')
			{
				float v = strtof(cp, &np);

				if (cp != np)
				{
					m_timeSeries.push_back(m_dataItem->convertValue(v));
				}
				else
					np = nullptr;

				cp = np;
			}
		}
		else
			m_value = value.substr(lastPipe + 1);
	}
	else if (m_dataItem->conversionRequired())
		m_value = m_dataItem->convertValue(value);
	else
		m_value = value;
}


ComponentEvent *ComponentEvent::getFirst()
{
	if (m_prev.getObject() != nullptr)
		return m_prev->getFirst();
	else
		return this;
}


void ComponentEvent::getList(std::list<ComponentEventPtr> &list)
{
	if (m_prev.getObject())
		m_prev->getList(list);

	list.push_back(this);
}


ComponentEvent *ComponentEvent::find(const std::string &code)
{
	if (m_code == code)
		return this;

	if (m_prev.getObject() != nullptr)
		return m_prev->find(code);

	return nullptr;
}


bool ComponentEvent::replace(ComponentEvent *oldEvent,
							 ComponentEvent *newEvent)
{
	ComponentEvent *obj = m_prev.getObject();

	if (obj == nullptr)
		return false;

	if (obj == oldEvent)
	{
		newEvent->m_prev = oldEvent->m_prev;
		m_prev = newEvent;
		return true;
	}

	return m_prev->replace(oldEvent, newEvent);
}


ComponentEvent *ComponentEvent::deepCopy()
{
	ComponentEvent *n = new ComponentEvent(*this);

	if (m_prev.getObject() != nullptr)
	{
		n->m_prev = m_prev->deepCopy();
		n->m_prev->unrefer();
	}

	return n;
}

ComponentEvent *ComponentEvent::deepCopyAndRemove(ComponentEvent *old)
{
	if (this == old)
	{
		if (m_prev.getObject() != nullptr)
			return m_prev->deepCopy();
		else
			return nullptr;
	}

	ComponentEvent *n = new ComponentEvent(*this);

	if (m_prev.getObject() != nullptr)
	{
		n->m_prev = m_prev->deepCopyAndRemove(old);

		if (n->m_prev.getObject() != nullptr)
			n->m_prev->unrefer();
	}

	return n;
}
