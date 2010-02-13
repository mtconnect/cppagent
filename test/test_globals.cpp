/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
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

#include "test_globals.hpp"
#include <libxml/tree.h>

using namespace std;

string getFile(string fileLoc)
{
  ifstream ifs(fileLoc.c_str());
  stringstream stream;
  stream << ifs.rdbuf();
  return stream.str();
}

void fillErrorText(string& errorXml, const string& text)
{
  size_t pos = errorXml.find("</Error>");
  
  // Error in case </Error> was not found
  if (pos == string::npos)
  {
    return;
  }
  
  // Trim everything between >....</Error>
  size_t gT = pos;
  while (errorXml[gT-1] != '>')
  {
    gT--;
  }
  errorXml.erase(gT, pos-gT);
  
  // Insert new text into pos
  pos = errorXml.find("</Error>");
  errorXml.insert(pos, text);
}

void fillAttribute(
    string& xmlString,
    const string& attribute,
    const string& value
  )
{
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

void xpathTest(const xmlpp::Node* node, const char *xpath, const char *expected,
               CPPUNIT_NS::SourceLine sourceLine)
{
  string path(xpath), attribute;
  size_t pos = path.find_first_of('@');
  while (pos != string::npos && attribute.empty()) {
    if (xpath[pos - 1] != '[') {
      attribute = path.substr(pos + 1);
      path = path.substr(0, pos);
    }
    pos = path.find_first_of('@', pos + 1);
  }
  
  xmlpp::Node::PrefixNsMap spaces;
  spaces.insert(pair<Glib::ustring, Glib::ustring>("m", node->get_namespace_uri()));
  xmlpp::NodeSet set = node->find(path, spaces);
  
  if (set.size() < 1)
  {
    CPPUNIT_NS::OStringStream message;
    message << "Xpath " << xpath << " did not match any nodes in XML document";
    CPPUNIT_NS::Asserter::fail(message.str(), sourceLine);
    return;
  }
  
  // Special case when no children are expected
  xmlpp::Node *first = set.front();
  if (expected == 0)
  {
    CPPUNIT_NS::OStringStream message;
    message << "Xpath " << xpath << " was not supposed to have any children.";
    xmlpp::Node::NodeList children = first->get_children();
    bool has_content = false;
    xmlpp::Node::NodeList::iterator child;
    for (child = children.begin(); !has_content && child != children.end(); child++)
    {
      if ((*child)->cobj()->type == XML_ELEMENT_NODE)
      {
        has_content = true;
      } 
      else if (first->cobj()->type == XML_TEXT_NODE)
      {
        string res = ((xmlpp::TextNode*) *child)->get_content();
        has_content = !trim(res).empty();
      }
    }
    
    CPPUNIT_NS::Asserter::failIf(has_content, message.str(), sourceLine);    
    return;
  }
  
  string actual;
  xmlpp::Element *ele;
  switch(first->cobj()->type)
  {
    case XML_ELEMENT_NODE:
      ele = (xmlpp::Element*) first;
      if (!attribute.empty())
      {
        actual = ele->get_attribute(attribute)->get_value();
      } else {
        actual = ele->get_child_text()->get_content();
      }
      break;
      
    case XML_ATTRIBUTE_NODE:
      actual = ((xmlpp::Attribute*) first)->get_value();
      break;
      
      
    case XML_TEXT_NODE:
      actual = ((xmlpp::TextNode*) first)->get_content();
      break;
      
    default:
      cerr << "Cannot handle type: " << first->cobj()->type << endl;
  }
  
  trim(actual);
  
  string message = (string) "Incorrect value for path " + xpath;  
  CPPUNIT_NS::Asserter::failNotEqualIf(actual != expected,
                                       expected,
                                       actual,
                                       sourceLine,
                                       message);
}

string &trim(string &str)
{
  char const* delims = " \t\r\n";
  
  // trim leading whitespace
  string::size_type not_white = str.find_first_not_of(delims);
  if (not_white == string::npos)
    str.erase();
  else if (not_white > 0)
    str.erase(0, not_white);
  
  if (not_white != string::npos)
  {  
    // trim trailing whitespace
    not_white = str.find_last_not_of(delims); 
    if (not_white < (str.length() - 1))
      str.erase(not_white + 1);
  }
  
  return str;
}
