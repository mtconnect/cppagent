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

#include "entity/xml_parser.hpp"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "logging.hpp"
#include "printer/xml_helper.hpp"

using namespace std;

namespace mtconnect::entity {
  using namespace mtconnect::printer;

  extern "C" void XMLCDECL entityXMLErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...)
  {
    va_list args;

    char buffer[2048] = {0};
    va_start(args, msg);
    vsnprintf(buffer, 2046u, msg, args);
    buffer[2047] = '0';
    va_end(args);

    LOG(error) << "XML: " << buffer;
  }

  inline entity::QName nodeQName(xmlNodePtr node)
  {
    entity::QName qname((const char *)node->name);

    if (node->ns && node->ns->prefix &&
        strncmp((const char *)node->ns->href, "urn:mtconnect.org:MTConnectDevices", 34u))
    {
      qname.setNs((const char *)node->ns->prefix);
    }

    return qname;
  }

  inline void trim(string &s)
  {
    auto beg = s.find_first_not_of(" \t\n");
    if (beg > 0)
      s.erase(0, beg);
    if (!s.empty())
    {
      auto end = s.find_last_not_of(" \t\n");
      if (end < s.size() - 1)
        s.erase(end + 1);
    }
  }

  static Value parseRawNode(xmlNodePtr node)
  {
    stringstream str;
    for (xmlNodePtr child = node->children; child; child = child->next)
    {
      xmlBufferPtr buf;
      THROW_IF_XML2_NULL(buf = xmlBufferCreate());
      auto count = xmlNodeDump(buf, child->doc, child, 0, 0);
      if (count > 0)
      {
        str << (const char *)buf->content;
      }
      xmlBufferFree(buf);
    }
    Value content {nullptr};
    if (str.tellp() > 0)
      content = str.str();
    return content;
  }

  EntityPtr XmlParser::parseXmlNode(FactoryPtr factory, xmlNodePtr node, ErrorList &errors,
                                    bool parseNamespaces)
  {
    auto qname = nodeQName(node);
    auto ef = factory->factoryFor(qname);
    if (ef)
    {
      OrderMapPtr order;
      AttributeSet attrs;
      if (ef->isAny())
      {
        order = make_shared<OrderMap>();
      }

      Properties properties;
      EntityList *l {nullptr};
      if (ef->isList())
      {
        l = &properties["LIST"].emplace<EntityList>();
      }

      for (xmlAttrPtr attr = node->properties; attr; attr = attr->next)
      {
        if (attr->type == XML_ATTRIBUTE_NODE)
        {
          entity::QName qname((const char *)attr->name);
          if (attr->ns)
            qname.setNs((const char *)attr->ns->prefix);
          properties.insert({qname, string((const char *)attr->children->content)});
          if (!islower(qname.getName()[0]))
          {
            attrs.emplace(qname);
          }
        }
      }

      if (parseNamespaces && node->nsDef)
      {
        for (auto def = node->nsDef; def; def = def->next)
        {
          string name;
          if (def->prefix)
            name = {string("xmlns:") + (const char *)def->prefix};
          else
            name = "xmlns";

          properties.insert({name, string((const char *)def->href)});
        }
      }

      if (ef->hasRaw())
      {
        auto value = parseRawNode(node);
        if (holds_alternative<string>(value))
          properties.insert({"RAW", value});
      }
      else
      {
        int orderCount = 0;
        for (xmlNodePtr child = node->children; child; child = child->next)
        {
          if (child->type == XML_ELEMENT_NODE)
          {
            auto name = nodeQName(child);
            bool simple = ef->isSimpleProperty(name);
            if (order)
            {
              order->emplace(name, orderCount++);

              if (!simple && !ef->isProperty(name))
              {
                simple = child->children != nullptr && child->properties == nullptr &&
                         child->nsDef == nullptr && child->children->next == nullptr &&
                         child->children->type == XML_TEXT_NODE;
              }
            }

            if (simple)
            {
              if (child->children != nullptr && child->children->content != nullptr)
              {
                string s((const char *)child->children->content);
                trim(s);
                if (!s.empty())
                  properties.insert({nodeQName(child), s});
              }
            }
            else
            {
              auto ent = parseXmlNode(ef, child, errors);
              if (ent)
              {
                if (l != nullptr)
                {
                  l->emplace_back(ent);
                }
                else if (ef->isPropertySet(ent->getName()))
                {
                  auto res = properties.try_emplace(ent->getName(), EntityList {});
                  get<EntityList>(res.first->second).emplace_back(ent);
                }
                else
                {
                  properties.insert({ent->getName(), ent});
                }
              }
              else
              {
                LOG(warning) << "Unexpected element: " << nodeQName(child);
                errors.emplace_back(
                    new EntityError("Invalid element '" + nodeQName(child) + "'", qname));
              }
            }
          }
          else if (child->type == XML_TEXT_NODE)
          {
            string s((const char *)child->content);
            trim(s);
            if (!s.empty())
              properties.insert({"VALUE", s});
          }
        }
      }

      try
      {
        auto entity = ef->make(qname, properties, errors);
        if (entity)
        {
          if (order)
            entity->setOrder(order);
          if (!attrs.empty())
            entity->setAttributes(attrs);
        }
        return entity;
      }
      catch (EntityError &e)
      {
        e.setEntity(qname);
        errors.emplace_back(e.dup());
      }
    }

    return nullptr;
  }

  EntityPtr XmlParser::parse(FactoryPtr factory, const string &document, const string &version,
                             ErrorList &errors, bool parseNamespaces)
  {
    NAMED_SCOPE("entity.xml_parser");
    EntityPtr entity;
    try
    {
      xmlInitParser();
      xmlXPathInit();
      xmlSetGenericErrorFunc(nullptr, entityXMLErrorFunc);

      unique_ptr<xmlDoc, function<void(xmlDocPtr)>> doc(
          xmlReadMemory(document.c_str(), int32_t(document.length()), "document.xml", nullptr,
                        XML_PARSE_NOBLANKS),
          [](xmlDocPtr d) { xmlFreeDoc(d); });
      xmlNodePtr root = xmlDocGetRootElement(doc.get());
      if (root != nullptr)
        entity = parseXmlNode(factory, root, errors, parseNamespaces);
      else
        errors.emplace_back(new EntityError("Cannot parse asset"));
    }

    catch (EntityError e)
    {
      LOG(error) << "Cannot parse XML document: " << e.what();
      errors.emplace_back(e.dup());
      entity.reset();
    }

    catch (XmlError e)
    {
      LOG(error) << "Cannot parse XML document: " << e.what();
      errors.emplace_back(new EntityError(e.what()));
      entity.reset();
    }

    catch (...)
    {
      LOG(error) << "Cannot parse XML document: unknown error";
      errors.emplace_back(new EntityError("Unknown Error"));
      entity.reset();
    }

    return entity;
  }
}  // namespace mtconnect::entity
