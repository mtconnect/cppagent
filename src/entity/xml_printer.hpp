//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "entity.hpp"

extern "C"
{
  using xmlTextWriter = struct _xmlTextWriter;
  using xmlTextWriterPtr = xmlTextWriter *;
}

namespace mtconnect
{
  namespace entity
  {
    class XmlPrinter
    {
     public:
      XmlPrinter() = default;

      void print(xmlTextWriterPtr writer, const EntityPtr entity);
    };
  }  // namespace entity
}  // namespace mtconnect