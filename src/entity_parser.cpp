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

#include "entity_parser.hpp"

#include <dlib/logger.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

using namespace std;

#define strstrfy(x) #x
#define strfy(x) strstrfy(x)
#define THROW_IF_XML2_ERROR(expr)                                                  \
  if ((expr) < 0)                                                                  \
  {                                                                                \
    throw runtime_error("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); \
  }
#define THROW_IF_XML2_NULL(expr)                                                   \
  if (!(expr))                                                                     \
  {                                                                                \
    throw runtime_error("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); \
  }


namespace mtconnect
{
  using namespace entity;
  
  static dlib::logger g_logger("EntityParser");


  extern "C" void XMLCDECL agentXMLErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...)
  {
    va_list args;

    char buffer[2048] = {0};
    va_start(args, msg);
    vsnprintf(buffer, 2046u, msg, args);
    buffer[2047] = '0';
    va_end(args);

    g_logger << dlib::LERROR << "XML: " << buffer;
  }
  
}
