//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
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

#include "assets/cutting_tool.hpp"
#include "device_model/component.hpp"
#include "device_model/data_item/data_item.hpp"
#include "device_model/device.hpp"
#include "device_model/relationships.hpp"
#include "observation/checkpoint.hpp"
#include "utilities.hpp"

#include <libxml/tree.h>

#include <list>
#include <set>

namespace mtconnect
{
  class XmlPrinter;

  class XmlParser
  {
  public:
    // Constructor to set the open the correct file
    XmlParser();

    virtual ~XmlParser();

    // Parses a file and returns a list of devices
    std::list<Device *> parseFile(const std::string &aPath, XmlPrinter *aPrinter);

    // Just loads the document, assumed it has already been parsed before.
    void loadDocument(const std::string &aDoc);

    // Get std::list of data items in path
    void getDataItems(observation::FilterSet &filterSet, const std::string &path,
                      xmlNodePtr node = nullptr);

  protected:
    // Main method to process the nodes and return the objects
    Component *handleNode(xmlNodePtr node, Component *parent = nullptr, Device *device = nullptr);

    // Helper to handle/return each component of the device
    Component *loadComponent(xmlNodePtr node, const std::string &name);

    // Load the data items
    void loadDataItems(xmlNodePtr dataItems, Component *component);

    // Load the data items
    // Perform loading on children and set up relationships
    void handleChildren(xmlNodePtr components, Component *parent = nullptr,
                        Device *device = nullptr);

    std::unique_ptr<Composition> handleComposition(xmlNodePtr composition);

    // Perform loading of references and set up relationships
    void handleReference(xmlNodePtr reference, Component *parent = nullptr);

  protected:
    // LibXML XML Doc
    xmlDocPtr m_doc = nullptr;
    std::map<std::string, std::function<void(xmlNodePtr, Component *, Device *)>> m_handlers;
  };
}  // namespace mtconnect
