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

#include <map>
#include <utility>
#include <vector>

#include "factory.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/utilities.hpp"
#include "requirement.hpp"

struct _xmlNode;
namespace mtconnect {
  namespace entity {
    /// @brief  Parse an XML document to an entity
    class AGENT_LIB_API XmlParser
    {
    public:
      XmlParser() = default;
      ~XmlParser() = default;
      using xmlNodePtr = _xmlNode *;

      /// @brief Parse an xmlNodePointer (libxml2) to an entity
      /// @param factory The factory to use to create the top level entity
      /// @param node an libxml2 node
      /// @param errors errors that occurred during the parsing
      /// @param parseNamespaces `true` if namespaces should be parsed
      /// @return a shared pointer to an entity if successful
      static EntityPtr parseXmlNode(FactoryPtr factory, xmlNodePtr node, ErrorList &errors,
                                    bool parseNamespaces = true);
      /// @brief Parse a string document to an entity
      /// @param factory The factory to use to create the top level entity
      /// @param document the document as a string
      /// @param errors errors that occurred during the parsing
      /// @param parseNamespaces `true` if namespaces should be parsed
      /// @return a shared pointer to an entity if successful
      static EntityPtr parse(FactoryPtr factory, const std::string &document, ErrorList &errors,
                             bool parseNamespaces = true);
    };
  }  // namespace entity
}  // namespace mtconnect
