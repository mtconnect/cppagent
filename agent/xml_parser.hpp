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

#include <list>
#include <set>

#include <libxml/tree.h>

#include "component.hpp"
#include "device.hpp"
#include "data_item.hpp"
#include "globals.hpp"
#include "cutting_tool.hpp"

class XmlParser
{
public:
	// Constructor to set the open the correct file
	XmlParser();

	virtual ~XmlParser();

	// Parses a file and returns a list of devices
	std::vector<Device *> parseFile(const std::string &aPath);

	// Just loads the document, assumed it has already been parsed before.
	void loadDocument(const std::string &aDoc);

	// Update the dom for this device
	void updateDevice(Device *aDevice);

	// Get std::list of data items in path
	void getDataItems(
		std::set<std::string> &filterSet,
		const std::string &path,
		xmlNodePtr node = NULL);

	// Get an asset object representing a parsed XML Asset document. This can be
	// full document or a fragment.
	AssetPtr parseAsset(
		const std::string &assetId,
		const std::string &type,
		const std::string &content);

	// Modify
	void updateAsset(AssetPtr assetPtr, const std::string &type, const std::string &content);

protected:
	// LibXML XML Doc
	xmlDocPtr m_doc;

protected:
	// Main method to process the nodes and return the objects
	Component *handleComponent(
		xmlNodePtr component,
		Component *parent = NULL,
		Device *device = NULL);

	// Helper to handle/return each component of the device
	Component *loadComponent(
		xmlNodePtr node,
		Component::EComponentSpecs spec,
		std::string &name);

	// Put all of the attributes of an element into a map
	std::map<std::string, std::string> getAttributes(const xmlNodePtr element);

	// Load the data items
	void loadDataItem(xmlNodePtr dataItems, Component *component, Device *device);

	// Perform loading on children and set up relationships
	void handleChildren(
		xmlNodePtr components,
		Component *parent = NULL,
		Device *device = NULL);

	void handleComposition(
		xmlNodePtr dataItems,
		Component *component );

	// Perform loading of references and set up relationships
	void handleRefenence(
		xmlNodePtr components,
		Component *parent = NULL,
		Device *device = NULL);

	// Asset Parser
	AssetPtr handleAsset(
		xmlNodePtr asset,
		const std::string &assetId,
		const std::string &type,
		const std::string &content,
		xmlDocPtr doc);

	// Cutting Tool Parser
	CuttingToolPtr handleCuttingTool(xmlNodePtr asset, xmlDocPtr doc);
	CuttingToolValuePtr parseCuttingToolNode(xmlNodePtr node, xmlDocPtr doc);
	void parseCuttingToolLife(CuttingToolPtr tool, xmlNodePtr node, xmlDocPtr doc);
	CuttingItemPtr parseCuttingItem(xmlNodePtr node, xmlDocPtr doc);
};

