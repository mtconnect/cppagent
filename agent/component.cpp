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
#include "agent.hpp"
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
	"DataItemRef",
	"ComponentRef",
	"Compositions",
	"Composition"
};


// Component public methods
Component::Component(
	const string &className,
  const std::map<string, string> &attributes,
	const string &prefix) :
	m_assetChanged(nullptr),
	m_assetRemoved(nullptr)
{
	const auto idPos = attributes.find("id");
	if(idPos != attributes.end())
		m_id = idPos->second;

	const auto namePos = attributes.find("name");
	if(namePos != attributes.end())
		m_name = namePos->second;

	const auto nativeNamePos = attributes.find("nativeName");
	if(nativeNamePos != attributes.end())
		m_nativeName = nativeNamePos->second;

	const auto uuidPos = attributes.find("uuid");
	if(uuidPos != attributes.end())
		m_uuid = uuidPos->second;


	const auto sampleIntervalPos = attributes.find("sampleInterval");
	if(sampleIntervalPos != attributes.end())
		m_sampleInterval = atof(sampleIntervalPos->second.c_str());
	else
	{
		const auto sampleRatePos = attributes.find("sampleRate");
		if(sampleRatePos == attributes.end())
			m_sampleInterval = 0.0f;
		else
			m_sampleInterval = atof(sampleRatePos->second.c_str());
	}

	m_parent = nullptr;
	m_device = nullptr;
	m_availability = nullptr;
	m_assetChanged = nullptr;
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


void Component::addDescription(string body, const std::map<string, string> &attributes)
{
	m_description = attributes;

	if (!body.empty())
		m_descriptionBody = body;
}


Device *Component::getDevice()
{
	if (!m_device)
	{
		if (getClass() == "Device")
			m_device = (Device *) this;
		else if (m_parent)
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

	for (auto &reference : m_references)
	{
    if (reference.m_type == Component::Reference::DATA_ITEM) {
      auto di = device->getDeviceDataItem(reference.m_id);
      
      if (!di)
        throw runtime_error("Cannot resolve Reference for component " + m_name + " to data item " +	reference.m_id);
      
      reference.m_dataItem = di;
    } else if (reference.m_type == Component::Reference::COMPONENT) {
      auto comp = device->getComponentById(reference.m_id);
      
      if (!comp)
        throw runtime_error("Cannot resolve Reference for component " + m_name + " to component " +  reference.m_id);
      
      reference.m_component = comp;
    }
	}

	for (const auto childComponent : m_children)
		childComponent->resolveReferences();
}

void Component::setParent(Component &parent)
{
  m_parent = &parent;
  auto device = getDevice();
  device->addComponent(this);
}
