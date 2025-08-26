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

// Must be first
#include <gtest/gtest.h>
// Here

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

// Retrieve a sample file, open it, and return it as a string
inline std::string getFile(std::string file)
{
  using namespace std;

  string path = string(TEST_RESOURCE_DIR "/") + file;

  ifstream ifs(path.c_str());
  stringstream stream;
  stream << ifs.rdbuf();
  return stream.str();
}

// Fill the error
inline void fillErrorText(std::string &errorXml, const std::string &text)
{
  using namespace std;

  size_t pos = errorXml.find("</Error>");

  // Error in case </Error> was not found
  if (pos == string::npos)
  {
    return;
  }

  // Trim everything between >....</Error>
  size_t gT = pos;

  while (errorXml[gT - 1] != '>')
  {
    gT--;
  }

  errorXml.erase(gT, pos - gT);

  // Insert new text into pos
  pos = errorXml.find("</Error>");
  errorXml.insert(pos, text);
}

// Search the xml and insert a value into an attribute (attribute="")
inline void fillAttribute(std::string &xmlString, const std::string &attribute,
                          const std::string &value)
{
  using namespace std;

  size_t pos = xmlString.find(attribute + "=\"\"");

  if (pos == string::npos)
  {
    return;
  }
  else
  {
    pos += attribute.length() + 2;
  }

  xmlString.insert(pos, value);
}

/// Asserts that two XML strings are equivalent.
#define ASSERT_XML_PATH_EQUAL(doc, path, expected) \
  xpathTest(doc, path, expected, __FILE__, __LINE__)

#define XML_PATH_VALUE(doc, path) xpathValue(doc, path, __FILE__, __LINE__)

#define PARSE_XML(expr)                                                         \
  string result = expr;                                                         \
  auto doc = xmlParseMemory(result.c_str(), static_cast<int>(result.length())); \
  ASSERT_TRUE(doc);

/// Asserts that two XML strings are equivalent.
#define ASSERT_XML_PATH_COUNT(doc, path, expected) \
  xpathTestCount(doc, path, expected, __FILE__, __LINE__)

#define DUMP_XML(doc) \
  dumpXml(doc)

inline void failIf(bool condition, const std::string &message, const std::string &file, int line)
{
  ASSERT_FALSE(condition) << file << "(" << line << "): Failed " << message;
}

inline void failNotEqualIf(bool condition, const std::string &expected, const std::string &actual,
                           const std::string &message, const std::string &file, int line)
{
  ASSERT_FALSE(condition) << file << "(" << line << "): Failed not equal " << message << "\n"
                          << "  Expected: " << expected << "\n"
                          << "  Actual: " << actual;
}

inline void assertIf(bool condition, const std::string &message, const std::string &file, int line)
{
  ASSERT_TRUE(condition) << file << "(" << line << "): Failed " << message;
}

using ValueResponse = std::pair<std::optional<std::string>, std::optional<std::string>>;

inline std::string dumpXml(xmlDocPtr doc)
{
  int size;
  xmlChar *memory;
  xmlDocDumpFormatMemory(doc, &memory, &size, 1);
  
  std::string s((const char *) memory, size);
  xmlFree(memory);
  
  return s;
}

inline ValueResponse xpathValue(xmlDocPtr doc, const char *xpath, const std::string &file, int line,
                                bool noValue = false)
{
  using namespace std;

  xmlNodePtr root = xmlDocGetRootElement(doc);

  string path(xpath), attribute;
  size_t pos = path.find_first_of('@');

  while (pos != string::npos && attribute.empty())
  {
    if (xpath[pos - 1] != '[')
    {
      attribute = path.substr(pos + 1);
      path = path.substr(0, pos);
    }

    pos = path.find_first_of('@', pos + 1);
  }

  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);

  bool any = false;

  for (xmlNsPtr ns = root->nsDef; ns; ns = ns->next)
  {
    if (ns->prefix)
    {
      xmlXPathRegisterNs(xpathCtx, ns->prefix, ns->href);
      any = true;
    }
  }

  if (!any)
    xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href);

  xmlXPathObjectPtr obj = xmlXPathEvalExpression(BAD_CAST path.c_str(), xpathCtx);

  if (!obj || !obj->nodesetval || obj->nodesetval->nodeNr == 0)
  {
    stringstream message;
    message << file << "(" << line << "): "
            << "Xpath " << xpath << " did not match any nodes in XML document" << endl
            << dumpXml(doc);

    if (obj)
      xmlXPathFreeObject(obj);
    xmlXPathFreeContext(xpathCtx);

    if (noValue)
      return ValueResponse(std::nullopt, std::nullopt);
    else
      return ValueResponse(std::nullopt, message.str());
  }

  // Special case when no children are expected
  xmlNodePtr first = obj->nodesetval->nodeTab[0];

  if (noValue)
  {
    bool has_content = false;
    stringstream message;

    if (attribute.empty())
    {
      message << "Xpath " << xpath << " was not supposed to have any children.";

      for (xmlNodePtr child = first->children; !has_content && child; child = child->next)
      {
        if (child->type == XML_ELEMENT_NODE)
        {
          has_content = true;
        }
        else if (child->type == XML_TEXT_NODE)
        {
          string res = (const char *)child->content;
          has_content = !mtconnect::trim(res).empty();
        }
      }
    }
    else
    {
      message << "Xpath " << xpath << " was not supposed to have an attribute.";
      xmlChar *text = xmlGetProp(first, BAD_CAST attribute.c_str());

      if (text)
      {
        message << "Value was: " << text;
        has_content = true;
        xmlFree(text);
      }
    }

    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(xpathCtx);

    if (has_content)
      return ValueResponse(std::nullopt, message.str());
    else
      return ValueResponse(std::nullopt, std::nullopt);
  }

  string actual;
  xmlChar *text = nullptr;

  switch (first->type)
  {
    case XML_ELEMENT_NODE:
      if (attribute.empty())
      {
        text = xmlNodeGetContent(first);
      }
      else if (first->properties)
      {
        text = xmlGetProp(first, BAD_CAST attribute.c_str());
        if (text == nullptr)
        {
          text = xmlStrdup(BAD_CAST "ATTRIBUTE NOT FOUND");
        }
      }

      if (text)
      {
        actual = (const char *)text;
        xmlFree(text);
      }

      break;

    case XML_ATTRIBUTE_NODE:
      actual = (const char *)first->content;
      break;

    case XML_TEXT_NODE:
      actual = (const char *)first->content;
      break;

    default:
      cerr << "Cannot handle type: " << first->type << endl;
  }

  xmlXPathFreeObject(obj);
  xmlXPathFreeContext(xpathCtx);

  actual = mtconnect::trim(actual);

  return ValueResponse(actual, std::nullopt);
}

inline void xpathTest(xmlDocPtr doc, const char *xpath, const char *expected,
                      const std::string &file, int line)
{
  using namespace std;

  auto res = xpathValue(doc, xpath, file, line, expected == nullptr);
  if (res.second)
    ADD_FAILURE_AT(file.c_str(), line) << *res.second;
  else if (expected != nullptr)
  {
    if (res.first)
    {
      string message = (string) "Incorrect value for path " + xpath;
      auto actual = *res.first;
      if (expected[0] != '!')
      {
        failNotEqualIf(actual != expected, expected, actual, message, file, line);
      }
      else
      {
        expected += 1;
        failNotEqualIf(actual == expected, expected, actual, message, file, line);
      }
    }
    else
    {
      ADD_FAILURE_AT(file.c_str(), line) << "No value for " << xpath;
    }
  }
}

inline void xpathTestCount(xmlDocPtr doc, const char *xpath, int expected, const std::string &file,
                           int line)

{
  using namespace std;

  xmlNodePtr root = xmlDocGetRootElement(doc);

  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);

  bool any = false;

  for (xmlNsPtr ns = root->nsDef; ns; ns = ns->next)
  {
    if (ns->prefix)
    {
      xmlXPathRegisterNs(xpathCtx, ns->prefix, ns->href);
      any = true;
    }
  }

  if (!any)
    xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href);

  xmlXPathObjectPtr obj = xmlXPathEvalExpression(BAD_CAST xpath, xpathCtx);

  if (!obj || !obj->nodesetval)
  {
    stringstream message;
    message << "Xpath " << xpath << " did not match any nodes in XML document";
    failIf(true, message.str(), file, line);

    if (obj)
      xmlXPathFreeObject(obj);

    xmlXPathFreeContext(xpathCtx);
    return;
  }

  string message = (string) "Incorrect count of elements for path " + xpath;

  int actual = obj->nodesetval->nodeNr;

  xmlXPathFreeObject(obj);
  xmlXPathFreeContext(xpathCtx);

  failNotEqualIf(actual != expected, boost::lexical_cast<string>(expected),
                 boost::lexical_cast<string>(actual), message, file, line);
}
