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

#include "xml_parser.hpp"

#include "composition.hpp"
#include "coordinate_systems.hpp"
#include "assets/cutting_tool.hpp"
#include "motion.hpp"
#include "sensor_configuration.hpp"
#include "solid_model.hpp"
#include "specifications.hpp"
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
  static dlib::logger g_logger("xml.parser");

  template <class T>
  void handleConfiguration(xmlNodePtr node, T *parent);

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

  static inline std::string getRawContent(xmlNodePtr node)
  {
    string res;
    xmlBufferPtr buf;
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    auto count = xmlNodeDump(buf, node->doc, node, 0, 0);
    if (count > 0)
    {
      res = ((const char *)buf->content);
    }
    xmlBufferFree(buf);
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

  // Put all of the attributes of an element into a map
  static inline std::map<string, string> getValidatedAttributes(
      const xmlNodePtr node, const std::map<string, bool> &parameters)
  {
    std::map<string, string> toReturn;
    std::map<string, bool> matched = parameters;

    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next)
    {
      if (attr->type == XML_ATTRIBUTE_NODE)
      {
        string key = (const char *)(attr->name);
        auto it = matched.find(key);
        if (it != matched.end())
        {
          matched.erase(it);
          toReturn[key] = (const char *)attr->children->content;
        }
        else
        {
          g_logger << dlib::LWARN << "Unknown attribute for " << (const char *)node->name << ": "
                   << key << ", skipping";
        }
      }
    }

    for (auto &p : matched)
    {
      if (p.second)
      {
        g_logger << dlib::LWARN << (const char *)node->name
                 << " missing required attribute: " << p.first;
        toReturn.clear();
      }
    }

    return toReturn;
  }

  static inline void forEachElement(const xmlNodePtr node,
                                    map<string, function<void(const xmlNodePtr)>> funs)
  {
    for (xmlNodePtr child = node->children; child; child = child->next)
    {
      if (child->type != XML_ELEMENT_NODE)
        continue;

      auto other = funs.find("OTHERWISE");

      string name((const char *)child->name);
      auto lambda = funs.find(name);
      if (lambda != funs.end())
      {
        lambda->second(child);
      }
      else if (other != funs.end())
      {
        other->second(child);
      }
    }
  }

  struct ThreeSpace
  {
    double m_1, m_2, m_3;
  };

  static inline std::optional<ThreeSpace> getThreeSpace(const string &cdata)
  {
    ThreeSpace v;

    stringstream s(cdata);
    s >> v.m_1 >> v.m_2 >> v.m_3;
    if (s.rdstate() & std::istream::failbit)
    {
      g_logger << dlib::LWARN << "Cannot parse three space value: " << cdata;
      return nullopt;
    }
    else
    {
      return make_optional(v);
    }
  }

  static inline std::optional<Geometry> getGeometry(const xmlNodePtr node, bool hasScale,
                                                    bool hasAxis)
  {
    Geometry geometry;
    forEachElement(node, {{"Transformation",
                           [&geometry](const xmlNodePtr n) {
                             if (geometry.m_location.index() != 0)
                             {
                               g_logger << dlib::LWARN << "Translation or Origin already given";
                               return;
                             }

                             Transformation t;
                             forEachElement(n, {{"Translation",
                                                 [&t](const xmlNodePtr c) {
                                                   auto s = getThreeSpace(getCDATA(c));
                                                   if (s)
                                                     t.m_translation.emplace(s->m_1, s->m_2,
                                                                             s->m_3);
                                                 }},
                                                {"Rotation", [&t](const xmlNodePtr c) {
                                                   auto s = getThreeSpace(getCDATA(c));
                                                   if (s)
                                                     t.m_rotation.emplace(s->m_1, s->m_2, s->m_3);
                                                 }}});
                             if (t.m_rotation || t.m_translation)
                               geometry.m_location.emplace<Transformation>(t);
                             else
                               g_logger << dlib::LWARN << "Cannot parse Translation";
                           }},
                          {"Origin",
                           [&geometry](const xmlNodePtr n) {
                             if (geometry.m_location.index() != 0)
                             {
                               g_logger << dlib::LWARN << "Translation or Origin already given";
                               return;
                             }

                             auto s = getThreeSpace(getCDATA(n));
                             if (s)
                               geometry.m_location.emplace<Origin>(s->m_1, s->m_2, s->m_3);
                             else
                               g_logger << dlib::LWARN << "Cannot parse Origin";
                           }},
                          {"Scale",
                           [&geometry, hasScale](const xmlNodePtr n) {
                             if (hasScale)
                             {
                               auto s = getThreeSpace(getCDATA(n));
                               if (s)
                                 geometry.m_scale.emplace(s->m_1, s->m_2, s->m_3);
                               else
                                 g_logger << dlib::LWARN << "Cannot parse Scale";
                             }
                           }},
                          {"Axis", [&geometry, hasAxis](const xmlNodePtr n) {
                             if (hasAxis)
                             {
                               auto s = getThreeSpace(getCDATA(n));
                               if (s)
                                 geometry.m_axis.emplace(s->m_1, s->m_2, s->m_3);
                               else
                                 g_logger << dlib::LWARN << "Cannot parse Scale";
                             }
                           }}});

    if (geometry.m_location.index() != 0 || geometry.m_scale)
      return make_optional(geometry);
    else
      return nullopt;
  }

  XmlParser::XmlParser()
    : m_handlers(
          {{"Components",
            [this](xmlNodePtr n, Component *p, Device *d) { handleChildren(n, p, d); }},
           {"DataItems",
            [this](xmlNodePtr n, Component *p, Device *d) { handleChildren(n, p, d); }},
           {"References",
            [this](xmlNodePtr n, Component *p, Device *d) { handleChildren(n, p, d); }},
           {"Compositions",
            [this](xmlNodePtr n, Component *p, Device *d) { handleChildren(n, p, d); }},
           {"DataItem", [this](xmlNodePtr n, Component *p, Device *d) { loadDataItem(n, p, d); }},
           {"Reference", [this](xmlNodePtr n, Component *p, Device *d) { handleReference(n, p); }},
           {"DataItemRef",
            [this](xmlNodePtr n, Component *p, Device *d) { handleReference(n, p); }},
           {"ComponentRef",
            [this](xmlNodePtr n, Component *p, Device *d) { handleReference(n, p); }},
           {"Composition",
            [this](xmlNodePtr n, Component *p, Device *d) {
              auto c = handleComposition(n);
              p->addComposition(c);
            }},
           {"Description", [](xmlNodePtr n, Component *p,
                              Device *d) { p->addDescription(getCDATA(n), getAttributes(n)); }},
           {"Configuration",
            [](xmlNodePtr n, Component *p, Device *d) { handleConfiguration(n, p); }}})
  {
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

  void XmlParser::loadDataItem(xmlNodePtr dataItem, Component *parent, Device *device)
  {
    auto d = new DataItem(getAttributes(dataItem));
    d->setComponent(*parent);

    if (dataItem->children)
    {
      for (xmlNodePtr child = dataItem->children; child; child = child->next)
      {
        if (child->type != XML_ELEMENT_NODE)
          continue;

        if (!xmlStrcmp(child->name, BAD_CAST "Source"))
        {
          auto cdata = getCDATA(child);
          auto attributes = getAttributes(child);

          string dataItemId, componentId, compositionId;
          auto it = attributes.find("dataItemId");
          if (it != attributes.end())
            dataItemId = it->second;
          it = attributes.find("componentId");
          if (it != attributes.end())
            componentId = it->second;
          it = attributes.find("compositionId");
          if (it != attributes.end())
            compositionId = it->second;

          d->addSource(cdata, dataItemId, componentId, compositionId);
        }
        else if (!xmlStrcmp(child->name, BAD_CAST "Constraints"))
        {
          for (xmlNodePtr constraint = child->children; constraint; constraint = constraint->next)
          {
            if (constraint->type != XML_ELEMENT_NODE)
              continue;

            auto text = getCDATA(constraint);

            if (!text.empty())
            {
              if (!xmlStrcmp(constraint->name, BAD_CAST "Value"))
                d->addConstrainedValue(text);
              else if (!xmlStrcmp(constraint->name, BAD_CAST "Minimum"))
                d->setMinimum(text);
              else if (!xmlStrcmp(constraint->name, BAD_CAST "Maximum"))
                d->setMaximum(text);
              else if (!xmlStrcmp(constraint->name, BAD_CAST "Filter"))
                d->setMinmumDelta(strtod(text.c_str(), nullptr));
            }
          }
        }
        else if (!xmlStrcmp(child->name, BAD_CAST "Filters"))
        {
          for (xmlNodePtr filter = child->children; filter; filter = filter->next)
          {
            if (filter->type != XML_ELEMENT_NODE)
              continue;

            if (!xmlStrcmp(filter->name, BAD_CAST "Filter"))
            {
              auto text = getCDATA(filter);
              auto type = getAttribute(filter, "type");

              if (!type.empty())
              {
                if (type == "PERIOD")
                  d->setMinmumPeriod(strtod(text.c_str(), nullptr));
                else
                  d->setMinmumDelta(strtod(text.c_str(), nullptr));
              }
              else
                d->setMinmumDelta(strtod(text.c_str(), nullptr));
            }
          }
        }
        else if (!xmlStrcmp(child->name, BAD_CAST "InitialValue"))
        {
          d->setInitialValue(getCDATA(child));
        }
        else if (!xmlStrcmp(child->name, BAD_CAST "ResetTrigger"))
        {
          d->setResetTrigger(getCDATA(child));
        }
        else if (!xmlStrcmp(child->name, BAD_CAST "Definition"))
        {
          loadDataItemDefinition(child, d, device);
        }
        else if (!xmlStrcmp(child->name, BAD_CAST "Relationships"))
        {
          loadDataItemRelationships(child, d, device);
        }
      }
    }

    parent->addDataItem(d);
  }

  void XmlParser::loadDefinition(xmlNodePtr definition, AbstractDefinition *def)
  {
    def->m_key = getAttribute(definition, "key");
    def->m_keyType = getAttribute(definition, "keyType");
    def->m_units = getAttribute(definition, "units");
    def->m_type = getAttribute(definition, "type");
    def->m_subType = getAttribute(definition, "subType");

    forEachElement(
        definition,
        {{"Description", [&def](xmlNodePtr node) { def->m_description = getCDATA(node); }}});
  }

  void XmlParser::loadDefinitions(xmlNodePtr definitions, std::set<EntryDefinition> &result)
  {
    forEachElement(definitions, {{"EntryDefinition", [this, &result](xmlNodePtr node) {
                                    EntryDefinition def;
                                    loadDefinition(node, &def);
                                    forEachElement(
                                        node, {{"CellDefinitions", [this, &def](xmlNodePtr node) {
                                                  loadDefinitions(node, def.m_cells);
                                                }}});
                                    result.emplace(def);
                                  }}});
  }

  void XmlParser::loadDefinitions(xmlNodePtr definitions, std::set<CellDefinition> &result)
  {
    forEachElement(definitions, {
                                    {"CellDefinition",
                                     [this, &result](xmlNodePtr node) {
                                       CellDefinition def;
                                       loadDefinition(node, &def);
                                       result.emplace(def);
                                     }},
                                });
  }

  // Load the data items
  void XmlParser::loadDataItemDefinition(xmlNodePtr definition, DataItem *dataItem, Device *device)
  {
    unique_ptr<DataItemDefinition> def(new DataItemDefinition());

    forEachElement(
        definition,
        {
            {"Description", [&def](xmlNodePtr node) { def->m_description = getCDATA(node); }},
            {"EntryDefinitions",
             [this, &def](xmlNodePtr node) { loadDefinitions(node, def->m_entries); }},
            {"CellDefinitions",
             [this, &def](xmlNodePtr node) { loadDefinitions(node, def->m_cells); }},
        });

    dataItem->setDefinition(def);
  }

  static void addDataItemRelationship(xmlNodePtr node, DataItem *dataItem)
  {
    DataItem::Relationship rel;
    auto attrs = getAttributes(node);

    rel.m_relation = string((char *)node->name);
    rel.m_name = attrs["name"];
    rel.m_type = attrs["type"];
    rel.m_idRef = attrs["idRef"];
    if (!rel.m_type.empty() && !rel.m_idRef.empty())
      dataItem->getRelationships().push_back(rel);
    else
    {
      g_logger << dlib::LWARN << "Bad Data Item Relationship: " << rel.m_relation << ", "
               << rel.m_name << ", " << rel.m_type << ", " << rel.m_idRef
               << ": type or idRef missing, skipping";
    }
  }

  void XmlParser::loadDataItemRelationships(xmlNodePtr relationships, DataItem *dataItem,
                                            Device *device)
  {
    forEachElement(relationships,
                   {{"DataItemRelationship",
                     [&dataItem](xmlNodePtr node) { addDataItemRelationship(node, dataItem); }},
                    {"SpecificationRelationship",
                     [&dataItem](xmlNodePtr node) { addDataItemRelationship(node, dataItem); }}});
  }

  unique_ptr<Composition> XmlParser::handleComposition(xmlNodePtr composition)
  {
    auto comp = make_unique<Composition>();
    comp->m_attributes = getValidatedAttributes(composition, comp->properties());
    if (!comp->m_attributes.empty())
    {
      forEachElement(composition, {{"Description",
                                    [&comp](xmlNodePtr n) {
                                      Description desc;
                                      desc.m_attributes =
                                          getValidatedAttributes(n, desc.properties());
                                      desc.m_body = getCDATA(n);
                                      comp->setDescription(desc);
                                    }},
                                   {"Configuration", [&comp](xmlNodePtr n) {
                                      handleConfiguration(n, (Composition *)comp.get());
                                    }}});
    }
    else
    {
      g_logger << dlib::LWARN << "Skipping Composition";
    }
    return comp;
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

  unique_ptr<ComponentConfiguration> handleSensorConfiguration(xmlNodePtr node)
  {
    // Decode sensor configuration
    string firmware, date, nextDate, initials;
    vector<xmlNodePtr> rest;
    xmlNodePtr channels = nullptr;
    for (xmlNodePtr child = node->children; child; child = child->next)
    {
      string name((const char *)child->name);
      if (name == "FirmwareVersion")
        firmware = getCDATA(child);
      else if (name == "CalibrationDate")
        date = getCDATA(child);
      else if (name == "CalibrationInitials")
        initials = getCDATA(child);
      else if (name == "Channels")
        channels = child;
      else
        rest.emplace_back(child);
    }

    string restText;
    for (auto &text : rest)
      restText.append(getRawContent(text));

    unique_ptr<SensorConfiguration> sensor(
        new SensorConfiguration(firmware, date, nextDate, initials, restText));

    if (channels)
    {
      for (xmlNodePtr channel = channels->children; channel; channel = channel->next)
      {
        string name((const char *)channel->name);
        auto attributes = getAttributes(channel);
        string description, date, nextDate, initials;

        for (xmlNodePtr child = channel->children; child; child = child->next)
        {
          string name((const char *)child->name);
          if (name == "Description")
            description = getCDATA(child);
          else if (name == "CalibrationDate")
            date = getCDATA(child);
          else if (name == "CalibrationInitials")
            initials = getCDATA(child);
        }
        SensorConfiguration::Channel chl(date, nextDate, initials, attributes);
        chl.setDescription(description);
        sensor->addChannel(chl);
      }
    }

    return move(sensor);
  }

  unique_ptr<ComponentConfiguration> handleRelationships(xmlNodePtr node)
  {
    unique_ptr<Relationships> relationships = make_unique<Relationships>();

    for (xmlNodePtr child = node->children; child; child = child->next)
    {
      unique_ptr<Relationship> relationship;
      if (xmlStrcmp(child->name, BAD_CAST "ComponentRelationship") == 0)
      {
        unique_ptr<ComponentRelationship> crel{new ComponentRelationship()};
        crel->m_idRef = getAttribute(child, "idRef");
        relationship = std::move(crel);
      }
      else if (xmlStrcmp(child->name, BAD_CAST "DeviceRelationship") == 0)
      {
        unique_ptr<DeviceRelationship> drel{new DeviceRelationship()};
        drel->m_href = getAttribute(child, "href");
        drel->m_role = getAttribute(child, "role");
        drel->m_deviceUuidRef = getAttribute(child, "deviceUuidRef");

        relationship = std::move(drel);
      }
      else
      {
        g_logger << dlib::LWARN << "Bad Relationship: " << (const char *)child->name
                 << ", skipping";
      }

      if (relationship)
      {
        auto attrs = getAttributes(child);
        relationship->m_id = attrs["id"];
        relationship->m_name = attrs["name"];
        relationship->m_type = attrs["type"];
        relationship->m_criticality = attrs["criticality"];

        relationships->addRelationship(relationship);
      }
    }

    return move(relationships);
  }

  template <class T>
  unique_ptr<T> handleGeometricConfiguration(xmlNodePtr node)
  {
    unique_ptr<T> model = make_unique<T>();
    model->m_attributes = getValidatedAttributes(node, model->properties());
    if (!model->m_attributes.empty())
    {
      model->m_geometry = getGeometry(node, model->hasScale(), model->hasAxis());
    }
    else
    {
      g_logger << dlib::LWARN << "Skipping Geometric Definition";
    }

    if (model->hasDescription())
    {
      forEachElement(
          node, {{"Description", [&model](xmlNodePtr n) { model->m_description = getCDATA(n); }}});
    }

    return model;
  }

  unique_ptr<ComponentConfiguration> handleCoordinateSystems(xmlNodePtr node)
  {
    unique_ptr<CoordinateSystems> systems = make_unique<CoordinateSystems>();

    for (xmlNodePtr child = node->children; child; child = child->next)
    {
      systems->addCoordinateSystem(handleGeometricConfiguration<CoordinateSystem>(child).release());
    }

    return move(systems);
  }

  unique_ptr<ComponentConfiguration> handleSpecifications(xmlNodePtr node)
  {
    unique_ptr<Specifications> specifications = make_unique<Specifications>();
    for (xmlNodePtr child = node->children; child; child = child->next)
    {
      auto attrs = getAttributes(child);

      std::string klass((const char *)child->name);
      if (klass == "Specification" || klass == "ProcessSpecification")
      {
        unique_ptr<Specification> spec{new Specification(klass)};

        spec->m_id = attrs["id"];
        spec->m_name = attrs["name"];
        spec->m_type = attrs["type"];
        spec->m_subType = attrs["subType"];
        spec->m_units = attrs["units"];
        spec->m_dataItemIdRef = attrs["dataItemIdRef"];
        spec->m_compositionIdRef = attrs["compositionIdRef"];
        spec->m_coordinateSystemIdRef = attrs["coordinateSystemIdRef"];
        spec->m_originator = attrs["originator"];

        for (xmlNodePtr limit = child->children; limit; limit = limit->next)
        {
          if (spec->hasGroups())
          {
            std::string group((const char *)limit->name);

            for (xmlNodePtr val = limit->children; val; val = val->next)
            {
              std::string name((const char *)val->name);
              std::string value(getCDATA(val));
              spec->addLimitForGroup(group, name, stod(value));
            }
          }
          else
          {
            std::string name((const char *)limit->name);
            std::string value(getCDATA(limit));
            spec->addLimit(name, stod(value));
          }
        }

        specifications->addSpecification(spec);
      }
      else
      {
        g_logger << dlib::LWARN << "Bad Specifictation type " << klass << ", skipping";
      }
    }

    return move(specifications);
  }

  template <class T>
  void handleConfiguration(xmlNodePtr node, T *parent)
  {
    forEachElement(
        node,
        {{{"SensorConfiguration",
           [&parent](xmlNodePtr n) {
             auto s = handleSensorConfiguration(n);
             parent->addConfiguration(s);
           }},
          {"Relationships",
           [&parent](xmlNodePtr n) {
             auto r = handleRelationships(n);
             parent->addConfiguration(r);
           }},
          {"CoordinateSystems",
           [&parent](xmlNodePtr n) {
             auto c = handleCoordinateSystems(n);
             parent->addConfiguration(c);
           }},
          {"Specifications",
           [&parent](xmlNodePtr n) {
             auto s = handleSpecifications(n);
             parent->addConfiguration(s);
           }},
          {"SolidModel",
           [&parent](xmlNodePtr n) {
             unique_ptr<ComponentConfiguration> g(handleGeometricConfiguration<SolidModel>(n));
             parent->addConfiguration(g);
           }},
          {"Motion",
           [&parent](xmlNodePtr n) {
             unique_ptr<ComponentConfiguration> g(handleGeometricConfiguration<Motion>(n));
             parent->addConfiguration(g);
           }},
          {"OTHERWISE", [&parent](xmlNodePtr n) {
             unique_ptr<ComponentConfiguration> ext(
                 new ExtendedComponentConfiguration(getRawContent(n)));
             parent->addConfiguration(ext);
           }}}});
  }
}  // namespace mtconnect
