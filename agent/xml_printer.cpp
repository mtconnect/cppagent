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
#include "dlib/sockets.h"
#include "dlib/logger.h"

static dlib::logger sLogger("xml.printer");

#define strfy(line) #line
#define THROW_IF_XML2_ERROR(expr) \
  if ((expr) < 0) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }
#define THROW_IF_XML2_NULL(expr) \
  if ((expr) == NULL) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }

using namespace std;

struct SchemaNamespace {
  string mUrn;
  string mSchemaLocation;
};

static map<string, SchemaNamespace> sDevicesNamespaces;
static map<string, SchemaNamespace> sStreamsNamespaces;
static map<string, SchemaNamespace> sErrorNamespaces;
static map<string, SchemaNamespace> sAssetsNamespaces;

enum EDocumentType {
  eERROR,
  eSTREAMS,
  eDEVICES,
  eASSETS
};

namespace XmlPrinter {
  /***** Helper Methods *****/
  /* Initiate all documents */
  void initXmlDoc(xmlTextWriterPtr writer,
                  EDocumentType aDocType,
                  const unsigned int instanceId,
                  const unsigned int bufferSize,
                  const Int64 nextSeq,
                  const Int64 firstSeq = 0);  

  /* Helper to print individual components and details */
  void printProbeHelper(xmlTextWriterPtr writer, Component *component);
  void printDataItem(xmlTextWriterPtr writer, DataItem *dataItem);


  /* Add attributes to an xml element */
  void addDeviceStream(xmlTextWriterPtr writer, Device *device);
  void addComponentStream(xmlTextWriterPtr writer, Component *component);
  void addCategory(xmlTextWriterPtr writer, DataItem::ECategory category);
  void addSimpleElement(xmlTextWriterPtr writer, std::string element, std::string &body, 
                        std::map<std::string, std::string> *attributes = NULL);

  void addAttributes(xmlTextWriterPtr writer,
                     std::map<std::string, std::string> *attributes);
  void addAttributes(xmlTextWriterPtr writer,
                     AttributeList *attributes);

  void addEvent(xmlTextWriterPtr writer, ComponentEvent *result);
};


void XmlPrinter::addDevicesNamespace(const std::string &aUrn, const std::string &aLocation, 
                         const std::string &aPrefix)
{
  pair<string, SchemaNamespace> item;
  item.second.mUrn = aUrn;
  item.second.mSchemaLocation = aLocation;
  item.first = aPrefix;
  
  sDevicesNamespaces.insert(item);
}

void XmlPrinter::clearDevicesNamespaces()
{
  sDevicesNamespaces.clear();
}

const string XmlPrinter::getDevicesUrn(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sDevicesNamespaces.find(aPrefix);
  if (ns != sDevicesNamespaces.end())
    return ns->second.mUrn;
  else
    return "";
}

void XmlPrinter::addErrorNamespace(const std::string &aUrn, const std::string &aLocation, 
                       const std::string &aPrefix)
{
  pair<string, SchemaNamespace> item;
  item.second.mUrn = aUrn;
  item.second.mSchemaLocation = aLocation;
  item.first = aPrefix;
  
  sErrorNamespaces.insert(item);  
}

void XmlPrinter::clearErrorNamespaces()
{
  sErrorNamespaces.clear();
}

const string XmlPrinter::getErrorUrn(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sErrorNamespaces.find(aPrefix);
  if (ns != sErrorNamespaces.end())
    return ns->second.mUrn;
  else
    return "";
}

void XmlPrinter::addStreamsNamespace(const std::string &aUrn, const std::string &aLocation, 
                         const std::string &aPrefix)
{
  pair<string, SchemaNamespace> item;
  item.second.mUrn = aUrn;
  item.second.mSchemaLocation = aLocation;
  item.first = aPrefix;
  
  sStreamsNamespaces.insert(item);  
}

void XmlPrinter::clearStreamsNamespaces()
{
  sStreamsNamespaces.clear();
}

const string XmlPrinter::getStreamsUrn(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sStreamsNamespaces.find(aPrefix);
  if (ns != sStreamsNamespaces.end())
    return ns->second.mUrn;
  else
    return "";
}

void XmlPrinter::addAssetsNamespace(const std::string &aUrn, const std::string &aLocation, 
                         const std::string &aPrefix)
{
  pair<string, SchemaNamespace> item;
  item.second.mUrn = aUrn;
  item.second.mSchemaLocation = aLocation;
  item.first = aPrefix;
  
  sAssetsNamespaces.insert(item);  
}

void XmlPrinter::clearAssetsNamespaces()
{
  sAssetsNamespaces.clear();
}

const string XmlPrinter::getAssetsUrn(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sAssetsNamespaces.find(aPrefix);
  if (ns != sAssetsNamespaces.end())
    return ns->second.mUrn;
  else
    return "";
}

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
  
    initXmlDoc(writer, eERROR, instanceId,
               bufferSize, nextSeq);
  
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Errors"));
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Error"));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "errorCode", 
                                                    BAD_CAST errorCode.c_str()));
    xmlChar *text = xmlEncodeEntitiesReentrant(NULL, BAD_CAST errorText.c_str());
    THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, text));
    xmlFree(text);

    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Error
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Errors
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectError
    THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
    
    // Cleanup
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);
  }
  catch (string error) {
    sLogger << dlib::LERROR << "printError: " << error;
  }
  catch (...) {
    sLogger << dlib::LERROR << "printError: unknown error";
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
  xmlTextWriterPtr writer;
  xmlBufferPtr buf;
  string ret;
  
  try {
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
    
    initXmlDoc(writer, eDEVICES,
                instanceId,
                bufferSize,
                nextSeq);
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Devices"));
    
    
    vector<Device *>::iterator dev;
    for (dev = deviceList.begin(); dev != deviceList.end(); dev++)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Device"));
      printProbeHelper(writer, *dev);
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Device
    }      

    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Devices
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectDevices
    THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
    
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);    
  }
  catch (string error) {
    sLogger << dlib::LERROR << "printProbe: " << error;
  }
  catch (...) {
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  
  return ret;
 }

void XmlPrinter::printProbeHelper(xmlTextWriterPtr writer,
                                  Component * component)
{
  addAttributes(writer, component->getAttributes());
  
  std::map<string, string> desc = component->getDescription();
  string body = component->getDescriptionBody();
  
  if (desc.size() > 0 || !body.empty())
  {
    addSimpleElement(writer, "Description", body, &desc);
  }
  
  list<DataItem *> datum = component->getDataItems();
  if (datum.size() > 0)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DataItems"));
    
    list<DataItem *>::iterator data;
    for (data = datum.begin(); data!= datum.end(); data++)
    {
      printDataItem(writer, *data);
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DataItems    
  }
  
  list<Component *> children = component->getChildren();
  
  if (children.size() > 0)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Components"));
    
    list<Component *>::iterator child;
    for (child = children.begin(); child != children.end(); child++)
    {
      xmlChar *name = NULL;
      if (!(*child)->getPrefix().empty()) 
      {
        map<string, SchemaNamespace>::iterator ns = sDevicesNamespaces.find((*child)->getPrefix());
        if (ns != sDevicesNamespaces.end()) {
          name = BAD_CAST (*child)->getPrefixedClass().c_str();
        }
      }
      if (name == NULL) name = BAD_CAST (*child)->getClass().c_str();
      
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, name));
      
      printProbeHelper(writer, *child);
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Component 
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Components    
  }
}

void XmlPrinter::printDataItem(xmlTextWriterPtr writer, DataItem *dataItem)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DataItem"));

  addAttributes(writer, dataItem->getAttributes());
  string source = dataItem->getSource();
  if (!source.empty())
  {
    addSimpleElement(writer, "Source", source);
  }
  
  if (dataItem->hasConstraints())
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Constraints"));

    string s = dataItem->getMaximum();
    if (!s.empty())
    {
      addSimpleElement(writer, "Maximum", s);
    }
    
    s = dataItem->getMinimum();
    if (!s.empty())
    {
      addSimpleElement(writer, "Minimum", s);
    }
    
    vector<string> values = dataItem->getConstrainedValues();
    vector<string>::iterator value;
    for (value = values.begin(); value != values.end(); value++)
    {
      addSimpleElement(writer, "Value", *value);
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Constraints   
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DataItem   
}

string XmlPrinter::printSample(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const Int64 nextSeq,
    const Int64 firstSeq,
    vector<ComponentEventPtr>& results
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
    
    initXmlDoc(writer, eSTREAMS,
               instanceId,
               bufferSize,
               nextSeq,
               firstSeq);
        
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Streams"));
    
    // Sort the vector by category.
    std::sort(results.begin(), results.end());

    Device *lastDevice = NULL;
    Component *lastComponent = NULL;
    int lastCategory = -1;
    
    vector<ComponentEventPtr>::iterator result;
    for (result = results.begin(); result != results.end(); result++)
    {
      DataItem *dataItem = (*result)->getDataItem();
      Component *component = dataItem->getComponent();
      Device *device = component->getDevice();
      
      if (device != lastDevice)
      {
        if (lastDevice != NULL)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DeviceStream
        lastDevice = device;
        if (lastComponent != NULL)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // ComponentStream
        lastComponent = NULL;
        if (lastCategory != -1)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
        lastCategory = -1;
        addDeviceStream(writer, device);
      }
      
      if (component != lastComponent)
      {
        if (lastComponent != NULL)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // ComponentStream
        lastComponent = component;
        if (lastCategory != -1)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
        lastCategory = -1;
        addComponentStream(writer, component);        
      }
      
      if (lastCategory != dataItem->getCategory())
      {
        if (lastCategory != -1)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
        lastCategory = dataItem->getCategory();
        addCategory(writer, dataItem->getCategory());
      }
      
      addEvent(writer, *result);
    }
    
    if (lastCategory != -1)
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
    if (lastDevice != NULL)
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DeviceStream
    if (lastComponent != NULL)
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // ComponentStream
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Streams    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectStreams
    THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
    
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);    
  }
  catch (string error) {
    sLogger << dlib::LERROR << "printProbe: " << error;
  }
  catch (...) {
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  
  return ret;
}

string XmlPrinter::printAssets(const unsigned int instanceId,
                               const unsigned int aBufferSize,
                               const unsigned int anAssetCount,
                               std::vector<AssetPtr> &anAssets)
{
  xmlTextWriterPtr writer;
  xmlBufferPtr buf;
  string ret;
  
  try {
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
    
    initXmlDoc(writer, eASSETS, instanceId, aBufferSize, anAssetCount);
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Assets"));

    vector<AssetPtr>::iterator iter;
    for (iter = anAssets.begin(); iter != anAssets.end(); ++iter)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST (*iter)->getContent().c_str()));
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Assets    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectAssets
    
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);
  }
  catch (string error) {
    sLogger << dlib::LERROR << "printProbe: " << error;
  }
  catch (...) {
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  
  return ret;
}


void XmlPrinter::addDeviceStream(xmlTextWriterPtr writer, Device *device)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DeviceStream"));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "name", 
                                                  BAD_CAST device->getName().c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "uuid", 
                                                  BAD_CAST device->getUuid().c_str()));
}

void XmlPrinter::addComponentStream(xmlTextWriterPtr writer, Component *component)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "ComponentStream"));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "component", 
                                                  BAD_CAST component->getClass().c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "name", 
                                                  BAD_CAST component->getName().c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "componentId", 
                                                  BAD_CAST component->getId().c_str()));
}

void XmlPrinter::addCategory(xmlTextWriterPtr writer, DataItem::ECategory category)
{
  switch (category)
  {
    case DataItem::SAMPLE:
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Samples"));
      break;
      
    case DataItem::EVENT:
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Events"));
      break;
      
    case DataItem::CONDITION:
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Condition"));
      break;
  }
  
}

void XmlPrinter::addEvent(xmlTextWriterPtr writer, ComponentEvent *result)
{
  DataItem *dataItem = result->getDataItem();
  if (dataItem->isCondition()) {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST result->getLevelString().c_str()));
  } else {
    xmlChar *element = NULL;
    if (!dataItem->getPrefix().empty()) 
    {
      map<string, SchemaNamespace>::iterator ns = sStreamsNamespaces.find(dataItem->getPrefix());
      if (ns != sStreamsNamespaces.end()) {
        element = BAD_CAST dataItem->getPrefixedElementName().c_str();
      }
    }
    if (element == NULL) element = BAD_CAST dataItem->getElementName().c_str();
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, element));
  }
  
  addAttributes(writer, result->getAttributes());

  if (result->isTimeSeries()) {
    ostringstream ostr;
    ostr.precision(6);
    const vector<float> &v = result->getTimeSeries();
    for (size_t i = 0; i < v.size(); i++)
      ostr << v[i] << ' ';
    string str = ostr.str();
    THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST str.c_str()));
  } else if (!result->getValue().empty()) {
    xmlChar *text = xmlEncodeEntitiesReentrant(NULL, BAD_CAST result->getValue().c_str());
    THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, text));
    xmlFree(text);
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Streams    
}

void XmlPrinter::addAttributes(xmlTextWriterPtr writer,
                               std::map<string, string> *attributes)
{
  std::map<string, string>::iterator attr;
  for (attr= attributes->begin(); attr!=attributes->end(); attr++)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST attr->first.c_str(), 
                                                    BAD_CAST attr->second.c_str()));

  }
}

void XmlPrinter::addAttributes(xmlTextWriterPtr writer,
                               AttributeList *attributes)
{
  AttributeList::iterator attr;
  for (attr= attributes->begin(); attr!=attributes->end(); attr++)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST attr->first, 
                                                    BAD_CAST attr->second.c_str()));
    
  }
}

/* XmlPrinter helper Methods */
void XmlPrinter::initXmlDoc(xmlTextWriterPtr writer, EDocumentType aType,
                            const unsigned int instanceId,
                            const unsigned int bufferSize,
                            const Int64 nextSeq,
                            const Int64 firstSeq
                            )
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL));
  
  // Write the root element  
  string xmlType;
  map<string, SchemaNamespace> *namespaces;
  switch (aType) {
  case eERROR:
      namespaces = &sErrorNamespaces;
      xmlType = "Error";
      break;
      
  case eSTREAMS:
      namespaces = &sStreamsNamespaces;
      xmlType = "Streams";
      break;
      
  case eDEVICES:
      namespaces = &sDevicesNamespaces;
      xmlType = "Devices";
      break;
      
    case eASSETS:
      namespaces = &sAssetsNamespaces;
      xmlType = "Assets";
      break;
      
  }
  
  
  string rootName = "MTConnect" + xmlType;
  string xmlns = "urn:mtconnect.org:" + rootName + ":1.2";  
  string location;
  
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST rootName.c_str()));

  
  // Always make the default namespace and the m: namespace MTConnect default.
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xmlns:m",
                                                  BAD_CAST xmlns.c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xmlns",
                                                  BAD_CAST xmlns.c_str()));
  
  // Alwats add the xsi namespace
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xmlns:xsi",
                                                  BAD_CAST "http://www.w3.org/2001/XMLSchema-instance"));
  
  // Add in the other namespaces if they exist
  map<string, SchemaNamespace>::iterator ns;
  for (ns = namespaces->begin(); ns != namespaces->end(); ns++)
  {
    string attr = "xmlns:" + ns->first;
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                    BAD_CAST attr.c_str(),
                                                    BAD_CAST ns->second.mUrn.c_str()));
    
    // Always take the first location. There should only be one location!
    if (location.empty() && !ns->second.mSchemaLocation.empty())
    {
      location = ns->second.mUrn + " " + ns->second.mSchemaLocation;
    }
  }
  
  // Write the schema location
  if (location.empty()) {
    location = xmlns + " http://www.mtconnect.org/schemas/" + rootName + "_1.2.xsd";
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xsi:schemaLocation",
                                                  BAD_CAST location.c_str()));
  
  
  // Create the header
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Header"));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "creationTime", BAD_CAST getCurrentTime(GMT).c_str()));
  
  string hostname;
  if (dlib::get_local_hostname(hostname) != 0)
    hostname = "localhost";
  
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "sender", BAD_CAST hostname.c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "instanceId", BAD_CAST intToString(instanceId).c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST "1.2"));
  if (aType == eASSETS) 
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "assetBufferSize", BAD_CAST intToString(bufferSize).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "assetCount", BAD_CAST int64ToString(nextSeq).c_str()));
  }
  else
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "bufferSize", BAD_CAST intToString(bufferSize).c_str()));    
  }
  if (aType == eSTREAMS)
  {
    // Add additional attribtues for streams
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "nextSequence", 
                                                    BAD_CAST int64ToString(nextSeq).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST  "firstSequence", 
                                                    BAD_CAST int64ToString(firstSeq).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "lastSequence", 
                                                    BAD_CAST int64ToString(nextSeq - 1).c_str()));
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
}


void XmlPrinter::addSimpleElement(xmlTextWriterPtr writer, string element, string &body, 
                      map<string, string> *attributes)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST element.c_str()));
  
  if (attributes != NULL && attributes->size() > 0) 
    addAttributes(writer, attributes);
  
  if (!body.empty())
  {
    xmlChar *text = xmlEncodeEntitiesReentrant(NULL, BAD_CAST body.c_str());
    THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, text));
    xmlFree(text);
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Element    
}


