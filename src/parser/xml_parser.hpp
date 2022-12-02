//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <list>
#include <set>
#include <shared_mutex>

#include <libxml/tree.h>

#include "asset/cutting_tool.hpp"
#include "buffer/checkpoint.hpp"
#include "device_model/component.hpp"
#include "device_model/data_item/data_item.hpp"
#include "device_model/device.hpp"
#include "utilities.hpp"

namespace mtconnect::printer {
  class XmlPrinter;
}
namespace mtconnect::parser {

  class XmlParser
  {
  public:
    // Constructor to set the open the correct file
    XmlParser();

    virtual ~XmlParser();

    // Parses a file and returns a list of devices
    std::list<device_model::DevicePtr> parseFile(const std::string &aPath,
                                                 printer::XmlPrinter *aPrinter);

    // Just loads the document, assumed it has already been parsed before.
    void loadDocument(const std::string &aDoc);

    // Get std::list of data items in path
    void getDataItems(FilterSet &filterSet, const std::string &path, xmlNodePtr node = nullptr);
    const auto &getSchemaVersion() const { return m_schemaVersion; }

  protected:
    // LibXML XML Doc
    xmlDocPtr m_doc = nullptr;
    std::optional<std::string> m_schemaVersion;
    mutable std::shared_mutex m_mutex;
  };
}  // namespace mtconnect::parser
