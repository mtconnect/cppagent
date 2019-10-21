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

#include <sstream>
#include <fstream>
#include <string>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "globals.hpp"

#if !defined(__MACH__) || defined(CUTI_NO_INTEGRATION)
#define XCTestCase void
#endif

#define STRINGIZE(s) #s
#define TO_STRING(s) STRINGIZE(s)
#define PROJECT_ROOT_DIR TO_STRING(PROJECT_ROOT)

#define TEST_BIN_ROOT_DIR TO_STRING(TEST_BIN_ROOT) "/../Resources"

// Retrieve a sample file, open it, and return it as a string
std::string getFile(std::string fileLoc);

// Fill the error
void fillErrorText(std::string &errorXml, const std::string &text);

// Search the xml and insert a value into an attribute (attribute="")
void fillAttribute(
                   std::string &xmlString,
                   const std::string &attribute,
                   const std::string &value
                   );

// Trim white space from string
std::string &trim(std::string &str);

#ifdef __MACH__
/// Asserts that two XML strings are equivalent.
#define CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, path, expected) \
xpathTest(doc, path, expected, __FILE__, __LINE__, self)
#else
#define CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, path, expected) \
xpathTest(doc, path, expected, __FILE__, __LINE__)
#endif

void xpathTest(xmlDocPtr doc, const char *xpath, const char *expected,
               const std::string &file, int line, XCTestCase *self = nullptr);

#define PARSE_XML(expr) \
string result = expr;\
auto doc = xmlParseMemory(result.c_str(), result.length()); \
CPPUNIT_ASSERT(doc);

#ifdef __MACH__
/// Asserts that two XML strings are equivalent.
#define CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, path, expected) \
xpathTestCount(doc, path, expected, __FILE__, __LINE__, self)
#else
#define CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, path, expected) \
xpathTestCount(doc, path, expected, __FILE__, __LINE__)
#endif

void xpathTestCount(xmlDocPtr doc, const char *xpath, int expected,
                    const std::string &file, int line, XCTestCase *self = nullptr);

void failIf(bool condition, const std::string &message,
            const std::string &file, int line, XCTestCase *self = nullptr);

void failNotEqualIf(bool condition,
                    const std::string &expected,
                    const std::string &actual,
                    const std::string &message,
                    const std::string &file, int line, XCTestCase *self = nullptr);

void assertIf(bool condition, const std::string &message,
              const std::string &file, int line, XCTestCase *self = nullptr);
