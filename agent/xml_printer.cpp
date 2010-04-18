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

#define strfy(line) #line
#define THROW_IF_XML2_ERROR(expr) \
  if ((expr) < 0) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }
#define THROW_IF_XML2_NULL(expr) \
  if ((expr) == NULL) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }

static xmlChar *ConvertInput(const char *in, const char *encoding);

using namespace std;

/* XmlPrinter main methods */
string XmlPrinter::printError(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const Int64 nextSeq,
    const string& errorCode,
    const string& errorText
  )
{
  xmlTextWriterPtr writer;
  xmlBufferPtr buf;
  string ret;
  
  try {
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
  
    initXmlDoc2(writer,
                "Error",
                instanceId,
                bufferSize,
                nextSeq);
  
    xmlChar *text = ConvertInput(errorText.c_str(), "UTF-8");
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Errors"));
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Error"));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "errorCode", 
                                                    BAD_CAST errorCode.c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, text));
    xmlFree(text);

    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Error
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Errors
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnect
    THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
    
    // Cleanup
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);
  }
  catch (string error) {
    cerr << error << endl;
  }
  catch (...) {
    cerr << "Unknown error" << endl;
  }
  
  return ret;
}

string XmlPrinter::printProbe(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const Int64 nextSeq,
    vector<Device *>& deviceList
  )
{
  xmlpp::Document *mProbeXml = initXmlDoc(
    "Devices",
    instanceId,
    bufferSize,
    nextSeq
  );
  
  xmlpp::Element *devices = mProbeXml->get_root_node()->add_child("Devices");
  
  vector<Device *>::iterator dev;
  for (dev = deviceList.begin(); dev != deviceList.end(); dev++)
  {
    xmlpp::Element *device = devices->add_child("Device");
    printProbeHelper(device, *dev);
  }
  
  string toReturn = printNode(mProbeXml->get_root_node());
  
  delete mProbeXml;
  
  return appendXmlEncode(toReturn);
}

string XmlPrinter::printSample(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const Int64 nextSeq,
    const Int64 firstSeq,
    vector<ComponentEventPtr>& results
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

  // Sort the vector by category.
  std::sort(results.begin(), results.end());
  
  vector<ComponentEventPtr>::iterator result;
  for (result = results.begin(); result != results.end(); result++)
  {
    addElement(*result, streams, elements, components, devices);
  }
  
  string toReturn = printNode(mSampleXml->get_root_node());
  
  delete mSampleXml;
  
  return appendXmlEncode(toReturn);
}

/* XmlPrinter helper Methods */
void XmlPrinter::initXmlDoc2(
    xmlTextWriterPtr writer,
    const string& xmlType,
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const Int64 nextSeq,
    const Int64 firstSeq
  )
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL));
  
  // Write the root element
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST ((string) "MTConnect" + xmlType).c_str()));
  
  string rootName = "MTConnect" + xmlType;
  string xmlns = "urn:mtconnect.org:MTConnect" + xmlType + ":1.1";
  string xsi = "urn:mtconnect.org:MTConnect" + xmlType +
    ":1.1 http://www.mtconnect.org/schemas/MTConnect" + xmlType + "_1.1.xsd";
  
  // Add root element attribtues
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xmlns:m",
                                                  BAD_CAST xmlns.c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xmlns",
                                                  BAD_CAST xmlns.c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xsi:schemaLocation",
                                                  BAD_CAST xsi.c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xmlns:xsi",
                                                  BAD_CAST "http://www.w3.org/2001/XMLSchema-instance"));
  
  // Create the header
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Header"));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "creationTime", BAD_CAST getCurrentTime(GMT).c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "sender", BAD_CAST "localhost"));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "instanceId", BAD_CAST intToString(instanceId).c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "bufferSize", BAD_CAST intToString(bufferSize).c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST "1.1"));
    
  // Add additional attribtues for streams
  if (xmlType == "Streams")
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "nextSequence", 
                                                    BAD_CAST int64ToString(nextSeq).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST  "firstSequence", 
                                                    BAD_CAST int64ToString(firstSeq).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "lastSequence", 
                                                    BAD_CAST int64ToString(nextSeq - 1).c_str()));
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
}

/* XmlPrinter helper Methods */
xmlpp::Document * XmlPrinter::initXmlDoc(
    const string& xmlType,
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const Int64 nextSeq,
    const Int64 firstSeq
  )
{
  xmlpp::Document *doc = new xmlpp::Document;
  
  string rootName = "MTConnect" + xmlType;
  string xmlns = "urn:mtconnect.org:MTConnect" + xmlType + ":1.1";
  string xsi = "urn:mtconnect.org:MTConnect" + xmlType +
    ":1.1 http://www.mtconnect.org/schemas/MTConnect" + xmlType + "_1.1.xsd";
  
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
  header->set_attribute("version", "1.1");
  
  if (xmlType == "Streams")
  {
    header->set_attribute("nextSequence", int64ToString(nextSeq));
    header->set_attribute("firstSequence", int64ToString(firstSeq));
    header->set_attribute("lastSequence", int64ToString(nextSeq - 1));
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
    addAttributes(description, &desc);
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
      
      if ((*data)->hasConstraints())
      {
        xmlpp::Element * constraints = dataItem->add_child("Constraints");
        string s = (*data)->getMaximum();
        if (!s.empty())
        {
          xmlpp::Element *v = constraints->add_child("Maximum");
          v->add_child_text(s);
        }          
        s = (*data)->getMinimum();
        if (!s.empty())
        {
          xmlpp::Element *v = constraints->add_child("Minimum");
          v->add_child_text(s);
        }          
        vector<string> values = (*data)->getConstrainedValues();
        vector<string>::iterator value;
        for (value = values.begin(); value != values.end(); value++)
        {
          xmlpp::Element *v = constraints->add_child("Value");
          v->add_child_text(*value);
        }
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
    std::map<string, string> *attributes
  )
{
  std::map<string, string>::iterator attr;
  for (attr= attributes->begin(); attr!=attributes->end(); attr++)
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
  const char *dataName = "";
  switch (dataItem->getCategory())
  {
  case DataItem::SAMPLE:
    dataName = "Samples";
    break;

  case DataItem::EVENT:
    dataName = "Events";
    break;

  case DataItem::CONDITION:
    dataName = "Condition";
    break;
  }
  
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

  if (dataItem->isCondition()) {
    child = element->add_child(result->getLevelString());
  } else {
    child = element->add_child(dataItem->getTypeString(false));
  }
  if (!result->getValue().empty())
    child->add_child_text(result->getValue());
  addAttributes(child, result->getAttributes());
}

/**
 * ConvertInput:
 * @in: string in a given encoding
 * @encoding: the encoding used
 *
 * Converts @in into UTF-8 for processing with libxml2 APIs
 *
 * Returns the converted UTF-8 string, or NULL in case of error.
 */
xmlChar *
ConvertInput(const char *in, const char *encoding)
{
  xmlChar *out;
  int ret;
  int size;
  int out_size;
  int temp;
  xmlCharEncodingHandlerPtr handler;
  
  if (in == 0)
    return 0;
  
  handler = xmlFindCharEncodingHandler(encoding);
  
  if (!handler) {
    printf("ConvertInput: no encoding handler found for '%s'\n",
           encoding ? encoding : "");
    return 0;
  }
  
  size = (int) strlen(in) + 1;
  out_size = size * 2 - 1;
  out = (unsigned char *) xmlMalloc((size_t) out_size);
  
  if (out != 0) {
    temp = size - 1;
    ret = handler->input(out, &out_size, (const xmlChar *) in, &temp);
    if ((ret < 0) || (temp - size + 1)) {
      if (ret < 0) {
        printf("ConvertInput: conversion wasn't successful.\n");
      } else {
        printf
        ("ConvertInput: conversion wasn't successful. converted: %i octets.\n",
         temp);
      }
      
      xmlFree(out);
      out = 0;
    } else {
      out = (unsigned char *) xmlRealloc(out, out_size + 1);
      out[out_size] = 0;  /*null terminating out */
    }
  } else {
    printf("ConvertInput: no mem\n");
  }
  
  return out;
}

