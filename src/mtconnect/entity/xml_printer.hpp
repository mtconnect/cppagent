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

#include <unordered_set>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"

extern "C"
{
  using xmlTextWriter = struct _xmlTextWriter;
  using xmlTextWriterPtr = xmlTextWriter *;
}

namespace mtconnect {
  namespace entity {
    /// @brief Convert an entity to an XML document
    class AGENT_LIB_API XmlPrinter
    {
    public:
      XmlPrinter(bool includeHidden = false) : m_includeHidden(includeHidden) {}

      /// @brief convert an entity to a XML document using `libxml2`
      /// @param writer libxml2 `xmlTextWriterPtr`
      /// @param entity the entity
      /// @param namespaces a set of namespaces to use in the document
      void print(xmlTextWriterPtr writer, const EntityPtr entity,
                 const std::unordered_set<std::string> &namespaces);

    protected:
      bool m_includeHidden {false};
    };
  }  // namespace entity
}  // namespace mtconnect
