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

#include <stdexcept>

#include "mtconnect/config.hpp"

#define xml_strfy(line) #line
/// @brief macro to throw an error from XML parsing based on the result of a libxml2 function
/// returning an int
/// @param expr the expression
#define THROW_IF_XML2_ERROR(expr)                                                 \
  if ((expr) < 0)                                                                 \
  {                                                                               \
    throw XmlError("XML Error at " __FILE__ "(" xml_strfy(__LINE__) "): " #expr); \
  }
/// @brief macro to throw an error from XML parsing based on the result of a libxml2 function
/// returning a pointer
/// @param expr the expression
#define THROW_IF_XML2_NULL(expr)                                                  \
  if (!(expr))                                                                    \
  {                                                                               \
    throw XmlError("XML Error at " __FILE__ "(" xml_strfy(__LINE__) "): " #expr); \
  }

namespace mtconnect::printer {
  /// @brief Error class for catching parsing errors
  class AGENT_LIB_API XmlError : public std::logic_error
  {
  public:
    using std::logic_error::logic_error;
  };

}  // namespace mtconnect::printer
