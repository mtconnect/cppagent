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
#include "component.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "composition.hpp"
#include <stdlib.h>
#include <stdexcept>

using namespace std;

// Component static constants
const string Component::SComponentSpecs[NumComponentSpecs] =
{
	// Component parts
	"Device",
	// Component details
	"Components",
	"DataItem",
	"DataItems",
	"Configuration",
	"Description",
	"Source",
	"text",
	"References",
	"Reference",
	"Compositions",
	"Composition"
};


// Component public methods
Component::Component(
	const string &className,
	map<string, string> attributes,
	const string &prefix) :
	m_assetChanged(NULL),
	m_assetRemoved(NULL)
{
	m_id = attributes["id"];
	m_name = attributes["name"];
	m_nativeName = attributes["nativeName"];
	m_uuid = attributes["uuid"];

	if (attributes["sampleInterval"].empty())
		m_sampleInterval = (float)(attributes["sampleRate"].empty()) ? 0.0f : atof(attributes["sampleRate"].c_str());
	else
		m_sampleInterval = atof(attributes["sampleInterval"].c_str());

	m_parent = NULL;
	m_device = NULL;
	m_availability = NULL;
	m_assetChanged = NULL;
	m_class = className;
	m_prefix = prefix;
	m_prefixedClass = prefix + ":" + className;
	m_attributes = buildAttributes();
}


Component::~Component()
{
}


std::map<string, string> Component::buildAttributes() const
{
	std::map<string, string> attributes;
	attributes["id"] = m_id;

	if (!m_name.empty())
		attributes["name"] = m_name;

	if (m_sampleInterval != 0.0f)
		attributes["sampleInterval"] = floatToString(m_sampleInterval);

	if (!m_uuid.empty())
		attributes["uuid"] = m_uuid;

	if (!m_nativeName.empty())
		attributes["nativeName"] = m_nativeName;

	return attributes;
}


void Component::addDescription(string body, map<string, string> attributes)
{
	m_description = attributes;

	if (!body.empty())
		m_descriptionBody = body;
}


Device *Component::getDevice()
{
	if (m_device == NULL)
	{
		if (getClass() == "Device")
			m_device = (Device *) this;
		else if (m_parent != NULL)
			m_device = m_parent->getDevice();
	}

	return m_device;
}


void Component::addDataItem(DataItem &dataItem)
{
	if (dataItem.getType() == "AVAILABILITY")
		m_availability = &dataItem;
	else if (dataItem.getType() == "ASSET_CHANGED")
		m_assetChanged = &dataItem;
	else if (dataItem.getType() == "ASSET_REMOVED")
		m_assetRemoved = &dataItem;

	m_dataItems.push_back(&dataItem);
}


void Component::resolveReferences()
{
	Device *device = getDevice();

	std::list<Reference>::iterator iter;
	for (iter = m_references.begin(); iter != m_references.end(); iter++)
	{
		DataItem *di = device->getDeviceDataItem(iter->m_id);

		if (di == NULL)
			throw runtime_error("Cannot resolve Reference for component " + m_name + " to data item " + iter->m_id);

		iter->m_dataItem = di;
	}

	std::list<Component *>::iterator comp;
	for (comp = m_children.begin(); comp != m_children.end(); comp++)
		(*comp)->resolveReferences();
}
