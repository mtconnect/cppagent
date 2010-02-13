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

using namespace std;

/* XmlParser public methods */
XmlParser::XmlParser(const string& xmlPath)
{
  try
  {
    mParser = new xmlpp::DomParser;
    
    // Set to false now because XML does not contain DTD
    mParser->set_validate(false);
    
    // We want the text to be resolved/unescaped automatically.
    mParser->set_substitute_entities();
    
    mParser->parse_file(xmlPath);

    xmlpp::Node *root = getRootNode();
    std::string path = "//Devices/*";
        
    xmlpp::NodeSet devices;    
    if (root->get_namespace_uri().empty())
      devices = root->find(path);
    else
    {
      xmlpp::Node::PrefixNsMap spaces;
      spaces.insert(
	pair<Glib::ustring, Glib::ustring>("m", root->get_namespace_uri()));
      devices = root->find(addNamespace(path, "m"), spaces);
    }

    for (unsigned int i = 0; i < devices.size(); i++)
    {
      // MAKE SURE FIRST ITEMS IN HIERARCHIES ARE DEVICES
      mDevices.push_back(static_cast<Device *>(handleComponent(devices[i])));
    }
  }
  catch (exception & e)
  {
    logEvent("XmlParser::XmlParser", e.what());
    delete mParser;
    throw (string) e.what();
  }
}

XmlParser::~XmlParser()
{
  delete mParser;
}

xmlpp::Node * XmlParser::getRootNode() const
{
  return mParser->get_document()->get_root_node();
}

/* XmlParser protected methods */
Component * XmlParser::handleComponent(
    xmlpp::Node * component,
    Component * parent,
    Device *device
  )
{
  Component * toReturn = NULL;
  Component::EComponentSpecs spec =
    (Component::EComponentSpecs) getEnumeration(
      component->get_name(),
      Component::SComponentSpecs,
      Component::NumComponentSpecs
    );

  string name;
  switch (spec)
  {
  case Component::DEVICE:
    name = component->get_name().raw();
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
    name = component->get_name().raw();
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
  if (toReturn != NULL && !dynamic_cast<const xmlpp::ContentNode*>(component))
  {
    xmlpp::Node::NodeList children = component->get_children();
    
    xmlpp::Node::NodeList::iterator child;
    for (child = children.begin(); child != children.end(); ++child)
    {
      if ((*child)->get_name() == "Description")
      {
        const xmlpp::Element *nodeElement =
          dynamic_cast<const xmlpp::Element *>(*child);
        toReturn->addDescription(getAttributes(nodeElement));
      }
      else
      {
        handleComponent(*child, toReturn, device);
      }
    }
  }
  
  return toReturn;
}

Component * XmlParser::loadComponent(
    xmlpp::Node *node,
    Component::EComponentSpecs spec,
    string& name
  )
{
  const xmlpp::Element *nodeElement = dynamic_cast<const xmlpp::Element*>(node);
  std::map<string, string> attributes = getAttributes(nodeElement);
  
  switch (spec)
  {
  case Component::DEVICE:
    return new Device(attributes);
  default:
    return new Component(name, attributes);
  }
}

std::map<string, string> XmlParser::getAttributes(const xmlpp::Element *element)
{
  std::map<string, string> toReturn;
  
  // Load all the attributes into the map to return
  const xmlpp::Element::AttributeList& attributes =
    element->get_attributes();
    
  xmlpp::Element::AttributeList::const_iterator attr;
  for (attr = attributes.begin(); attr != attributes.end(); attr++)
  {
    toReturn[(*attr)->get_name()] = (*attr)->get_value();
  }
  
  return toReturn;
}

void XmlParser::loadDataItem(
    xmlpp::Node *dataItem,
    Component *parent,
    Device *device
  )
{
  const xmlpp::Element* nodeElement =
    dynamic_cast<const xmlpp::Element*>(dataItem);
  DataItem *d = new DataItem(getAttributes(nodeElement));
  d->setComponent(*parent);
  
  // Check children for "source"
  if (!dynamic_cast<const xmlpp::ContentNode*>(dataItem))
  {
    xmlpp::Node::NodeList children = dataItem->get_children("Source");
    
    if (children.size() == 1)
    {
      xmlpp::Element * source =
        dynamic_cast<xmlpp::Element *>(children.front());
      
      if (source && source->has_child_text())
      {
        xmlpp::TextNode * nodeText = source->get_child_text();
        d->addSource(nodeText->get_content());
      }
    }
    
    children = dataItem->get_children("Constraints");
    if (children.size() == 1)
    {
      xmlpp::Node *constraints = children.front();
      
      // Check for Minimum and Maximum or Values.
      xmlpp::Node::NodeList values = constraints->get_children("Value");
      if (values.size() > 0)
      {
        xmlpp::Node::NodeList::iterator child;
        for (child = values.begin(); child != values.end(); ++child)
        {
          xmlpp::Element *value = dynamic_cast<xmlpp::Element *>(*child);
          d->addConstrainedValue(value->get_child_text()->get_content());
        }
      }
      else
      {
        xmlpp::Node::NodeList values = constraints->get_children("Minimum");
        if (values.size() == 1) { 
          xmlpp::Element *value = dynamic_cast<xmlpp::Element *>(values.front());
          d->setMinimum(value->get_child_text()->get_content()); 
        }
        values = constraints->get_children("Maximum");
        if (values.size() == 1) { 
          xmlpp::Element *value = dynamic_cast<xmlpp::Element *>(values.front());
          d->setMaximum(value->get_child_text()->get_content()); 
        }        
      }
    }
    
  }
  
  parent->addDataItem(*d);
  device->addDeviceDataItem(*d);
}

void XmlParser::handleChildren(
    xmlpp::Node *components,
    Component *parent,
    Device *device
  )
{
  if (!dynamic_cast<const xmlpp::ContentNode*>(components))
  {
    xmlpp::Node::NodeList children = components->get_children();
    
    xmlpp::Node::NodeList::iterator child;
    for (child = children.begin(); child != children.end(); ++child)
    {
      handleComponent(*child, parent, device);
    }
  }
}

