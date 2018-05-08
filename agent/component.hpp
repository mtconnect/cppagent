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

#include <sstream>
#include <string>
#include <list>
#include <map>

#include "globals.hpp"

class DataItem;
class Device;
class Composition;

class Component
{
public:
	struct Reference
	{
	Reference(std::string &aId, std::string aName) :
		m_id(aId), m_name(aName), m_dataItem(NULL) {}

	std::string m_id;
	std::string m_name;
	DataItem *m_dataItem;
	};

	/* std::string enumeration for component parts and details */
	enum EComponentSpecs
	{
	// Component parts
	DEVICE,
	// Component details
	COMPONENTS,
	DATA_ITEM,
	DATA_ITEMS,
	CONFIGURATION,
	DESCRIPTION,
	SOURCE,
	TEXT,
	REFERENCES,
	REFERENCE,
	COMPOSITIONS,
	COMPOSITION
	};

	static const unsigned int NumComponentSpecs = 12;
	static const std::string SComponentSpecs[];

public:
	/* Take in a class name & mapping of attributes */
	Component(
	const std::string &cls,
	std::map<std::string, std::string> attributes,
	const std::string &aPrefix = ""
	);

	/* Virtual destructor */
	virtual ~Component();

	/* Return a map of attributes of all the component specs */
	std::map<std::string, std::string> *getAttributes() { return &m_attributes; }

	/* Return what part of the component it is */
	const std::string &getClass() const { return m_class; }
	const std::string &getPrefixedClass() const { return m_prefixedClass; }

	/* Getter methods for the component ID/Name */
	const std::string &getId() const { return m_id; }
	const std::string &getName() const { return m_name; }
	const std::string &getNativeName() const { return m_nativeName; }
	const std::string &getUuid() const { return m_uuid; }
	const std::string &getDescriptionBody() const { return m_descriptionBody; }
	const std::string &getPrefix() const { return m_prefix; }
	const std::string &getConfiguration() const { return m_configuration; }

	// Setter methods
	void setUuid(const std::string &aUuid) { m_uuid = aUuid; reBuildAttributes(); }
	void setManufacturer(const std::string &aManufacturer) { m_description["manufacturer"] = aManufacturer; }
	void setSerialNumber(const std::string &aSerialNumber) { m_description["serialNumber"] = aSerialNumber; }
	void setStation(const std::string &aStation) { m_description["station"] = aStation; }
	void setDescription(const std::string &aDescription) { m_descriptionBody = aDescription; }
	void setNativeName(const std::string &aNativeName) { m_nativeName = aNativeName; reBuildAttributes(); }

	// Cached data items
	DataItem *getAvailability() const { return m_availability; }
	DataItem *getAssetChanged() const { return m_assetChanged; }
	DataItem *getAssetRemoved() const { return m_assetRemoved; }


	/* Add/get description specifications using an attribute map */
	void addDescription(std::string body, std::map<std::string, std::string> attributes);
	const std::map<std::string, std::string> &getDescription() const { return m_description; }

	void setConfiguration(const std::string &aConfiguration) { m_configuration = aConfiguration; }

	/* Get the device that any component is associated with */
	Device *getDevice();

	/* Set/Get the component's parent component */
	void setParent(Component &parent) { m_parent = &parent; }
	Component *getParent() const { return m_parent; }

	/* Add to/get the component's std::list of children */
	void addChild(Component &child) { m_children.push_back(&child); }
	std::list<Component *> &getChildren() { return m_children; }

	// Add and get composition...
	void addComposition(Composition *aComposition) { m_compositions.push_back(aComposition); }
	std::list<Composition *> &getCompositions() { return m_compositions; }

	/* Add to/get the component's std::list of data items */
	void addDataItem(DataItem &dataItem);
	const std::list<DataItem *> &getDataItems() const { return m_dataItems; }

	bool operator<(const Component &comp) const { return m_id < comp.getId(); }
	bool operator==(const Component &comp) const { return m_id == comp.getId(); }

	/* References */
	void addReference(Reference &aReference) { m_references.push_back(aReference); }
	const std::list<Reference> &getReferences() const { return m_references; }

	void resolveReferences();

protected:
	/* Return a map of attributes of all the component specs */
	std::map<std::string, std::string> buildAttributes() const;
	void reBuildAttributes() { m_attributes = buildAttributes(); }

protected:
	/* Unique ID for each component */
	std::string m_id;

	/* Name for itself */
	std::string m_name;
	std::string m_nativeName;

	/* The class */
	std::string m_class;
	std::string m_prefix;
	std::string m_prefixedClass;

	/* Universal unique identifier */
	std::string m_uuid;

	/* If receiving data, a sample rate is needed */
	float m_sampleInterval;

	/* Description of itself */
	std::map<std::string, std::string> m_description;
	std::string m_descriptionBody;
	std::string m_configuration;

	/* Component relationships */
	/* Pointer to the parent component */
	Component *m_parent;
	Device *m_device;
	DataItem *m_availability;
	DataItem *m_assetChanged;
	DataItem *m_assetRemoved;

	/* Each component keeps track of it's children in a std::list */
	std::list<Component *> m_children;

	/* Keep Track of all the data items associated with this component */
	std::list<DataItem *> m_dataItems;

	/* List of all the compositions */
	std::list<Composition *> m_compositions;

	/* The set of attribtues */
	std::map<std::string, std::string> m_attributes;

	/* References */
	std::list<Reference> m_references;
};

struct ComponentComp
{
	bool operator()(const Component *lhs, const Component *rhs) const
	{
	return *lhs < *rhs;
	}
};


