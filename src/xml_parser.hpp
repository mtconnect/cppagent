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
    std::list<device_model::DevicePtr> parseFile(const std::string &aPath, XmlPrinter *aPrinter);

    // Just loads the document, assumed it has already been parsed before.
    void loadDocument(const std::string &aDoc);

    // Get std::list of data items in path
    void getDataItems(observation::FilterSet &filterSet, const std::string &path,
                      xmlNodePtr node = nullptr);

  protected:
#if 0
    // Main method to process the nodes and return the objects
    device_model::ComponentPtr handleNode(xmlNodePtr node, device_model::ComponentPtr parent = nullptr, DevicePtr device = nullptr);

    // Helper to handle/return each component of the device
    device_model::ComponentPtr loadComponent(xmlNodePtr node, const std::string &name);

    // Load the data items
    void loadDataItems(xmlNodePtr dataItems, device_model::ComponentPtr component);

    // Load the data items
    // Perform loading on children and set up relationships
    void handleChildren(xmlNodePtr components, device_model::ComponentPtr parent = {},
                        device_model::DevicePtr device = {});

    // Perform loading of references and set up relationships
    void handleReference(xmlNodePtr reference, device_model::ComponentPtr parent = {});
#endif
  protected:
    // LibXML XML Doc
    xmlDocPtr m_doc = nullptr;
//    std::map<std::string, std::function<void(xmlNodePtr, Component *, DevicePtr )>> m_handlers;
  };
}  // namespace mtconnect
