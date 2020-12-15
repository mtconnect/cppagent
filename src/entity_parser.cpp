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
  namespace entity
  {
    static dlib::logger g_logger("EntityParser");
    
    
    extern "C" void XMLCDECL entityXMLErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...)
    {
      va_list args;
      
      char buffer[2048] = {0};
      va_start(args, msg);
      vsnprintf(buffer, 2046u, msg, args);
      buffer[2047] = '0';
      va_end(args);
      
      g_logger << dlib::LERROR << "XML: " << buffer;
    }
    
    EntityPtr parseXmlEntity(FactoryPtr factory, xmlNodePtr node, ErrorList &errors)
    {
      
    }
    
    
    static Value parseXmlNode(FactoryPtr factory, xmlNodePtr node,
                            ErrorList &errors)
    {
      string qname((const char*) node->name);
      
      if (node->ns && node->ns->prefix &&
          strncmp((const char *) node->ns->href, "urn:mtconnect.org:MTConnectDevices", 34u))
      {
        qname.insert(0, (const char* ) node->ns->prefix);
      }

      auto ef = factory->factoryFor(qname);
      if (ef)
      {
        Properties properties;
        EntityList list;
        
        for (xmlAttrPtr attr = node->properties; attr; attr = attr->next)
        {
          if (attr->type == XML_ATTRIBUTE_NODE)
          {
            properties.insert({ (const char *) attr->name,
              string((const char *) attr->children->content) });
          }
        }
              
        for (xmlNodePtr child = node->children; child; child = child->next)
        {
          if (child->type == XML_ELEMENT_NODE)
          {
            auto ent = parseXmlNode(ef, child, errors);
            if (ef->isList())
            {
              //list.emplace_back(ent);
            }
            else
            {
              auto e = get<EntityPtr>(ent);
              properties.insert({ e->getName(), ent });
            }
          }
          else if (child->type == XML_TEXT_NODE)
          {
            auto content = (const char *) child->content;
            if (*content != 0)
              properties.insert({ "value", content });
          }
        }
        
        if (ef->isList())
        {
          return list;
        }
        else
        {
          auto entity = ef->make(qname, properties, errors);
          return entity;
        }
      }
      else
      {
        g_logger << dlib::LWARN << "Unexpected element: " << qname;
      }
      
      return nullptr;
    }
    
    
    EntityPtr XmlParser::parse(FactoryPtr factory,
                                const string &document,
                                const string &version,
                                ErrorList &errors)
    {
      EntityPtr entity;
      try
      {
        xmlInitParser();
        xmlXPathInit();
        xmlSetGenericErrorFunc(nullptr, entityXMLErrorFunc);
        
        unique_ptr<xmlDoc,function<void(xmlDocPtr)>> doc(xmlReadMemory(document.c_str(),
                                                               document.length(),
                                                               "document.xml", nullptr,
                                                               XML_PARSE_NOBLANKS),
                                                        [](xmlDocPtr d){ xmlFreeDoc(d); });
        xmlNodePtr root = xmlDocGetRootElement(doc.get());
        //entity = parseXmlNode(factory, root, errors);
      }
      
      catch (PropertyError e)
      {
        
      }
      
      catch (string e)
      {
        g_logger << dlib::LFATAL << "Cannot parse XML document: " << e;
        throw;
      }
      
      return entity;
    }
    
    EntityPtr XmlParser::parseFile(FactoryPtr factory,
                                    const string &path,
                                    ErrorList &errors)
    {
      
    }
  }
}
