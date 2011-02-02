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

#include "xml_parser.hpp"
#include "xml_printer.hpp"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include "dlib/logger.h"
#include <stdexcept>

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
XmlParser::XmlParser(const string& xmlPath)
{
  xmlXPathContextPtr xpathCtx = NULL;
  xmlXPathObjectPtr devices = NULL;
  mDoc = NULL;
  
  try
  {
    xmlInitParser();
    xmlXPathInit();
    xmlSetGenericErrorFunc(NULL, agentXMLErrorFunc);
    
    THROW_IF_XML2_NULL(mDoc = xmlReadFile(xmlPath.c_str(), NULL, 
                       XML_PARSE_NOBLANKS));
    
    std::string path = "//Devices/*";
    THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(mDoc));
    
    xmlNodePtr root = xmlDocGetRootElement(mDoc);
    if (root->ns != NULL)
    {
      path = addNamespace(path, "m");
      THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
    }

    // Add additional namespaces to the printer if they are referenced
    // here.
    string locationUrn;
    const char *location = (const char*) xmlGetProp(root, BAD_CAST "schemaLocation");
    if (location != NULL && strncmp(location, "urn:mtconnect.org:MTConnectDevices", 34) != 0) 
    {
      string loc = location;
      size_t pos = loc.find(' ');
      if (pos != string::npos)
      {
        locationUrn = loc.substr(0, pos);
        string uri = loc.substr(pos + 1);
        
        // Try to find the prefix for this urn...
        xmlNsPtr ns = xmlSearchNsByHref(mDoc, root, BAD_CAST locationUrn.c_str());
        string prefix;
        if (ns->prefix != NULL)
          prefix = (const char*) ns->prefix;
        
        XmlPrinter::addDevicesNamespace(locationUrn, uri, prefix);
      }
    }
    
    // Add the rest of the namespaces...
    if (root->nsDef) {
      xmlNsPtr ns = root->nsDef;
      while (ns != NULL)
      {
        // Skip the standard namespaces for MTConnect and the w3c. Make sure we don't re-add the 
        // schema location again.
        if (strncmp((const char*) ns->href, "urn:mtconnect.org:MTConnectDevices", 34) != 0 &&
            strncmp((const char*) ns->href, "http://www.w3.org/", 18) != 0 &&
            locationUrn != (const char*) ns->href &&
            ns->prefix != NULL) {
          string urn = (const char*) ns->href;
          string prefix = (const char*) ns->prefix;
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
      mDevices.push_back(static_cast<Device *>(handleComponent(nodeset->nodeTab[i])));
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
}

XmlParser::~XmlParser()
{
  if (mDoc != NULL)    
    xmlFreeDoc(mDoc);
}

void XmlParser::getDataItems(set<string> &aFilterSet,
                             const string& aPath, xmlNodePtr node)
{
  xmlNodePtr root = xmlDocGetRootElement(mDoc);
  if (node == NULL) node = root;
  
  xmlXPathContextPtr xpathCtx = NULL;
  xmlXPathObjectPtr objs = NULL;

  try 
  {
    string path;
    THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(mDoc));
    xpathCtx->node = node;
    bool mtc = false;
      
    if (root->ns)
    {
      for (xmlNsPtr ns = root->nsDef; ns != NULL; ns = ns->next)
      {
        if (ns->prefix != NULL)
        {
          if (strncmp((const char *) ns->href, "urn:mtconnect.org:MTConnectDevices", 34) != 0) {
            THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, ns->prefix, ns->href));
          } else {
            mtc = true;
            THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
          }
        }
      }
      
      if (!mtc)
        THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
      
      path = addNamespace(aPath, "m");
    } else {
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
        else // Find all the data items below this node
        {
          getDataItems(aFilterSet, "*//DataItem", n);
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
Component * XmlParser::handleComponent(
    xmlNodePtr component,
    Component * parent,
    Device *device
  )
{
  Component * toReturn = NULL;
  Component::EComponentSpecs spec =
    (Component::EComponentSpecs) getEnumeration(
      (const char*) component->name,
      Component::SComponentSpecs,
      Component::NumComponentSpecs
    );

  string name;
  switch (spec)
  {
  case Component::DEVICE:
    name = (const char *) component->name;
    toReturn = device = (Device*) loadComponent(component, spec, name);
    break;
    
  case Component::COMPONENTS:
  case Component::DATA_ITEMS:
    handleChildren(component, parent, device);
    break;
    
  case Component::DATA_ITEM:
    loadDataItem(component, parent, device);
    break;
    
  case Component::TEXT:
    break;
    
  default:
    // Assume component
    name = (const char*) component->name;
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
          toReturn->addDescription((string) (const char *) text, getAttributes(child));
          xmlFree(text);
        }
        
      }
      else
      {
        handleComponent(child, toReturn, device);
      }
    }
  }
  
  return toReturn;
}

Component * XmlParser::loadComponent(
    xmlNodePtr node,
    Component::EComponentSpecs spec,
    string& name
  )
{
  std::map<string, string> attributes = getAttributes(node);
  
  switch (spec)
  {
    case Component::DEVICE:
      return new Device(attributes);
    default:
      string prefix;
      if (node->ns->prefix != 0 && 
          strncmp((const char*) node->ns->href, "urn:mtconnect.org:MTConnectDevices", 
                  34) != 0) {
        prefix = (const char*) node->ns->prefix;
      }
      
      return new Component(name, attributes, prefix);
  }
}

std::map<string, string> XmlParser::getAttributes(const xmlNodePtr node)
{
  std::map<string, string> toReturn;
  
  for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next)
  {
    if (attr->type == XML_ATTRIBUTE_NODE) {
      toReturn[(const char *) attr->name] = (const char*) attr->children->content;
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
          xmlFree(text);
        }
      }
    }   
  }
  
  parent->addDataItem(*d);
  device->addDeviceDataItem(*d);
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

