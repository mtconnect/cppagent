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

static dlib::logger g_logger("xml.parser");

#define strstrfy(x) #x
#define strfy(x) strstrfy(x)
#define THROW_IF_XML2_ERROR(expr) \
if ((expr) < 0) { throw runtime_error("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }
#define THROW_IF_XML2_NULL(expr) \
if (!(expr)) { throw runtime_error("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }

extern "C" void XMLCDECL
agentXMLErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...)
{
	va_list args;

	char buffer[2048] = {0};
	va_start(args, msg);
	vsnprintf(buffer, 2046u, msg, args);
	buffer[2047] = '0';
	va_end(args);

	g_logger << dlib::LERROR << "XML: " << buffer;
}


XmlParser::XmlParser() :
	m_doc(nullptr)
{
}


std::vector<Device *> XmlParser::parseFile(const std::string &filePath)
{
	if (m_doc)
	{
		xmlFreeDoc(m_doc);
		m_doc = nullptr;
	}

	xmlXPathContextPtr xpathCtx = nullptr;
	xmlXPathObjectPtr devices = nullptr;
	std::vector<Device *> deviceList;

	try
	{
		xmlInitParser();
		xmlXPathInit();
		xmlSetGenericErrorFunc(nullptr, agentXMLErrorFunc);

		THROW_IF_XML2_NULL(m_doc = xmlReadFile(filePath.c_str(), nullptr,
											  XML_PARSE_NOBLANKS));

		std::string path = "//Devices/*";
		THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(m_doc));

		auto root = xmlDocGetRootElement(m_doc);

		if (root->ns)
		{
			path = addNamespace(path, "m");
			THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));

			// Get schema version from Devices.xml
			if (XmlPrinter::getSchemaVersion().empty())
			{
				string ns((const char *)root->ns->href);

				if (!ns.find_first_of("urn:mtconnect.org:MTConnectDevices"))
				{
					auto last = ns.find_last_of(':');

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
		auto location = (const char *) xmlGetProp(root, BAD_CAST "schemaLocation");

		if (location && strncmp(location, "urn:mtconnect.org:MTConnectDevices", 34u))
		{
			string loc = location;
			auto pos = loc.find(' ');

			if (pos != string::npos)
			{
				locationUrn = loc.substr(0, pos);
				auto uri = loc.substr(pos + 1);

				// Try to find the prefix for this urn...
				auto ns = xmlSearchNsByHref(m_doc, root, BAD_CAST locationUrn.c_str());
				string prefix;

				if (ns->prefix)
					prefix = (const char *) ns->prefix;

				XmlPrinter::addDevicesNamespace(locationUrn, uri, prefix);
			}
		}

		// Add the rest of the namespaces...
		if (root->nsDef)
		{
			auto ns = root->nsDef;

			while (ns)
			{
				// Skip the standard namespaces for MTConnect and the w3c. Make sure we don't re-add the
				// schema location again.
				if (!isMTConnectUrn((const char *) ns->href) &&
					strncmp((const char *) ns->href, "http://www.w3.org/", 18u) &&
					locationUrn != (const char *) ns->href &&
					ns->prefix)
				{
					string urn = (const char *) ns->href;
					string prefix = (const char *) ns->prefix;
					XmlPrinter::addDevicesNamespace(urn, "", prefix);
				}

				ns = ns->next;
			}
		}

		devices = xmlXPathEval(BAD_CAST path.c_str(), xpathCtx);

		if (!devices)
			throw (string) xpathCtx->lastError.message;

		xmlNodeSetPtr nodeset = devices->nodesetval;

		if (!nodeset || !nodeset->nodeNr)
			throw (string) "Could not find Device in XML configuration";

		// Collect the Devices...
		for (int i = 0; i != nodeset->nodeNr; ++i)
			deviceList.push_back(static_cast<Device *>(handleComponent(nodeset->nodeTab[i])));

		xmlXPathFreeObject(devices);
		xmlXPathFreeContext(xpathCtx);
	}
	catch (string e)
	{
		if (devices)
			xmlXPathFreeObject(devices);

		if (xpathCtx)
			xmlXPathFreeContext(xpathCtx);

		g_logger << dlib::LFATAL << "Cannot parse XML file: " << e;
		throw e;
	}
	catch (...)
	{
		if (devices)
			xmlXPathFreeObject(devices);

		if (xpathCtx)
			xmlXPathFreeContext(xpathCtx);

		throw;
	}

	return deviceList;
}


XmlParser::~XmlParser()
{
	if (m_doc)
	{
		xmlFreeDoc(m_doc);
		m_doc = nullptr;
	}
}


void XmlParser::loadDocument(const std::string &doc)
{
	if (m_doc)
	{
		xmlFreeDoc(m_doc);
		m_doc = nullptr;
	}

	try
	{
		xmlInitParser();
		xmlXPathInit();
		xmlSetGenericErrorFunc(nullptr, agentXMLErrorFunc);

		THROW_IF_XML2_NULL(m_doc = xmlReadMemory(doc.c_str(), doc.length(),
												"Devices.xml", nullptr, XML_PARSE_NOBLANKS));
	}

	catch (string e)
	{
		g_logger << dlib::LFATAL << "Cannot parse XML document: " << e;
		throw e;
	}
}


void XmlParser::updateDevice(Device *device)
{
	// Update the dom for this device...
}


void XmlParser::getDataItems(
	set<string> &filterSet,
	const string &inputPath,
	xmlNodePtr node)
{
	xmlNodePtr root = xmlDocGetRootElement(m_doc);

	if (!node)
		node = root;

	xmlXPathContextPtr xpathCtx = nullptr;
	xmlXPathObjectPtr objs = nullptr;

	try
	{
		string path;
		THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(m_doc));
		xpathCtx->node = node;
		bool mtc = false;

		if (root->ns)
		{
			for (xmlNsPtr ns = root->nsDef; ns; ns = ns->next)
			{
				if (ns->prefix)
				{
					if (strncmp((const char *) ns->href, "urn:mtconnect.org:MTConnectDevices", 34u))
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
			{
				THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
			}

			path = addNamespace(inputPath, "m");
		}
		else
			path = inputPath;

		objs = xmlXPathEvalExpression(BAD_CAST path.c_str(), xpathCtx);

		if (!objs)
		{
			xmlXPathFreeContext(xpathCtx);
			return;
		}

		xmlNodeSetPtr nodeset = objs->nodesetval;

		if (nodeset)
		{
			for (int i = 0; i != nodeset->nodeNr; ++i)
			{
				xmlNodePtr n = nodeset->nodeTab[i];

				if (!xmlStrcmp(n->name, BAD_CAST "DataItem"))
				{
					auto id = xmlGetProp(n, BAD_CAST "id");

					if (id)
					{
						filterSet.insert((const char *) id);
						xmlFree(id); id = nullptr;
					}
				}
				else if (!xmlStrcmp(n->name, BAD_CAST "DataItems"))
				{
					// Handle case where we are specifying the data items node...
					getDataItems(filterSet, "DataItem", n);
				}
        else if (!xmlStrcmp(n->name, BAD_CAST "Reference"))
        {
					auto id = xmlGetProp(n, BAD_CAST "dataItemId");

					if (id)
					{
						filterSet.insert((const char *) id);
						xmlFree(id); id = nullptr;
					}
				}
        else if (!xmlStrcmp(n->name, BAD_CAST "DataItemRef"))
        {
          auto id = xmlGetProp(n, BAD_CAST "idRef");
          
          if (id)
          {
            filterSet.insert((const char *) id);
            xmlFree(id); id = nullptr;
          }
        }
        else if (!xmlStrcmp(n->name, BAD_CAST "ComponentRef"))
        {
          auto id = string((const char*) xmlGetProp(n, BAD_CAST "idRef"));
          getDataItems(filterSet, "//*[@id='" + id + "']");
        }
				else // Find all the data items and references below this node
				{
					getDataItems(filterSet, "*//DataItem", n);
					getDataItems(filterSet, "*//Reference", n);
          getDataItems(filterSet, "*//DataItemRef", n);
          getDataItems(filterSet, "*//ComponentRef", n);
				}
			}
		}

		xmlXPathFreeObject(objs);
		xmlXPathFreeContext(xpathCtx);
	}
	catch (...)
	{
		if (objs)
			xmlXPathFreeObject(objs);

		if (xpathCtx)
			xmlXPathFreeContext(xpathCtx);

		g_logger << dlib::LWARN << "getDataItems: Could not parse path: " << inputPath;
	}
}


Component *XmlParser::handleComponent(
	xmlNodePtr component,
	Component *parent,
	Device *device
)
{
	Component *toReturn = nullptr;
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
  case Component::DATA_ITEM_REF:
  case Component::COMPONENT_REF:
		handleReference(component, parent, device);
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
	if (toReturn && parent)
	{
		parent->addChild(*toReturn);
		toReturn->setParent(*parent);
	}

	// Check if there are children
	if (toReturn && component->children)
	{
		for (xmlNodePtr child = component->children; child; child = child->next)
		{
			if (child->type != XML_ELEMENT_NODE)
				continue;

			if (!xmlStrcmp(child->name, BAD_CAST "Description"))
			{
				auto text = xmlNodeGetContent(child);

				if (text)
				{
					toReturn->addDescription((string)(const char *) text, getAttributes(child));
					xmlFree(text); text = nullptr;
				}

			}
			else if (!xmlStrcmp(child->name, BAD_CAST "Configuration"))
			{
				xmlNodePtr config = child->children;
				xmlBufferPtr buf;
				THROW_IF_XML2_NULL(buf = xmlBufferCreate());
				auto count = xmlNodeDump(buf, config->doc, config, 0, 0);

				if (count > 0)
					toReturn->setConfiguration((string)(const char *) buf->content);

				xmlBufferFree(buf);
			}
			else
				handleComponent(child, toReturn, device);
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
	auto attributes = getAttributes(node);

	switch (spec)
	{
	case Component::DEVICE:
		return new Device(attributes);

	default:
		string prefix;

		if (node->ns && 
			node->ns->prefix &&
			strncmp((const char *) node->ns->href, "urn:mtconnect.org:MTConnectDevices", 34u))
		{
			prefix = (const char *) node->ns->prefix;
		}

		return new Component(name, attributes, prefix);
	}
}


std::map<string, string> XmlParser::getAttributes(const xmlNodePtr node)
{
	std::map<string, string> toReturn;

	for (xmlAttrPtr attr = node->properties; attr; attr = attr->next)
	{
		if (attr->type == XML_ATTRIBUTE_NODE)
			toReturn[(const char *) attr->name] = (const char *) attr->children->content;
	}

	return toReturn;
}


void XmlParser::loadDataItem(
	xmlNodePtr dataItem,
	Component *parent,
	Device *device
)
{
	auto d = new DataItem(getAttributes(dataItem));
	d->setComponent(*parent);

	if (dataItem->children)
	{

		for (xmlNodePtr child = dataItem->children; child; child = child->next)
		{
			if (child->type != XML_ELEMENT_NODE)
				continue;

			if (!xmlStrcmp(child->name, BAD_CAST "Source"))
			{
				auto text = xmlNodeGetContent(child);
        auto attributes = getAttributes(child);
        
        string cdata;
        if (text != nullptr)
          cdata = (const char*) text;

        string dataItemId, componentId, compositionId;
        auto it = attributes.find("dataItemId");
        if (it != attributes.end()) dataItemId = it->second;
        it = attributes.find("componentId");
        if (it != attributes.end()) componentId = it->second;
        it = attributes.find("compositionId");
        if (it != attributes.end()) compositionId = it->second;

        d->addSource(cdata, dataItemId, componentId, compositionId);
        xmlFree(text); text = nullptr;
			}
			else if (!xmlStrcmp(child->name, BAD_CAST "Constraints"))
			{
				for (xmlNodePtr constraint = child->children; constraint; constraint = constraint->next)
				{
					if (constraint->type != XML_ELEMENT_NODE)
						continue;

					auto text = xmlNodeGetContent(constraint);

					if (!text)
						continue;

					if (!xmlStrcmp(constraint->name, BAD_CAST "Value"))
						d->addConstrainedValue((const char *) text);
					else if (!xmlStrcmp(constraint->name, BAD_CAST "Minimum"))
						d->setMinimum((const char *) text);
					else if (!xmlStrcmp(constraint->name, BAD_CAST "Maximum"))
						d->setMaximum((const char *) text);
					else if (!xmlStrcmp(constraint->name, BAD_CAST "Filter"))
						d->setMinmumDelta(strtod((const char *) text, nullptr));

					xmlFree(text); text = nullptr;
				}
			}
			else if (!xmlStrcmp(child->name, BAD_CAST "Filters"))
			{
				for (xmlNodePtr filter = child->children; filter; filter = filter->next)
				{
					if (filter->type != XML_ELEMENT_NODE)
						continue;

					if (!xmlStrcmp(filter->name, BAD_CAST "Filter"))
					{
						xmlChar *text = xmlNodeGetContent(filter);
						xmlChar *type = xmlGetProp(filter, BAD_CAST "type");

						if (type)
						{
							if (!xmlStrcmp(type, BAD_CAST "PERIOD"))
								d->setMinmumPeriod(strtod((const char *) text, nullptr));
							else
								d->setMinmumDelta(strtod((const char *) text, nullptr));

							xmlFree(type); type = nullptr;
						}
						else
							d->setMinmumDelta(strtod((const char *) text, nullptr));

						xmlFree(text); text = nullptr;
					}
				}
			}
			else if (!xmlStrcmp(child->name, BAD_CAST "InitialValue"))
			{
				auto text = xmlNodeGetContent(child);
				d->setInitialValue(string((const char *)text));
				xmlFree(text); text = nullptr;
			}
			else if (!xmlStrcmp(child->name, BAD_CAST "ResetTrigger"))
			{
				auto text = xmlNodeGetContent(child);
				d->setResetTrigger(string((const char *)text));
				xmlFree(text); text = nullptr;
			}
		}
	}

	parent->addDataItem(*d);
	device->addDeviceDataItem(*d);
}


void XmlParser::handleComposition(xmlNodePtr composition,
				  Component *parent)
{
	auto comp = new Composition(getAttributes(composition));

	for (xmlNodePtr child = composition->children; child; child = child->next)
	{
		if (!xmlStrcmp(child->name, BAD_CAST "Description"))
		{
			auto text = xmlNodeGetContent(child);
			string body;

			if (text)
			{
				body = string(static_cast<const char *>(static_cast<void *>(text)));
				xmlFree(text); text = nullptr;
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
	Device *device )
{
	for (xmlNodePtr child = components->children; child; child = child->next)
	{
		if (child->type != XML_ELEMENT_NODE)
			continue;

		handleComponent(child, parent, device);
	}
}


void XmlParser::handleReference(
	xmlNodePtr reference,
	Component *parent,
	Device *device)
{
	auto attrs = getAttributes(reference);
	string name;

	if (attrs.count("name") > 0)
		name = attrs["name"];

  if (xmlStrcmp(reference->name, BAD_CAST "Reference") == 0) {
    Component::Reference ref(attrs["dataItemId"], name,
                             Component::Reference::DATA_ITEM);
    parent->addReference(ref);
  } else if (xmlStrcmp(reference->name, BAD_CAST "DataItemRef") == 0){
    Component::Reference ref(attrs["idRef"], name,
                             Component::Reference::DATA_ITEM);
    parent->addReference(ref);
  } else if (xmlStrcmp(reference->name, BAD_CAST "ComponentRef") == 0){
    Component::Reference ref(attrs["idRef"], name,
                             Component::Reference::COMPONENT);
    parent->addReference(ref);
  }
}


AssetPtr XmlParser::parseAsset(
	const std::string &assetId,
	const std::string &type,
	const std::string &content)
{
	AssetPtr asset;

	xmlXPathContextPtr xpathCtx = nullptr;
	xmlXPathObjectPtr assetNodes = nullptr;
	xmlDocPtr document = nullptr;
	xmlBufferPtr buffer = nullptr;

	try
	{
		// TODO: Check for asset fragment - check if top node is MTConnectAssets
		// If we don't have complete doc, parse as a fragment and create a top level node
		// adding namespaces from the printer namespaces and then using xmlParseInNodeContext.
		// This will solve fragment xml namespace issues (unless the fragment has namespace)
		// definition.

		THROW_IF_XML2_NULL(document = xmlReadDoc(BAD_CAST content.c_str(),
			((string) "file://" + assetId + ".xml").c_str(),
			nullptr, XML_PARSE_NOBLANKS));

		std::string path = "//Assets/*";
		THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(document));

		auto root = xmlDocGetRootElement(document);

		if (root->ns)
		{
			path = addNamespace(path, "m");
			THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
		}

		// Spin through all the assets and create cutting tool objects for the cutting tools
		// all others add as plain text.
		xmlNodePtr node = nullptr;
		assetNodes = xmlXPathEval(BAD_CAST path.c_str(), xpathCtx);

		if (!assetNodes ||
			!assetNodes->nodesetval ||
			!assetNodes->nodesetval->nodeNr)
		{
			// See if this is a fragment... the root node will be check when it is
			// parsed...
			node = root;
		}
		else
		{
			auto nodeset = assetNodes->nodesetval;
			node = nodeset->nodeTab[0];
		}

		THROW_IF_XML2_NULL(buffer = xmlBufferCreate());

		for (xmlNodePtr child = node->children; child; child = child->next)
			xmlNodeDump(buffer, document, child, 0, 0);

		asset = handleAsset(node,
			assetId,
			type,
			(const char *) buffer->content,
			document);

		// Cleanup objects...
		xmlBufferFree(buffer); buffer = nullptr;
		xmlXPathFreeObject(assetNodes); assetNodes = nullptr;
		xmlXPathFreeContext(xpathCtx); xpathCtx = nullptr;
		xmlFreeDoc(document); document = nullptr;

	}
	catch (string e)
	{
		if (assetNodes)
		{
			xmlXPathFreeObject(assetNodes);
			assetNodes = nullptr;
		}

		if (xpathCtx)
		{
			xmlXPathFreeContext(xpathCtx);
			xpathCtx = nullptr;
		}

		if (document)
		{
			xmlFreeDoc(document);
			document = nullptr;
		}

		if (buffer)
		{
			xmlBufferFree(buffer);
			buffer = nullptr;
		}

		g_logger << dlib::LERROR << "Cannot parse asset XML: " << e;
		asset = nullptr;
	}
	catch (...)
	{
		if (assetNodes)
		{
			xmlXPathFreeObject(assetNodes);
			assetNodes = nullptr;
		}

		if (xpathCtx)
		{
			xmlXPathFreeContext(xpathCtx);
			xpathCtx = nullptr;
		}

		if (document)
		{
			xmlFreeDoc(document);
			document = nullptr;
		}

		if (buffer)
		{
			xmlBufferFree(buffer);
			buffer = nullptr;
		}

		g_logger << dlib::LERROR << "Cannot parse asset XML, Unknown execption occurred";
		asset = nullptr;
	}

	return asset;
}


CuttingToolValuePtr XmlParser::parseCuttingToolNode(xmlNodePtr node, xmlDocPtr doc)
{
	CuttingToolValuePtr value(new CuttingToolValue(), true);
	string name;

	if (node->ns && node->ns->prefix)
	{
		name = (const char *) node->ns->prefix;
		name += ':';
	}

	name += (const char *) node->name;
	value->m_key = name;

	if (!node->children)
	{
		auto text = xmlNodeGetContent(node);

		if (text)
		{
			value->m_value = (char *) text;
			xmlFree(text); text = nullptr;
		}
	}
	else
	{
		xmlBufferPtr buffer;
		THROW_IF_XML2_NULL(buffer = xmlBufferCreate());

		for (xmlNodePtr child = node->children; child; child = child->next)
			xmlNodeDump(buffer, doc, child, 0, 0);

		value->m_value = (char *) buffer->content;
		xmlBufferFree(buffer); buffer = nullptr;
	}

	for (xmlAttrPtr attr = node->properties; attr; attr = attr->next)
	{
		if (attr->type == XML_ATTRIBUTE_NODE)
			value->m_properties[(const char *) attr->name] = (const char *) attr->children->content;
	}

	return value;
}


CuttingItemPtr XmlParser::parseCuttingItem(xmlNodePtr node, xmlDocPtr doc)
{
	CuttingItemPtr item(new CuttingItem(), true);

	for (xmlAttrPtr attr = node->properties; attr; attr = attr->next)
	{
		if (attr->type == XML_ATTRIBUTE_NODE)
			item->m_identity[(const char *) attr->name] = (const char *) attr->children->content;
	}

	for (xmlNodePtr child = node->children; child; child = child->next)
	{
		if (!xmlStrcmp(child->name, BAD_CAST "Measurements"))
		{
			for (xmlNodePtr meas = child->children; meas; meas = meas->next)
			{
				CuttingToolValuePtr value = parseCuttingToolNode(meas, doc);
				item->m_measurements[value->m_key] = value;
			}
		}
		else if (!xmlStrcmp(child->name, BAD_CAST "ItemLife"))
		{
			CuttingToolValuePtr value = parseCuttingToolNode(child, doc);
			item->m_lives.push_back(value);
		}
		else if (xmlStrcmp(child->name, BAD_CAST "text"))
		{
			CuttingToolValuePtr value = parseCuttingToolNode(child, doc);
			item->m_values[value->m_key] = value;
		}
	}

	return item;
}


void XmlParser::parseCuttingToolLife(CuttingToolPtr tool, xmlNodePtr node, xmlDocPtr doc)
{
	for (xmlNodePtr child = node->children; child; child = child->next)
	{
		if (!xmlStrcmp(child->name, BAD_CAST "CuttingItems"))
		{
			for (xmlAttrPtr attr = child->properties; attr; attr = attr->next)
			{
				if (attr->type == XML_ATTRIBUTE_NODE &&
					!xmlStrcmp(attr->name, BAD_CAST "count"))
				{
					tool->m_itemCount = (const char *) attr->children->content;
				}
			}

			for (xmlNodePtr itemNode = child->children; itemNode; itemNode = itemNode->next)
			{
				if (!xmlStrcmp(itemNode->name, BAD_CAST "CuttingItem"))
				{
					CuttingItemPtr item = parseCuttingItem(itemNode, doc);
					tool->m_items.push_back(item);
				}
			}
		}
		else if (!xmlStrcmp(child->name, BAD_CAST "Measurements"))
		{
			for (xmlNodePtr meas = child->children; meas; meas = meas->next)
			{
				if (xmlStrcmp(meas->name, BAD_CAST "text"))
				{
					CuttingToolValuePtr value = parseCuttingToolNode(meas, doc);
					tool->m_measurements[value->m_key] = value;
				}
			}
		}
		else if (!xmlStrcmp(child->name, BAD_CAST "CutterStatus"))
		{
			for (xmlNodePtr status = child->children; status; status = status->next)
			{
				if (!xmlStrcmp(status->name, BAD_CAST "Status"))
				{
					xmlChar *text = xmlNodeGetContent(status);

					if (text)
					{
						tool->m_status.push_back((const char *) text);
						xmlFree(text); text = nullptr;
					}
				}
			}
		}
		else if (!xmlStrcmp(child->name, BAD_CAST "ToolLife"))
		{
			CuttingToolValuePtr value = parseCuttingToolNode(child, doc);
			tool->m_lives.push_back(value);
		}
		else if (xmlStrcmp(child->name, BAD_CAST "text"))
			tool->addValue(parseCuttingToolNode(child, doc));
	}
}


AssetPtr XmlParser::handleAsset(
	xmlNodePtr inputAsset,
	const std::string &assetId,
	const std::string &type,
	const std::string &content,
	xmlDocPtr doc)
{
	AssetPtr asset;

	// We only handle cuttng tools for now...
	if (!xmlStrcmp(inputAsset->name, BAD_CAST "CuttingTool") ||
		!xmlStrcmp(inputAsset->name, BAD_CAST "CuttingToolArchetype") )
	{
		asset = handleCuttingTool(inputAsset, doc);
	}
	else
	{
		asset.setObject(new Asset(assetId, (const char *) inputAsset->name, content), true);

		for (xmlAttrPtr attr = inputAsset->properties; attr; attr = attr->next)
		{
			if (attr->type == XML_ATTRIBUTE_NODE)
				asset->addIdentity(((const char *) attr->name), ((const char *) attr->children->content));
		}
	}

	return asset;
}


CuttingToolPtr XmlParser::handleCuttingTool(xmlNodePtr asset, xmlDocPtr doc)
{
	CuttingToolPtr tool;

	// We only handle cuttng tools for now...
	if (!xmlStrcmp(asset->name, BAD_CAST "CuttingTool") ||
		!xmlStrcmp(asset->name, BAD_CAST "CuttingToolArchetype") )
	{
		// Get the attributes...
		tool.setObject(new CuttingTool("", (const char *) asset->name, ""), true);

		for (xmlAttrPtr attr = asset->properties; attr; attr = attr->next)
		{
			if (attr->type == XML_ATTRIBUTE_NODE)
				tool->addIdentity((const char *) attr->name, (const char *) attr->children->content);
		}

		if (asset->children)
		{
			for (xmlNodePtr child = asset->children; child; child = child->next)
			{
				if (!xmlStrcmp(child->name, BAD_CAST "AssetArchetypeRef"))
				{
					XmlAttributes attrs;

					for (xmlAttrPtr attr = child->properties; attr; attr = attr->next)
					{
						if (attr->type == XML_ATTRIBUTE_NODE)
							attrs[(const char *) attr->name] = (const char *) attr->children->content;
					}

					tool->setArchetype(attrs);
				}
				else if (!xmlStrcmp(child->name, BAD_CAST "Description"))
				{
					auto text = xmlNodeGetContent(child);

					if (text)
					{
						tool->setDescription((const char *) text);
						xmlFree(text); text = nullptr;
					}
				}
				else if (!xmlStrcmp(child->name, BAD_CAST "CuttingToolDefinition"))
				{
					auto text = xmlNodeGetContent(child);

					if (text)
					{
						tool->addValue(parseCuttingToolNode(child, doc));
						xmlFree(text); text = nullptr;
					}
				}
				else if (!xmlStrcmp(child->name, BAD_CAST "CuttingToolLifeCycle"))
				{
					parseCuttingToolLife(tool, child, doc);
				}
				else if (xmlStrcmp(child->name, BAD_CAST "text"))
				{
					auto text = xmlNodeGetContent(child);

					if (text)
					{
						tool->addValue(parseCuttingToolNode(child, doc));
						xmlFree(text); text = nullptr;
					}
				}
			}
		}
	}

	return tool;
}


void XmlParser::updateAsset(
	AssetPtr asset,
	const std::string &type,
	const std::string &content)
{
	if (type != "CuttingTool" && type != "CuttingToolArchetype")
	{
		g_logger << dlib::LWARN << "Cannot update asset: " << type
				<< " is unsupported for incremental updates";
		return;
	}

	xmlDocPtr document = nullptr;
	CuttingToolPtr ptr = (CuttingTool *) asset.getObject();

	try
	{
		THROW_IF_XML2_NULL(document = xmlReadDoc(BAD_CAST content.c_str(), "file://node.xml",
						  nullptr, XML_PARSE_NOBLANKS));

		auto root = xmlDocGetRootElement(document);

		if (!xmlStrcmp(BAD_CAST "CuttingItem", root->name))
		{
			auto item = parseCuttingItem(root, document);

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
			auto value = parseCuttingToolNode(root, document);

			if (ptr->m_values.count(value->m_key) > 0)
				ptr->addValue(value);
			else if (ptr->m_measurements.count(value->m_key) > 0)
				ptr->m_measurements[value->m_key] = value;
		}

		ptr->changed();

		// Cleanup objects...
		xmlFreeDoc(document); document = nullptr;

	}
	catch (string e)
	{
		if (document)
		{
			xmlFreeDoc(document);
			document = nullptr;
		}

		g_logger << dlib::LERROR << "Cannot parse asset XML: " << e;
	}
	catch (...)
	{
		if (document)
		{
			xmlFreeDoc(document);
			document = nullptr;
		}
	}
}
