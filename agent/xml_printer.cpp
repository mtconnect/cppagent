/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "xml_printer.hpp"

using namespace std;

/* XmlPrinter main methods */
string XmlPrinter::printError(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const unsigned int nextSeq,
    const string& errorCode,
    const string& errorText
  )
{
  xmlpp::Document * mErrorXml = initXmlDoc(
    "Error",
    instanceId,
    bufferSize,
    nextSeq
  );
  
  xmlpp::Element * error = mErrorXml->get_root_node()->add_child("Error");
  error->set_attribute("errorCode", errorCode);
  error->set_child_text(errorText);
  
  string toReturn = printNode(mErrorXml->get_root_node());
  
  delete mErrorXml;
  
  return appendXmlEncode(toReturn);
}

string XmlPrinter::printProbe(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const unsigned int nextSeq,
    list<Device *>& deviceList
  )
{
  xmlpp::Document *mProbeXml = initXmlDoc(
    "Devices",
    instanceId,
    bufferSize,
    nextSeq
  );
  
  xmlpp::Element *devices = mProbeXml->get_root_node()->add_child("Devices");
  
  list<Device *>::iterator dev;
  for (dev = deviceList.begin(); dev != deviceList.end(); dev++)
  {
    xmlpp::Element *device = devices->add_child("Device");
    printProbeHelper(device, *dev);
  }
  
  string toReturn = printNode(mProbeXml->get_root_node());
  
  delete mProbeXml;
  
  return appendXmlEncode(toReturn);
}

string XmlPrinter::printCurrent(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const unsigned int nextSeq,
    const unsigned int firstSeq,
    list<DataItem *>& dataItems
  )
{
  xmlpp::Document *mSampleXml = initXmlDoc(
    "Streams",
    instanceId,
    bufferSize,
    nextSeq,
    firstSeq
  );
  
  xmlpp::Element *streams = mSampleXml->get_root_node()->add_child("Streams");
  std::map<string, xmlpp::Element *> elements; 
  std::map<string, xmlpp::Element *> components; 
  std::map<string, xmlpp::Element *> devices; 
 
  list<DataItem *>::iterator dataItem;
  for (dataItem = dataItems.begin(); dataItem != dataItems.end(); dataItem++)
  {
    if ((*dataItem)->getLatestEvent() != NULL)
    {
      addElement((*dataItem)->getLatestEvent(),
		 streams, elements, components, devices);
      
    }
  }
  
  string toReturn = printNode(mSampleXml->get_root_node());
  
  delete mSampleXml;
  
  return appendXmlEncode(toReturn);
}

string XmlPrinter::printSample(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const unsigned int nextSeq,
    const unsigned int firstSeq,
    list<ComponentEvent *>& results
  )
{
  xmlpp::Document * mSampleXml = initXmlDoc(
    "Streams",
    instanceId,
    bufferSize,
    nextSeq,
    firstSeq
  );
    
  xmlpp::Element *streams = mSampleXml->get_root_node()->add_child("Streams");
  std::map<string, xmlpp::Element *> elements;
  std::map<string, xmlpp::Element *> components; 
  std::map<string, xmlpp::Element *> devices; 
  
  list<ComponentEvent *>::iterator result;
  for (result = results.begin(); result != results.end(); result++)
  {
    addElement(*result, streams, elements, components, devices);
  }
  
  string toReturn = printNode(mSampleXml->get_root_node());
  
  delete mSampleXml;
  
  return appendXmlEncode(toReturn);
}

/* XmlPrinter helper Methods */
xmlpp::Document * XmlPrinter::initXmlDoc(
    const string& xmlType,
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const unsigned int nextSeq,
    const unsigned int firstSeq
  )
{
  xmlpp::Document *doc = new xmlpp::Document;
  
  string rootName = "MTConnect" + xmlType;
  string xmlns = "urn:mtconnect.com:MTConnect" + xmlType + ":1.0";
  string xsi = "urn:mtconnect.com:MTConnect" + xmlType +
    ":1.0 http://www.mtconnect.org/schemas/MTConnect" + xmlType + ".xsd";
  
  // Root
  xmlpp::Element * root = doc->create_root_node(rootName);
  root->set_attribute("xmlns:m", xmlns);
  root->set_attribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
  root->set_attribute("xmlns", xmlns);
  root->set_attribute("xsi:schemaLocation", xsi);
  
  // Header
  xmlpp::Element * header = doc->get_root_node()->add_child("Header");
  header->set_attribute("creationTime", getCurrentTime(GMT));
  header->set_attribute("sender", "localhost");
  header->set_attribute("instanceId", intToString(instanceId));
  header->set_attribute("bufferSize", intToString(bufferSize));
  header->set_attribute("version", "1.0");
  
  if (xmlType == "Streams")
  {
    header->set_attribute("nextSequence", intToString(nextSeq));
    header->set_attribute("firstSequence", intToString(firstSeq));
    header->set_attribute("lastSequence", intToString(nextSeq - 1));
  }
  
  return doc;
}

string XmlPrinter::printNode(
    const xmlpp::Node *node,
    const unsigned int indentation
  )
{
  // Constant node data determined for each node
  const xmlpp::TextNode* nodeText = dynamic_cast<const xmlpp::TextNode*>(node);
  const xmlpp::Element* nodeElement = dynamic_cast<const xmlpp::Element*>(node);
  
  xmlpp::Node::NodeList children = node->get_children();
  bool hasChildren = (children.size() > 0);
  
  string toReturn;

  // Ignore empty whitespace
  if (nodeText && nodeText->is_white_space())
  {
    return "";
  }
  
  Glib::ustring nodename = node->get_name();
  
  // Element node: i.e. "<element"
  if (!nodeText && !nodename.empty())
  {
    toReturn += printIndentation(indentation) + "<" + nodename;
  }
  
  if (nodeText)
  {
    string text = nodeText->get_content();
    replaceIllegalCharacters(text);
    toReturn += text;
  }
  else if (nodeElement)
  {
    // Print attributes for the element: i.e. attribute1="value1"
    const xmlpp::Element::AttributeList& attributes =
      nodeElement->get_attributes();
    
    xmlpp::Element::AttributeList::const_iterator attr;
    for (attr = attributes.begin(); attr != attributes.end(); attr++)
    {
      toReturn += " " + (*attr)->get_name() +
       "=\"" + (*attr)->get_value() + "\"";
    }
    
    toReturn += (hasChildren) ? ">" : " />";
    
    if (!nodeElement->has_child_text())
    {
      toReturn += "\n";
    }
  }
  
  // If node does NOT have content, then it may have children
  if (!dynamic_cast<const xmlpp::ContentNode*>(node))
  {
    xmlpp::Node::NodeList::iterator child;
    for (child = children.begin(); child != children.end(); child++)
    {
      toReturn += printNode(*child, indentation + 2);
    }
  }
  
  // Close off xml tag, i.e. </tag>
  if (!nodeText && !nodename.empty() && hasChildren)// || indentation == 0)
  {
    if (!nodeElement->has_child_text())
    {
      toReturn += printIndentation(indentation);
    }
    toReturn += "</" + nodename + ">\n";
  }
  
  return toReturn;
}

void XmlPrinter::printProbeHelper(
    xmlpp::Element * element,
    Component * component
  )
{
  addAttributes(element, component->getAttributes());
    
  std::map<string, string> desc = component->getDescription();
  
  if (desc.size() > 0)
  {
    xmlpp::Element * description = element->add_child("Description");
    addAttributes(description, desc);
  }
  
  list<DataItem *> datum = component->getDataItems();
  if (datum.size() > 0)
  {
    xmlpp::Element * dataItems = element->add_child("DataItems");
    
    list<DataItem *>::iterator data;
    for (data = datum.begin(); data!= datum.end(); data++)
    {
      xmlpp::Element * dataItem = dataItems->add_child("DataItem");
      addAttributes(dataItem, (*data)->getAttributes());
      string source = (*data)->getSource();
      if (!source.empty())
      {
        xmlpp::Element * src = dataItem->add_child("Source");
        src->add_child_text(source);
      }
    }
  }

  list<Component *> children = component->getChildren();
  
  if (children.size() > 0)
  {
    xmlpp::Element * components = element->add_child("Components");
    
    list<Component *>::iterator child;
    for (child = children.begin(); child != children.end(); child++)
    {
      xmlpp::Element * component = components->add_child((*child)->getClass());
      printProbeHelper(component, *child);
    }
  }
}

string XmlPrinter::appendXmlEncode(const string& xml)
{
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + xml;
}

string XmlPrinter::printIndentation(unsigned int indentation)
{
  string indents;
  for (unsigned int i = 0; i < indentation; i++)
  {
    indents += " ";
  }
  return indents;
}

void XmlPrinter::addAttributes(
    xmlpp::Element *element,
    std::map<string, string> attributes
  )
{
  std::map<string, string>::iterator attr;
  for (attr=attributes.begin(); attr!=attributes.end(); attr++)
  {
    element->set_attribute(attr->first, attr->second);
  }
}

xmlpp::Element * XmlPrinter::getDeviceStream(
    xmlpp::Element *element,
    Device *device,
    std::map<string, xmlpp::Element *> &devices
  )
{
  xmlpp::Element *deviceStream = devices[device->getName()];
  if (deviceStream != NULL)
    return deviceStream;
  
  // No element for device found, create a new one
  deviceStream = element->add_child("DeviceStream");
  deviceStream->set_attribute("name", device->getName());
  deviceStream->set_attribute("uuid", device->getUuid());
  devices[device->getName()] = deviceStream;
  
  return deviceStream;
}

void XmlPrinter::addElement(ComponentEvent *result,
			    xmlpp::Element *streams,
			    std::map<string, xmlpp::Element *> &elements,
			    std::map<string, xmlpp::Element *> &components,
			    std::map<string, xmlpp::Element *> &devices)
{
  DataItem *dataItem = result->getDataItem();
  Component *component = dataItem->getComponent();
  bool sample = dataItem->isSample();
  string dataName = (sample) ? "Samples" : "Events";
  string key = component->getId() + dataName;

  xmlpp::Element *element = elements[key];
  xmlpp::Element *child;
  
  if (element == NULL)
  {
    xmlpp::Element * componentStream = components[component->getId()];
    if (componentStream == NULL)
    {
      xmlpp::Element *deviceStream = 
        getDeviceStream(streams, component->getDevice(), devices);
      
      componentStream = deviceStream->add_child("ComponentStream");
      components[component->getId()] = componentStream;
      
      componentStream->set_attribute("component", component->getClass());
      componentStream->set_attribute("name", component->getName());
      componentStream->set_attribute("componentId", component->getId());
    }
    
    elements[key] = element = componentStream->add_child(dataName);
  }

  child = element->add_child(dataItem->getTypeString(false));
  
  if (dataItem->isSample())
  {
    child->add_child_text(floatToString(result->getFValue()));
  }
  else
  {
    child->add_child_text(result->getSValue());
  }
  
  addAttributes(child, result->getAttributes());
}


