//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/asset/cutting_tool.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/device_model/component.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::printer {
  class XmlPrinter;
}

/// @brief MTConnect Device parser namespace
namespace mtconnect::parser {
  /// @brief parse an xml document and create a list of devices
  class AGENT_LIB_API XmlParser
  {
  public:
    /// @brief Constructor to set the open the correct file
    XmlParser();

    virtual ~XmlParser();

    /// @brief Parses a file and returns a list of devices
    /// @param[in] aPath to the file
    /// @param[in] aPrinter the printer to obtain and set namespaces
    /// @returns a list of device pointers
    std::list<device_model::DevicePtr> parseFile(const std::string &aPath,
                                                 printer::XmlPrinter *aPrinter);
    /// @brief Parses a single device fragment
    /// @param[in] deviceXml device xml of a single device
    /// @param[in] aPrinter the printer to obtain and set namespaces
    /// @returns a shared device pointer if successful
    device_model::DevicePtr parseDevice(const std::string &deviceXml,
                                        printer::XmlPrinter *aPrinter);

    /// @brief Just loads the document, assumed it has already been parsed before.
    /// @param aDoc the XML document to parse
    void loadDocument(const std::string &aDoc);
    /// @brief get data items given a filter set and an xpath
    /// @param[out] filterSet a filter set to build
    /// @param[in] path the xpath
    /// @param[in] node an option node pointer to start from. defaults to the document root.
    void getDataItems(FilterSet &filterSet, const std::string &path, xmlNodePtr node = nullptr);
    /// @brief get the schema version
    /// @return the version
    const auto &getSchemaVersion() const { return m_schemaVersion; }

  protected:
    // LibXML XML Doc
    xmlDocPtr m_doc = nullptr;
    std::optional<std::string> m_schemaVersion;
    mutable std::shared_mutex m_mutex;
  };
}  // namespace mtconnect::parser
