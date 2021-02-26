//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "xml_parser.hpp"

#include "assets/cutting_tool.hpp"
#include "device_model/composition.hpp"
#include "entity/xml_parser.hpp"
#include "xml_printer.hpp"

#include <dlib/logger.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <stdexcept>

#if _MSC_VER >= 1900
#define gets gets_s
#endif

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
  using namespace observation;
  using namespace device_model;

  static dlib::logger g_logger("xml.parser");

  static inline auto handleConfiguration(xmlNodePtr node)
  {
    using namespace configuration;
    entity::ErrorList errors;

    auto configuration_entity =
        entity::XmlParser::parseXmlNode(Configuration::getRoot(), node, errors);

    unique_ptr<Configuration> configuration = make_unique<Configuration>();
    configuration->setEntity(configuration_entity);
    return configuration;
  }

  static inline auto handleComposition(xmlNodePtr node)
  {
    entity::ErrorList errors;

    auto compositions_entity =
        entity::XmlParser::parseXmlNode(Composition::getRoot(), node, errors);

    unique_ptr<Composition> compositions = make_unique<Composition>();
    compositions->setEntity(compositions_entity);
    return compositions;
  }

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

  static inline std::string getCDATA(xmlNodePtr node)
  {
    auto text = xmlNodeGetContent(node);
    string res;
    if (text)
    {
      res = (const char *)text;
      xmlFree(text);
    }
    return res;
  }

  static inline std::string getAttribute(xmlNodePtr node, const char *name)
  {
    auto value = xmlGetProp(node, BAD_CAST name);
    string res;
    if (value)
    {
      res = (const char *)value;
      xmlFree(value);
    }
    return res;
  }

  // Put all of the attributes of an element into a map
  static inline std::map<string, string> getAttributes(const xmlNodePtr node)
  {
    std::map<string, string> toReturn;

    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next)
    {
      if (attr->type == XML_ATTRIBUTE_NODE)
        toReturn[(const char *)attr->name] = (const char *)attr->children->content;
    }

    return toReturn;
  }

  XmlParser::XmlParser()
    : m_handlers(
          {{"Components",
            [this](xmlNodePtr n, Component *p, Device *d) { handleChildren(n, p, d); }},
           {"References",
            [this](xmlNodePtr n, Component *p, Device *d) { handleChildren(n, p, d); }},
           {"Reference", [this](xmlNodePtr n, Component *p, Device *d) { handleReference(n, p); }},
          {"DataItems", [this](xmlNodePtr n, Component *p, Device *d) { loadDataItems(n, p); }},
           {"DataItemRef",
            [this](xmlNodePtr n, Component *p, Device *d) { handleReference(n, p); }},
           {"ComponentRef",
            [this](xmlNodePtr n, Component *p, Device *d) { handleReference(n, p); }},
           {"Description", [](xmlNodePtr n, Component *p,
                              Device *d) { p->addDescription(getCDATA(n), getAttributes(n)); }},
           {"Compositions", [](xmlNodePtr n, Component *p, Device *d) {
             auto c = handleComposition(n);
             p->setCompositions(c); }},
           {"Configuration",
            [](xmlNodePtr n, Component *p, Device *d) {
              auto c = handleConfiguration(n);
              p->setConfiguration(c);              
            }}})
  {
  }

  inline static bool isMTConnectUrn(const char *aUrn)
  {
    return !strncmp(aUrn, "urn:mtconnect.org:MTConnect", 27u);
  }

  std::list<Device *> XmlParser::parseFile(const std::string &filePath, XmlPrinter *aPrinter)
  {
    if (m_doc)
    {
      xmlFreeDoc(m_doc);
      m_doc = nullptr;
    }

    xmlXPathContextPtr xpathCtx = nullptr;
    xmlXPathObjectPtr devices = nullptr;
    std::list<Device *> deviceList;

    try
    {
      xmlInitParser();
      xmlXPathInit();
      xmlSetGenericErrorFunc(nullptr, agentXMLErrorFunc);

      THROW_IF_XML2_NULL(m_doc = xmlReadFile(filePath.c_str(), nullptr, XML_PARSE_NOBLANKS));

      std::string path = "//Devices/*";
      THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(m_doc));

      auto root = xmlDocGetRootElement(m_doc);

      if (root->ns)
      {
        path = addNamespace(path, "m");
        THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));

        // Get schema version from Devices.xml
        if (aPrinter->getSchemaVersion().empty())
        {
          string ns((const char *)root->ns->href);

          if (!ns.find_first_of("urn:mtconnect.org:MTConnectDevices"))
          {
            auto last = ns.find_last_of(':');

            if (last != string::npos)
            {
              string version = ns.substr(last + 1);
              aPrinter->setSchemaVersion(version);
            }
          }
        }
      }

      // Add additional namespaces to the printer if they are referenced
      // here.
      string locationUrn;
      auto location = getAttribute(root, "schemaLocation");

      if (location.substr(0, 34) != "urn:mtconnect.org:MTConnectDevices")
      {
        auto pos = location.find(' ');

        if (pos != string::npos)
        {
          locationUrn = location.substr(0, pos);
          auto uri = location.substr(pos + 1);

          // Try to find the prefix for this urn...
          auto ns = xmlSearchNsByHref(m_doc, root, BAD_CAST locationUrn.c_str());
          string prefix;

          if (ns && ns->prefix)
            prefix = (const char *)ns->prefix;

          aPrinter->addDevicesNamespace(locationUrn, uri, prefix);
        }
      }

      // Add the rest of the namespaces...
      if (root->nsDef)
      {
        auto ns = root->nsDef;

        while (ns)
        {
          // Skip the standard namespaces for MTConnect and the w3c. Make sure we don't re-add the
          // schema location again.
          if (!isMTConnectUrn((const char *)ns->href) &&
              strncmp((const char *)ns->href, "http://www.w3.org/", 18u) != 0 &&
              locationUrn != (const char *)ns->href && ns->prefix)
          {
            string urn = (const char *)ns->href;
            string prefix = (const char *)ns->prefix;
            aPrinter->addDevicesNamespace(urn, "", prefix);
          }

          ns = ns->next;
        }
      }

      devices = xmlXPathEval(BAD_CAST path.c_str(), xpathCtx);

      if (!devices)
        throw(string) xpathCtx->lastError.message;

      xmlNodeSetPtr nodeset = devices->nodesetval;

      if (!nodeset || !nodeset->nodeNr)
        throw(string) "Could not find Device in XML configuration";

      // Collect the Devices...
      for (int i = 0; i != nodeset->nodeNr; ++i)
        deviceList.emplace_back(static_cast<Device *>(handleNode(nodeset->nodeTab[i])));

      xmlXPathFreeObject(devices);
      xmlXPathFreeContext(xpathCtx);
    }
    catch (string e)
    {
      if (devices)
        xmlXPathFreeObject(devices);

      if (xpathCtx)
        xmlXPathFreeContext(xpathCtx);

      g_logger << dlib::LFATAL << "Cannot parse XML file: " << e;
      throw;
    }
    catch (...)
    {
      if (devices)
        xmlXPathFreeObject(devices);

      if (xpathCtx)
        xmlXPathFreeContext(xpathCtx);

      throw;
    }

    return deviceList;
  }

  XmlParser::~XmlParser()
  {
    if (m_doc)
    {
      xmlFreeDoc(m_doc);
      m_doc = nullptr;
    }
  }

  void XmlParser::loadDocument(const std::string &doc)
  {
    if (m_doc)
    {
      xmlFreeDoc(m_doc);
      m_doc = nullptr;
    }

    try
    {
      xmlInitParser();
      xmlXPathInit();
      xmlSetGenericErrorFunc(nullptr, agentXMLErrorFunc);

      THROW_IF_XML2_NULL(m_doc = xmlReadMemory(doc.c_str(), doc.length(), "Devices.xml", nullptr,
                                               XML_PARSE_NOBLANKS));
    }

    catch (string e)
    {
      g_logger << dlib::LFATAL << "Cannot parse XML document: " << e;
      throw;
    }
  }

  void XmlParser::getDataItems(FilterSet &filterSet, const string &inputPath, xmlNodePtr node)
  {
    xmlNodePtr root = xmlDocGetRootElement(m_doc);

    if (!node)
      node = root;

    xmlXPathContextPtr xpathCtx = nullptr;
    xmlXPathObjectPtr objs = nullptr;

    try
    {
      string path;
      THROW_IF_XML2_NULL(xpathCtx = xmlXPathNewContext(m_doc));
      xpathCtx->node = node;
      bool mtc = false;

      if (root->ns)
      {
        for (xmlNsPtr ns = root->nsDef; ns; ns = ns->next)
        {
          if (ns->prefix)
          {
            if (strncmp((const char *)ns->href, "urn:mtconnect.org:MTConnectDevices", 34u) != 0)
            {
              THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, ns->prefix, ns->href));
            }
            else
            {
              mtc = true;
              THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
            }
          }
        }

        if (!mtc)
        {
          THROW_IF_XML2_ERROR(xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href));
        }

        path = addNamespace(inputPath, "m");
      }
      else
        path = inputPath;

      objs = xmlXPathEvalExpression(BAD_CAST path.c_str(), xpathCtx);

      if (!objs)
      {
        xmlXPathFreeContext(xpathCtx);
        return;
      }

      xmlNodeSetPtr nodeset = objs->nodesetval;

      if (nodeset)
      {
        for (int i = 0; i != nodeset->nodeNr; ++i)
        {
          xmlNodePtr n = nodeset->nodeTab[i];

          if (!xmlStrcmp(n->name, BAD_CAST "DataItem"))
          {
            filterSet.insert(getAttribute(n, "id"));
          }
          else if (!xmlStrcmp(n->name, BAD_CAST "DataItems"))
          {
            // Handle case where we are specifying the data items node...
            getDataItems(filterSet, "DataItem", n);
          }
          else if (!xmlStrcmp(n->name, BAD_CAST "Reference"))
          {
            auto id = getAttribute(n, "dataItemId");
            if (!id.empty())
              filterSet.insert(id);
          }
          else if (!xmlStrcmp(n->name, BAD_CAST "DataItemRef"))
          {
            auto id = getAttribute(n, "idRef");
            if (!id.empty())
              filterSet.insert(id);
          }
          else if (!xmlStrcmp(n->name, BAD_CAST "ComponentRef"))
          {
            auto id = getAttribute(n, "idRef");
            getDataItems(filterSet, "//*[@id='" + id + "']");
          }
          else  // Find all the data items and references below this node
          {
            getDataItems(filterSet, "*//DataItem", n);
            getDataItems(filterSet, "*//Reference", n);
            getDataItems(filterSet, "*//DataItemRef", n);
            getDataItems(filterSet, "*//ComponentRef", n);
          }
        }
      }

      xmlXPathFreeObject(objs);
      xmlXPathFreeContext(xpathCtx);
    }
    catch (...)
    {
      if (objs)
        xmlXPathFreeObject(objs);

      if (xpathCtx)
        xmlXPathFreeContext(xpathCtx);

      g_logger << dlib::LWARN << "getDataItems: Could not parse path: " << inputPath;
    }
  }

  Component *XmlParser::handleNode(xmlNodePtr node, Component *parent, Device *device)
  {
    string name((const char *)node->name);
    auto lambda = m_handlers.find(name);
    if (lambda != m_handlers.end())
    {
      // Parts of components
      (lambda->second)(node, parent, device);
      return nullptr;
    }
    else
    {
      // Node is a component
      Component *component = loadComponent(node, name);

      // Top level, then must be a device
      if (device == nullptr)
        device = dynamic_cast<Device *>(component);

      // Construct relationships
      if (component)
      {
        if (parent)
          parent->addChild(component);

        // Recurse for children
        if (node->children)
        {
          for (xmlNodePtr child = node->children; child; child = child->next)
          {
            if (child->type != XML_ELEMENT_NODE)
              continue;

            handleNode(child, component, device);
          }
        }
      }

      return component;
    }
  }

  Component *XmlParser::loadComponent(xmlNodePtr node, const string &name)
  {
    auto attributes = getAttributes(node);
    if (name == "Device")
    {
      return new Device(attributes);
    }
    else
    {
      string prefix;

      if (node->ns && node->ns->prefix &&
          strncmp((const char *)node->ns->href, "urn:mtconnect.org:MTConnectDevices", 34u) != 0)
      {
        prefix = (const char *)node->ns->prefix;
      }

      return new Component(name, attributes, prefix);
    }
  }

  void XmlParser::loadDataItems(xmlNodePtr dataItems, Component *parent)
  {
    using namespace entity;
    using namespace device_model::data_item;
    ErrorList errors;

    auto items = entity::XmlParser::parseXmlNode(DataItem::getRoot(), dataItems, errors);

    if (!errors.empty())
    {
      for (auto &e : errors)
        g_logger << dlib::LWARN << e->what();
    }

    auto list = items->get<EntityList>("LIST");
    for (auto &e : list)
    {
      auto di = dynamic_pointer_cast<DataItem>(e);
      if (di)
      {
        parent->addDataItem(di);
      }
    }
  }

  void XmlParser::handleChildren(xmlNodePtr components, Component *parent, Device *device)
  {
    for (xmlNodePtr child = components->children; child; child = child->next)
    {
      if (child->type != XML_ELEMENT_NODE)
        continue;

      handleNode(child, parent, device);
    }
  }

  void XmlParser::handleReference(xmlNodePtr reference, Component *parent)
  {
    auto attrs = getAttributes(reference);
    string name;

    if (attrs.count("name") > 0)
      name = attrs["name"];

    if (xmlStrcmp(reference->name, BAD_CAST "Reference") == 0)
    {
      Component::Reference ref(attrs["dataItemId"], name, Component::Reference::DATA_ITEM);
      parent->addReference(ref);
    }
    else if (xmlStrcmp(reference->name, BAD_CAST "DataItemRef") == 0)
    {
      Component::Reference ref(attrs["idRef"], name, Component::Reference::DATA_ITEM);
      parent->addReference(ref);
    }
    else if (xmlStrcmp(reference->name, BAD_CAST "ComponentRef") == 0)
    {
      Component::Reference ref(attrs["idRef"], name, Component::Reference::COMPONENT);
      parent->addReference(ref);
    }
  }

}  // namespace mtconnect
