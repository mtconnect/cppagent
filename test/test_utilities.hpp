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

#include <fstream>
#include <sstream>
#include <string>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

// Retrieve a sample file, open it, and return it as a string
std::string getFile(std::string fileLoc);

// Fill the error
void fillErrorText(std::string &errorXml, const std::string &text);

// Search the xml and insert a value into an attribute (attribute="")
void fillAttribute(std::string &xmlString, const std::string &attribute, const std::string &value);

// Trim white space from string
std::string &trim(std::string &str);

/// Asserts that two XML strings are equivalent.
#define ASSERT_XML_PATH_EQUAL(doc, path, expected) \
  xpathTest(doc, path, expected, __FILE__, __LINE__)

void xpathTest(xmlDocPtr doc, const char *xpath, const char *expected, const std::string &file,
               int line);

#define PARSE_XML(expr)                                                         \
  string result = expr;                                                         \
  auto doc = xmlParseMemory(result.c_str(), static_cast<int>(result.length())); \
  ASSERT_TRUE(doc);

/// Asserts that two XML strings are equivalent.
#define ASSERT_XML_PATH_COUNT(doc, path, expected) \
  xpathTestCount(doc, path, expected, __FILE__, __LINE__)

void xpathTestCount(xmlDocPtr doc, const char *xpath, int expected, const std::string &file,
                    int line);

void failIf(bool condition, const std::string &message, const std::string &file, int line);

void failNotEqualIf(bool condition, const std::string &expected, const std::string &actual,
                    const std::string &message, const std::string &file, int line);

void assertIf(bool condition, const std::string &message, const std::string &file, int line);
