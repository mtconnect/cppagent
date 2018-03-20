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
#include "xml_parser.hpp"
#include "xml_printer.hpp"
#include "cutting_tool.hpp"
#include "composition.hpp"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include "dlib/logger.h"
#include <stdexcept>

#if _MSC_VER >= 1900
	#define gets gets_s
#endif

using namespace std;

static dlib::logger sLogger("xml.parser");

#define strstrfy(x) #x
#define strfy(x) strstrfy(x)
#define THROW_IF_XML2_ERROR(expr) \
if ((expr) < 0) { throw runtime_error("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }
#define THROW_IF_XML2_NULL(expr) \
if ((expr) == NULL) { throw runtime_error("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }

extern "C" void XMLCDECL
agentXMLErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...)
{
	va_list args;

	char buffer[2048];
	va_start(args, msg);
	vsnprintf(buffer, 2046, msg, args);
	buffer[2047] = '0';
	va_end(args);

	sLogger << dlib::LERROR << "XML: " << buffer;
}

/* XmlParser public methods */
XmlParser::XmlParser()
{
	m_doc = NULL;
}

std::vector<Device *> XmlParser::parseFile(const std::string &aPath)
{
	if (m_doc != NULL)
	xmlFreeDoc(m_doc);

	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr devices = NULL;
	std::vector<Device *> deviceList;

	try
	{
	xmlInitParser();
	xmlXPathInit();
	xmlSetGenericErrorFunc(NULL, agentXMLErrorFunc);

	THROW_IF_XML2_NULL(m_doc = xmlReadFile(aPath.c_str(), NULL,
						   XML_PARSE_NOBLANKS));

	std::string path = "//Devices/*";
	THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(m_doc));

	xmlNodePtr root = xmlDocGetRootElement(m_doc);

	if (root->ns != NULL)
	{
		path = addNamespace(path, "m");
		THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));

		// Get schema version from Devices.xml
		if (XmlPrinter::getSchemaVersion().empty())
		{
		string ns((const char *)root->ns->href);

		if (ns.find_first_of("urn:mtconnect.org:MTConnectDevices") == 0)
		{
			int last = ns.find_last_of(':');

			if (last != string::npos)
			{
			string version = ns.substr(last + 1);
			XmlPrinter::setSchemaVersion(version);
			}
		}
		}
	}

	// Add additional namespaces to the printer if they are referenced
	// here.
	string locationUrn;
	const char *location = (const char *) xmlGetProp(root, BAD_CAST "schemaLocation");

	if (location != NULL && strncmp(location, "urn:mtconnect.org:MTConnectDevices", 34) != 0)
	{
		string loc = location;
		size_t pos = loc.find(' ');

		if (pos != string::npos)
		{
		locationUrn = loc.substr(0, pos);
		string uri = loc.substr(pos + 1);

		// Try to find the prefix for this urn...
		xmlNsPtr ns = xmlSearchNsByHref(m_doc, root, BAD_CAST locationUrn.c_str());
		string prefix;

		if (ns->prefix != NULL)
			prefix = (const char *) ns->prefix;

		XmlPrinter::addDevicesNamespace(locationUrn, uri, prefix);
		}
	}

	// Add the rest of the namespaces...
	if (root->nsDef)
	{
		xmlNsPtr ns = root->nsDef;

		while (ns != NULL)
		{
		// Skip the standard namespaces for MTConnect and the w3c. Make sure we don't re-add the
		// schema location again.
		if (!isMTConnectUrn((const char *) ns->href) &&
			strncmp((const char *) ns->href, "http://www.w3.org/", 18) != 0 &&
			locationUrn != (const char *) ns->href &&
			ns->prefix != NULL)
		{
			string urn = (const char *) ns->href;
			string prefix = (const char *) ns->prefix;
			XmlPrinter::addDevicesNamespace(urn, "", prefix);
		}

		ns = ns->next;
		}
	}

	devices = xmlXPathEval(BAD_CAST path.c_str(), xpathCtx);

	if (devices == NULL)
	{
		throw (string) xpathCtx->lastError.message;
	}

	xmlNodeSetPtr nodeset = devices->nodesetval;

	if (nodeset == NULL || nodeset->nodeNr == 0)
	{
		throw (string) "Could not find Device in XML configuration";
	}

	// Collect the Devices...
	for (int i = 0; i != nodeset->nodeNr; ++i)
	{
		deviceList.push_back(static_cast<Device *>(handleComponent(nodeset->nodeTab[i])));
	}

	xmlXPathFreeObject(devices);
	xmlXPathFreeContext(xpathCtx);
	}
	catch (string e)
	{
	if (devices != NULL)
		xmlXPathFreeObject(devices);

	if (xpathCtx != NULL)
		xmlXPathFreeContext(xpathCtx);

	sLogger << dlib::LFATAL << "Cannot parse XML file: " << e;
	throw e;
	}
	catch (...)
	{
	if (devices != NULL)
		xmlXPathFreeObject(devices);

	if (xpathCtx != NULL)
		xmlXPathFreeContext(xpathCtx);

	throw;
	}

	return deviceList;
}

XmlParser::~XmlParser()
{
	if (m_doc != NULL)
	xmlFreeDoc(m_doc);
}

void XmlParser::loadDocument(const std::string &aDoc)
{
	if (m_doc != NULL)
	xmlFreeDoc(m_doc);

	try
	{
	xmlInitParser();
	xmlXPathInit();
	xmlSetGenericErrorFunc(NULL, agentXMLErrorFunc);

	THROW_IF_XML2_NULL(m_doc = xmlReadMemory(aDoc.c_str(), aDoc.length(),
				   "Devices.xml", NULL, XML_PARSE_NOBLANKS));
	}

	catch (string e)
	{
	sLogger << dlib::LFATAL << "Cannot parse XML document: " << e;
	throw e;
	}
}

void XmlParser::updateDevice(Device *aDevice)
{
	// Update the dom for this device...
}


void XmlParser::getDataItems(set<string> &aFilterSet,
				 const string &aPath, xmlNodePtr node)
{
	xmlNodePtr root = xmlDocGetRootElement(m_doc);

	if (node == NULL) node = root;

	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr objs = NULL;

	try
	{
	string path;
	THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(m_doc));
	xpathCtx->node = node;
	bool mtc = false;

	if (root->ns)
	{
		for (xmlNsPtr ns = root->nsDef; ns != NULL; ns = ns->next)
		{
		if (ns->prefix != NULL)
		{
			if (strncmp((const char *) ns->href, "urn:mtconnect.org:MTConnectDevices", 34) != 0)
			{
			THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, ns->prefix, ns->href));
			}
			else
			{
			mtc = true;
			THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
			}
		}
		}

		if (!mtc)
		THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));

		path = addNamespace(aPath, "m");
	}
	else
	{
		path = aPath;
	}

	objs = xmlXPathEvalExpression(BAD_CAST path.c_str(), xpathCtx);

	if (objs == NULL)
	{
		xmlXPathFreeContext(xpathCtx);
		return;
	}

	xmlNodeSetPtr nodeset = objs->nodesetval;

	if (nodeset != NULL)
	{
		for (int i = 0; i != nodeset->nodeNr; ++i)
		{
		xmlNodePtr n = nodeset->nodeTab[i];

		if (xmlStrcmp(n->name, BAD_CAST "DataItem") == 0)
		{
			xmlChar *id = xmlGetProp(n, BAD_CAST "id");

			if (id != NULL)
			{
			aFilterSet.insert((const char *) id);
			xmlFree(id);
			}
		}
		else if (xmlStrcmp(n->name, BAD_CAST "DataItems") == 0)
		{
			// Handle case where we are specifying the data items node...
			getDataItems(aFilterSet, "DataItem", n);
		}
		else if (xmlStrcmp(n->name, BAD_CAST "Reference") == 0)
		{
			xmlChar *id = xmlGetProp(n, BAD_CAST "dataItemId");

			if (id != NULL)
			{
			aFilterSet.insert((const char *) id);
			xmlFree(id);
			}
		}
		else // Find all the data items and references below this node
		{
			getDataItems(aFilterSet, "*//DataItem", n);
			getDataItems(aFilterSet, "*//Reference", n);
		}
		}
	}

	xmlXPathFreeObject(objs);
	xmlXPathFreeContext(xpathCtx);
	}
	catch (...)
	{
	if (objs != NULL)
		xmlXPathFreeObject(objs);

	if (xpathCtx != NULL)
		xmlXPathFreeContext(xpathCtx);

	sLogger << dlib::LWARN << "getDataItems: Could not parse path: " << aPath;
	}
}

/* XmlParser protected methods */
Component *XmlParser::handleComponent(
	xmlNodePtr component,
	Component *parent,
	Device *device
)
{
	Component *toReturn = NULL;
	Component::EComponentSpecs spec =
	(Component::EComponentSpecs) getEnumeration(
		(const char *) component->name,
		Component::SComponentSpecs,
		Component::NumComponentSpecs
	);

	string name;

	switch (spec)
	{
	case Component::DEVICE:
	name = (const char *) component->name;
	toReturn = device = (Device *) loadComponent(component, spec, name);
	break;

	case Component::COMPONENTS:
	case Component::DATA_ITEMS:
	case Component::REFERENCES:
	case Component::COMPOSITIONS:
	handleChildren(component, parent, device);
	break;

	case Component::DATA_ITEM:
	loadDataItem(component, parent, device);
	break;

	case Component::TEXT:
	break;

	case Component::REFERENCE:
	handleRefenence(component, parent, device);
	break;

	case Component::COMPOSITION:
	handleComposition(component, parent);
	break;

	default:
	// Assume component
	name = (const char *) component->name;
	toReturn = loadComponent(component, spec, name);
	break;
	}

	// Construct relationships
	if (toReturn != NULL && parent != NULL)
	{
	parent->addChild(*toReturn);
	toReturn->setParent(*parent);
	}

	// Check if there are children
	if (toReturn != NULL && component->children)
	{
	for (xmlNodePtr child = component->children; child != NULL; child = child->next)
	{
		if (child->type != XML_ELEMENT_NODE)
		continue;

		if (xmlStrcmp(child->name, BAD_CAST "Description") == 0)
		{
		xmlChar *text = xmlNodeGetContent(child);

		if (text != NULL)
		{
			toReturn->addDescription((string)(const char *) text, getAttributes(child));
			xmlFree(text);
		}

		}
		else if (xmlStrcmp(child->name, BAD_CAST "Configuration") == 0)
		{
		xmlNodePtr config = child->children;
		xmlBufferPtr buf;
		THROW_IF_XML2_NULL(buf = xmlBufferCreate());
		int count = xmlNodeDump(buf, config->doc, config,
					0, 0);

		if (count > 0)
		{
			toReturn->setConfiguration((string)(const char *) buf->content);
		}

		xmlBufferFree(buf);
		}
		else
		{
		handleComponent(child, toReturn, device);
		}
	}
	}

	return toReturn;
}

Component *XmlParser::loadComponent(
	xmlNodePtr node,
	Component::EComponentSpecs spec,
	string &name
)
{
	std::map<string, string> attributes = getAttributes(node);

	switch (spec)
	{
	case Component::DEVICE:
	return new Device(attributes);

	default:
	string prefix;

	if (node->ns != NULL && node->ns->prefix != 0 &&
		strncmp((const char *) node->ns->href, "urn:mtconnect.org:MTConnectDevices",
			34) != 0)
	{
		prefix = (const char *) node->ns->prefix;
	}

	return new Component(name, attributes, prefix);
	}
}

std::map<string, string> XmlParser::getAttributes(const xmlNodePtr node)
{
	std::map<string, string> toReturn;

	for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next)
	{
	if (attr->type == XML_ATTRIBUTE_NODE)
	{
		toReturn[(const char *) attr->name] = (const char *) attr->children->content;
	}
	}

	return toReturn;
}

void XmlParser::loadDataItem(
	xmlNodePtr dataItem,
	Component *parent,
	Device *device
)
{
	DataItem *d = new DataItem(getAttributes(dataItem));
	d->setComponent(*parent);

	if (dataItem->children != NULL)
	{

	for (xmlNodePtr child = dataItem->children; child != NULL; child = child->next)
	{
		if (child->type != XML_ELEMENT_NODE)
		continue;

		if (xmlStrcmp(child->name, BAD_CAST "Source") == 0)
		{
		xmlChar *text = xmlNodeGetContent(child);

		if (text != NULL)
		{
			d->addSource((const char *) text);
			xmlFree(text);
		}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "Constraints") == 0)
		{
		for (xmlNodePtr constraint = child->children; constraint != NULL; constraint = constraint->next)
		{
			if (constraint->type != XML_ELEMENT_NODE)
			continue;

			xmlChar *text = xmlNodeGetContent(constraint);

			if (text == NULL)
			continue;

			if (xmlStrcmp(constraint->name, BAD_CAST "Value") == 0)
			{
			d->addConstrainedValue((const char *) text);
			}
			else if (xmlStrcmp(constraint->name, BAD_CAST "Minimum") == 0)
			{
			d->setMinimum((const char *) text);
			}
			else if (xmlStrcmp(constraint->name, BAD_CAST "Maximum") == 0)
			{
			d->setMaximum((const char *) text);
			}
			else if (xmlStrcmp(constraint->name, BAD_CAST "Filter") == 0)
			{
			d->setMinmumDelta(strtod((const char *) text, NULL));
			}

			xmlFree(text);
		}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "Filters") == 0)
		{
		for (xmlNodePtr filter = child->children; filter != NULL; filter = filter->next)
		{
			if (filter->type != XML_ELEMENT_NODE)
			continue;

			if (xmlStrcmp(filter->name, BAD_CAST "Filter") == 0)
			{
			xmlChar *text = xmlNodeGetContent(filter);
			xmlChar *type = xmlGetProp(filter, BAD_CAST "type");

			if (type != NULL)
			{
				if (xmlStrcmp(type, BAD_CAST "PERIOD") == 0)
				d->setMinmumPeriod(strtod((const char *) text, NULL));
				else
				d->setMinmumDelta(strtod((const char *) text, NULL));

				xmlFree(type);
			}
			else
				d->setMinmumDelta(strtod((const char *) text, NULL));

			xmlFree(text);
			}

		}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "InitialValue") == 0)
		{
		xmlChar *text = xmlNodeGetContent(child);
		d->setInitialValue(string((const char *)text));
		xmlFree(text);
		}
		else if (xmlStrcmp(child->name, BAD_CAST "ResetTrigger") == 0)
		{
		xmlChar *text = xmlNodeGetContent(child);
		d->setResetTrigger(string((const char *)text));
		xmlFree(text);

		}
	}
	}

	parent->addDataItem(*d);
	device->addDeviceDataItem(*d);
}

void XmlParser::handleComposition(xmlNodePtr composition,
				  Component *parent)
{
	Composition *comp = new Composition(getAttributes(composition));

	for (xmlNodePtr child = composition->children; child != NULL; child = child->next)
	{
	if (xmlStrcmp(child->name, BAD_CAST "Description") == 0)
	{
		xmlChar *text = xmlNodeGetContent(child);
		string body;

		if (text != NULL)
		{
		body = string(static_cast<const char *>(static_cast<void *>(text)));
		xmlFree(text);
		}

		Composition::Description *desc = new Composition::Description(body, getAttributes(child));
		comp->setDescription(desc);
	}
	}

	parent->addComposition(comp);
}

void XmlParser::handleChildren(
	xmlNodePtr components,
	Component *parent,
	Device *device
)
{
	for (xmlNodePtr child = components->children; child != NULL; child = child->next)
	{
	if (child->type != XML_ELEMENT_NODE)
		continue;

	handleComponent(child, parent, device);
	}
}

void XmlParser::handleRefenence(xmlNodePtr reference,
				Component *parent,
				Device *device)
{
	map<string, string> attrs = getAttributes(reference);
	string name;

	if (attrs.count("name") > 0) name = attrs["name"];

	Component::Reference ref(attrs["dataItemId"], name);
	parent->addReference(ref);
}

// Asset or Cutting Tool parser

AssetPtr XmlParser::parseAsset(const std::string &aAssetId, const std::string &aType,
				   const std::string &aContent)
{
	AssetPtr asset;

	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr assetNodes = NULL;
	xmlDocPtr document = NULL;
	xmlBufferPtr buffer = NULL;

	try
	{
	// TODO: Check for asset fragment - check if top node is MTConnectAssets
	// If we don't have complete doc, parse as a fragment and create a top level node
	// adding namespaces from the printer namespaces and then using xmlParseInNodeContext.
	// This will solve fragment xml namespace issues (unless the fragment has namespace)
	// definition.

	THROW_IF_XML2_NULL(document = xmlReadDoc(BAD_CAST aContent.c_str(),
					  ((string) "file://" + aAssetId + ".xml").c_str(),
					  NULL, XML_PARSE_NOBLANKS));

	std::string path = "//Assets/*";
	THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(document));

	xmlNodePtr root = xmlDocGetRootElement(document);

	if (root->ns != NULL)
	{
		path = addNamespace(path, "m");
		THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
	}

	// Spin through all the assets and create cutting tool objects for the cutting tools
	// all others add as plain text.
	xmlNodePtr node = NULL;
	assetNodes = xmlXPathEval(BAD_CAST path.c_str(), xpathCtx);

	if (assetNodes == NULL || assetNodes->nodesetval == NULL ||
		assetNodes->nodesetval->nodeNr == 0)
	{
		// See if this is a fragment... the root node will be check when it is
		// parsed...
		node = root;
	}
	else
	{
		xmlNodeSetPtr nodeset = assetNodes->nodesetval;
		node = nodeset->nodeTab[0];
	}

	THROW_IF_XML2_NULL(buffer = xmlBufferCreate());

	for (xmlNodePtr child = node->children; child != NULL; child = child->next)
	{
		xmlNodeDump(buffer, document, child, 0, 0);
	}

	asset = handleAsset(node, aAssetId, aType,
				(const char *) buffer->content,
				document);

	// Cleanup objects...
	xmlBufferFree(buffer);
	xmlXPathFreeObject(assetNodes);
	xmlXPathFreeContext(xpathCtx);
	xmlFreeDoc(document);

	}
	catch (string e)
	{
	if (assetNodes != NULL)
		xmlXPathFreeObject(assetNodes);

	if (xpathCtx != NULL)
		xmlXPathFreeContext(xpathCtx);

	if (document != NULL)
		xmlFreeDoc(document);

	if (buffer != NULL)
		xmlBufferFree(buffer);

	sLogger << dlib::LERROR << "Cannot parse asset XML: " << e;
	asset = NULL;
	}
	catch (...)
	{
	if (assetNodes != NULL)
		xmlXPathFreeObject(assetNodes);

	if (xpathCtx != NULL)
		xmlXPathFreeContext(xpathCtx);

	if (document != NULL)
		xmlFreeDoc(document);

	if (buffer != NULL)
		xmlBufferFree(buffer);

	sLogger << dlib::LERROR << "Cannot parse asset XML, Unknown execption occurred";
	asset = NULL;
	}

	return asset;
}

CuttingToolValuePtr XmlParser::parseCuttingToolNode(xmlNodePtr aNode, xmlDocPtr aDoc)
{
	CuttingToolValuePtr value(new CuttingToolValue(), true);
	string name;

	if (aNode->ns != NULL && aNode->ns->prefix != NULL)
	{
	name = (const char *) aNode->ns->prefix;
	name += ':';
	}

	name += (const char *) aNode->name;
	value->m_key = name;

	if (aNode->children == NULL)
	{
	xmlChar *text = xmlNodeGetContent(aNode);

	if (text != NULL)
	{
		value->m_value = (char *) text;
		xmlFree(text);
	}
	}
	else
	{
	xmlBufferPtr buffer;
	THROW_IF_XML2_NULL(buffer = xmlBufferCreate());

	for (xmlNodePtr child = aNode->children; child != NULL; child = child->next)
	{
		xmlNodeDump(buffer, aDoc, child, 0, 0);
	}

	value->m_value = (char *) buffer->content;
	xmlBufferFree(buffer);
	}

	for (xmlAttrPtr attr = aNode->properties; attr != NULL; attr = attr->next)
	{
	if (attr->type == XML_ATTRIBUTE_NODE)
	{
		value->m_properties[(const char *) attr->name] = (const char *) attr->children->content;
	}
	}

	return value;
}

CuttingItemPtr XmlParser::parseCuttingItem(xmlNodePtr aNode, xmlDocPtr aDoc)
{
	CuttingItemPtr item(new CuttingItem(), true);

	for (xmlAttrPtr attr = aNode->properties; attr != NULL; attr = attr->next)
	{
	if (attr->type == XML_ATTRIBUTE_NODE)
	{
		item->m_identity[(const char *) attr->name] = (const char *) attr->children->content;
	}
	}

	for (xmlNodePtr child = aNode->children; child != NULL; child = child->next)
	{
	if (xmlStrcmp(child->name, BAD_CAST "Measurements") == 0)
	{
		for (xmlNodePtr meas = child->children; meas != NULL; meas = meas->next)
		{
		CuttingToolValuePtr value = parseCuttingToolNode(meas, aDoc);
		item->m_measurements[value->m_key] = value;
		}
	}
	else if (xmlStrcmp(child->name, BAD_CAST "ItemLife") == 0)
	{
		CuttingToolValuePtr value = parseCuttingToolNode(child, aDoc);
		item->m_lives.push_back(value);
	}
	else if (xmlStrcmp(child->name, BAD_CAST "text") != 0)
	{
		CuttingToolValuePtr value = parseCuttingToolNode(child, aDoc);
		item->m_values[value->m_key] = value;
	}
	}

	return item;
}

void XmlParser::parseCuttingToolLife(CuttingToolPtr aTool, xmlNodePtr aNode, xmlDocPtr aDoc)
{
	for (xmlNodePtr child = aNode->children; child != NULL; child = child->next)
	{
	if (xmlStrcmp(child->name, BAD_CAST "CuttingItems") == 0)
	{
		for (xmlAttrPtr attr = child->properties; attr != NULL; attr = attr->next)
		{
		if (attr->type == XML_ATTRIBUTE_NODE && xmlStrcmp(attr->name, BAD_CAST "count") == 0)
		{
			aTool->m_itemCount = (const char *) attr->children->content;
		}
		}

		for (xmlNodePtr itemNode = child->children; itemNode != NULL; itemNode = itemNode->next)
		{
		if (xmlStrcmp(itemNode->name, BAD_CAST "CuttingItem") == 0)
		{
			CuttingItemPtr item = parseCuttingItem(itemNode, aDoc);
			aTool->m_items.push_back(item);
		}
		}
	}
	else if (xmlStrcmp(child->name, BAD_CAST "Measurements") == 0)
	{
		for (xmlNodePtr meas = child->children; meas != NULL; meas = meas->next)
		{
		if (xmlStrcmp(meas->name, BAD_CAST "text") != 0)
		{
			CuttingToolValuePtr value = parseCuttingToolNode(meas, aDoc);
			aTool->m_measurements[value->m_key] = value;
		}
		}
	}
	else if (xmlStrcmp(child->name, BAD_CAST "CutterStatus") == 0)
	{
		for (xmlNodePtr status = child->children; status != NULL; status = status->next)
		{
		if (xmlStrcmp(status->name, BAD_CAST "Status") == 0)
		{
			xmlChar *text = xmlNodeGetContent(status);

			if (text != NULL)
			{
			aTool->m_status.push_back((const char *) text);
			xmlFree(text);
			}
		}
		}
	}
	else if (xmlStrcmp(child->name, BAD_CAST "ToolLife") == 0)
	{
		CuttingToolValuePtr value = parseCuttingToolNode(child, aDoc);
		aTool->m_lives.push_back(value);
	}
	else if (xmlStrcmp(child->name, BAD_CAST "text") != 0)
	{
		aTool->addValue(parseCuttingToolNode(child, aDoc));
	}
	}
}

AssetPtr XmlParser::handleAsset(xmlNodePtr anAsset, const std::string &aAssetId,
				const std::string &aType, const std::string &aContent, xmlDocPtr aDoc)
{
	AssetPtr asset;

	// We only handle cuttng tools for now...
	if (xmlStrcmp(anAsset->name, BAD_CAST "CuttingTool") == 0 ||
	xmlStrcmp(anAsset->name, BAD_CAST "CuttingToolArchetype") == 0)
	{
	asset = handleCuttingTool(anAsset, aDoc);
	}
	else
	{
	asset.setObject(new Asset(aAssetId, (const char *) anAsset->name, aContent), true);

	for (xmlAttrPtr attr = anAsset->properties; attr != NULL; attr = attr->next)
	{
		if (attr->type == XML_ATTRIBUTE_NODE)
		{
		asset->addIdentity(((const char *) attr->name), ((const char *) attr->children->content));
		}
	}
	}

	return asset;
}


CuttingToolPtr XmlParser::handleCuttingTool(xmlNodePtr anAsset, xmlDocPtr aDoc)
{
	CuttingToolPtr tool;

	// We only handle cuttng tools for now...
	if (xmlStrcmp(anAsset->name, BAD_CAST "CuttingTool") == 0 ||
	xmlStrcmp(anAsset->name, BAD_CAST "CuttingToolArchetype") == 0)
	{
	// Get the attributes...
	tool.setObject(new CuttingTool("", (const char *) anAsset->name, ""), true);

	for (xmlAttrPtr attr = anAsset->properties; attr != NULL; attr = attr->next)
	{
		if (attr->type == XML_ATTRIBUTE_NODE)
		{
		tool->addIdentity((const char *) attr->name, (const char *) attr->children->content);
		}
	}

	if (anAsset->children != NULL)
	{
		for (xmlNodePtr child = anAsset->children; child != NULL; child = child->next)
		{
		if (xmlStrcmp(child->name, BAD_CAST "AssetArchetypeRef") == 0)
		{
			XmlAttributes attrs;

			for (xmlAttrPtr attr = child->properties; attr != NULL; attr = attr->next)
			{
			if (attr->type == XML_ATTRIBUTE_NODE)
			{
				attrs[(const char *) attr->name] = (const char *) attr->children->content;
			}
			}

			tool->setArchetype(attrs);
		}
		else if (xmlStrcmp(child->name, BAD_CAST "Description") == 0)
		{
			xmlChar *text = xmlNodeGetContent(child);

			if (text != NULL)
			{
			tool->setDescription((const char *) text);
			xmlFree(text);
			}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "CuttingToolDefinition") == 0)
		{
			xmlChar *text = xmlNodeGetContent(child);

			if (text != NULL)
			{
			tool->addValue(parseCuttingToolNode(child, aDoc));
			xmlFree(text);
			}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "CuttingToolLifeCycle") == 0)
		{
			parseCuttingToolLife(tool, child, aDoc);
		}
		else if (xmlStrcmp(child->name, BAD_CAST "text") != 0)
		{
			xmlChar *text = xmlNodeGetContent(child);

			if (text != NULL)
			{
			tool->addValue(parseCuttingToolNode(child, aDoc));
			xmlFree(text);
			}
		}
		}
	}
	}

	return tool;
}

void XmlParser::updateAsset(AssetPtr aAsset, const std::string &aType, const std::string &aContent)
{
	if (aType != "CuttingTool" && aType != "CuttingToolArchetype")
	{
	sLogger << dlib::LWARN << "Cannot update asset: " << aType
		<< " is unsupported for incremental updates";
	return;
	}

	xmlDocPtr document = NULL;
	CuttingToolPtr ptr = (CuttingTool *) aAsset.getObject();

	try
	{
	THROW_IF_XML2_NULL(document = xmlReadDoc(BAD_CAST aContent.c_str(), "file://node.xml",
					  NULL, XML_PARSE_NOBLANKS));

	xmlNodePtr root = xmlDocGetRootElement(document);

	if (xmlStrcmp(BAD_CAST "CuttingItem", root->name) == 0)
	{
		CuttingItemPtr item = parseCuttingItem(root, document);

		for (size_t i = 0; i < ptr->m_items.size(); i++)
		{
		if (item->m_identity["indices"] == ptr->m_items[i]->m_identity["indices"])
		{
			ptr->m_items[i] = item;
			break;
		}
		}
	}
	else
	{
		CuttingToolValuePtr value = parseCuttingToolNode(root, document);

		if (ptr->m_values.count(value->m_key) > 0)
		ptr->addValue(value);
		else if (ptr->m_measurements.count(value->m_key) > 0)
		ptr->m_measurements[value->m_key] = value;
	}

	ptr->changed();

	// Cleanup objects...
	xmlFreeDoc(document);

	}
	catch (string e)
	{
	if (document != NULL)
		xmlFreeDoc(document);

	sLogger << dlib::LERROR << "Cannot parse asset XML: " << e;
	}
	catch (...)
	{
	if (document != NULL)
		xmlFreeDoc(document);
	}
}
