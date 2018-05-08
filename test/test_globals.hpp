/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this std::list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this std::list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#pragma once

#include <sstream>
#include <fstream>
#include <string>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "globals.hpp"

/* Retrieve a sample file, open it, and return it as a string */
std::string getFile(std::string fileLoc);

/* Fill the error */
void fillErrorText(std::string& errorXml, const std::string& text);

/* Search the xml and insert a value into an attribute (attribute="") */
void fillAttribute(
  std::string& xmlString,
  const std::string& attribute,
  const std::string& value
);

// Trim white space from string
std::string &trim(std::string &str);


/// Asserts that two XML strings are equivalent.
#define CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, path, expected) \
 ::xpathTest(doc, path, expected, CPPUNIT_SOURCELINE() )

void xpathTest(xmlDocPtr doc, const char *xpath, const char *expected,
               CPPUNIT_NS::SourceLine sourceLine);

#define PARSE_XML(expr) \
  string result = expr;\
  xmlDocPtr doc = xmlParseMemory(result.c_str(), result.length()); \
  CPPUNIT_ASSERT(doc);

/// Asserts that two XML strings are equivalent.
#define CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, path, expected) \
  ::xpathTestCount(doc, path, expected, CPPUNIT_SOURCELINE() )

void xpathTestCount(xmlDocPtr doc, const char *xpath, int expected,
               CPPUNIT_NS::SourceLine sourceLine);

