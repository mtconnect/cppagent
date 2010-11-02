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
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include "dlib/logger.h"

using namespace std;

static dlib::logger sLogger("xml.parser");

#define strfy(line) #line
#define THROW_IF_XML2_ERROR(expr) \
if ((expr) < 0) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }
#define THROW_IF_XML2_NULL(expr) \
if ((expr) == NULL) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }

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
    THROW_IF_XML2_NULL(mDoc = xmlReadFile(xmlPath.c_str(), NULL, 
                       XML_PARSE_NOENT | XML_PARSE_NOBLANKS));
    
    std::string path = "//Devices/*";
    THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(mDoc));
    
    xmlNodePtr root = xmlDocGetRootElement(mDoc);
    xpathCtx->node = root;
    if (root->nsDef != NULL)
    {
      path = addNamespace(path, "m");
      THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->nsDef->href));
    }
    
    devices = xmlXPathEval(BAD_CAST path.c_str(), xpathCtx);
    if (devices == NULL)
    {
      throw (string) xpathCtx->lastError.message;
    }
    
    xmlNodeSetPtr nodeset = devices->nodesetval;
    if (nodeset->nodeNr == 0)
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
    
    if (root->nsDef != NULL)
    {
      path = addNamespace(aPath, "m");
      THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->nsDef->href));
    }
    else
    {
      path = aPath;
    }
  
    objs = xmlXPathEval(BAD_CAST path.c_str(), xpathCtx);
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
    return new Component(name, attributes);
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

