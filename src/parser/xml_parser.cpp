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

#include "xml_parser.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/any_range.hpp>
#include <boost/range/functions.hpp>
#include <boost/range/metafunctions.hpp>
#include <boost/range/numeric.hpp>

#include <stdexcept>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "asset/cutting_tool.hpp"
#include "device_model/composition.hpp"
#include "entity/xml_parser.hpp"
#include "logging.hpp"
#include "printer/xml_printer.hpp"

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

namespace mtconnect::parser {
  using namespace observation;
  using namespace device_model;
  using namespace printer;

  extern "C" void XMLCDECL agentXMLErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...)
  {
    va_list args;

    char buffer[2048] = {0};
    va_start(args, msg);
    vsnprintf(buffer, 2046u, msg, args);
    buffer[2047] = '0';
    va_end(args);

    LOG(error) << "XML: " << buffer;
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

  XmlParser::XmlParser() { NAMED_SCOPE("xml.parser"); }

  inline static bool isMTConnectUrn(const char *aUrn)
  {
    return !strncmp(aUrn, "urn:mtconnect.org:MTConnect", 27u);
  }

  std::list<DevicePtr> XmlParser::parseFile(const std::string &filePath, XmlPrinter *aPrinter)
  {
    using namespace boost::adaptors;
    using namespace boost::range;

    std::unique_lock lock(m_mutex);

    if (m_doc)
    {
      xmlFreeDoc(m_doc);
      m_doc = nullptr;
    }

    xmlXPathContextPtr xpathCtx = nullptr;
    xmlXPathObjectPtr devices = nullptr;
    std::list<DevicePtr> deviceList;

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
        string ns((const char *)root->ns->href);
        size_t colon = string::npos;
        if (ns.find_first_of("urn:mtconnect.org:MTConnectDevices") == 0 &&
            (colon = ns.find_last_of(':')) != string::npos)
        {
          auto version = ns.substr(colon + 1);
          LOG(info) << "MTConnect Schema Version of file: " << filePath << " = " << version;
          m_schemaVersion.emplace(version);
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

      entity::ErrorList errors;
      for (int i = 0; i != nodeset->nodeNr; ++i)
      {
        auto device =
            entity::XmlParser::parseXmlNode(Device::getRoot(), nodeset->nodeTab[i], errors);
        if (device)
          deviceList.emplace_back(dynamic_pointer_cast<Device>(device));

        if (!errors.empty())
        {
          for (auto &e : errors)
            LOG(warning) << "Error parsing device: " << e->what();
        }
      }

      xmlXPathFreeObject(devices);
      xmlXPathFreeContext(xpathCtx);
    }
    catch (string e)
    {
      if (devices)
        xmlXPathFreeObject(devices);

      if (xpathCtx)
        xmlXPathFreeContext(xpathCtx);

      LOG(fatal) << "Cannot parse XML file: " << e;
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
    std::unique_lock lock(m_mutex);

    if (m_doc)
    {
      xmlFreeDoc(m_doc);
      m_doc = nullptr;
    }
  }

  void XmlParser::loadDocument(const std::string &doc)
  {
    std::unique_lock lock(m_mutex);

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

      THROW_IF_XML2_NULL(m_doc = xmlReadMemory(doc.c_str(), int32_t(doc.length()), "Devices.xml",
                                               nullptr, XML_PARSE_NOBLANKS));
    }

    catch (string e)
    {
      LOG(fatal) << "Cannot parse XML document: " << e;
      throw;
    }
  }

  void XmlParser::getDataItems(FilterSet &filterSet, const string &inputPath, xmlNodePtr node)
  {
    std::shared_lock lock(m_mutex);

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

      LOG(warning) << "getDataItems: Could not parse path: " << inputPath;
    }
  }
}  // namespace mtconnect::parser
