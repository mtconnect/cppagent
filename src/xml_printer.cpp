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

#include "xml_printer.hpp"

#include "assets/asset.hpp"
#include "assets/cutting_tool.hpp"
#include "device_model/composition.hpp"
#include "device_model/device.hpp"
#include "device_model/configuration/configuration.hpp"
#include "entity/xml_printer.hpp"
#include "version.h"

#include <dlib/logger.h>
#include <dlib/sockets.h>

#include <libxml/xmlwriter.h>

#include <set>
#include <typeindex>
#include <typeinfo>
#include <utility>

#define strfy(line) #line
#define THROW_IF_XML2_ERROR(expr)                                           \
  if ((expr) < 0)                                                           \
  {                                                                         \
    throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); \
  }
#define THROW_IF_XML2_NULL(expr)                                            \
  if (!(expr))                                                              \
  {                                                                         \
    throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); \
  }

using namespace std;

namespace mtconnect
{
  static dlib::logger g_logger("xml.printer");
  using namespace observation;
  using namespace device_model::configuration;

  class XmlWriter
  {
  public:
    XmlWriter(bool pretty) : m_writer(nullptr), m_buf(nullptr)
    {
      THROW_IF_XML2_NULL(m_buf = xmlBufferCreate());
      THROW_IF_XML2_NULL(m_writer = xmlNewTextWriterMemory(m_buf, 0));
      if (pretty)
      {
        THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(m_writer, 1));
        THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(m_writer, BAD_CAST "  "));
      }
    }

    ~XmlWriter()
    {
      if (m_writer != nullptr)
      {
        xmlFreeTextWriter(m_writer);
        m_writer = nullptr;
      }
      if (m_buf != nullptr)
      {
        xmlBufferFree(m_buf);
        m_buf = nullptr;
      }
    }

    operator xmlTextWriterPtr() { return m_writer; }

    string getContent()
    {
      if (m_writer != nullptr)
      {
        THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(m_writer));
        xmlFreeTextWriter(m_writer);
        m_writer = nullptr;
      }
      return string((char *)m_buf->content, m_buf->use);
    }

  protected:
    xmlTextWriterPtr m_writer;
    xmlBufferPtr m_buf;
  };

  XmlPrinter::XmlPrinter(const string version, bool pretty)
    : Printer(pretty), m_schemaVersion(version)
  {
    if (m_schemaVersion.empty())
      m_schemaVersion = to_string(AGENT_VERSION_MAJOR) + "." + to_string(AGENT_VERSION_MINOR);
  }

  void XmlPrinter::addDevicesNamespace(const std::string &urn, const std::string &location,
                                       const std::string &prefix)
  {
    pair<string, SchemaNamespace> item;
    item.second.mUrn = urn;
    item.second.mSchemaLocation = location;
    item.first = prefix;
    
    m_deviceNsSet.insert(prefix);

    m_devicesNamespaces.insert(item);
  }

  void XmlPrinter::clearDevicesNamespaces() { m_devicesNamespaces.clear(); }

  string XmlPrinter::getDevicesUrn(const std::string &prefix)
  {
    auto ns = m_devicesNamespaces.find(prefix);
    if (ns != m_devicesNamespaces.end())
      return ns->second.mUrn;
    else
      return "";
  }

  string XmlPrinter::getDevicesLocation(const std::string &prefix)
  {
    auto ns = m_devicesNamespaces.find(prefix);
    if (ns != m_devicesNamespaces.end())
      return ns->second.mSchemaLocation;
    else
      return "";
  }

  void XmlPrinter::addErrorNamespace(const std::string &urn, const std::string &location,
                                     const std::string &prefix)
  {
    pair<string, SchemaNamespace> item;
    item.second.mUrn = urn;
    item.second.mSchemaLocation = location;
    item.first = prefix;
    
    m_errorNsSet.insert(prefix);

    m_errorNamespaces.insert(item);
  }

  void XmlPrinter::clearErrorNamespaces() { m_errorNamespaces.clear(); }

  string XmlPrinter::getErrorUrn(const std::string &prefix)
  {
    auto ns = m_errorNamespaces.find(prefix);
    if (ns != m_errorNamespaces.end())
      return ns->second.mUrn;
    else
      return "";
  }

  string XmlPrinter::getErrorLocation(const std::string &prefix)
  {
    auto ns = m_errorNamespaces.find(prefix);
    if (ns != m_errorNamespaces.end())
      return ns->second.mSchemaLocation;
    else
      return "";
  }

  void XmlPrinter::addStreamsNamespace(const std::string &urn, const std::string &location,
                                       const std::string &prefix)
  {
    pair<string, SchemaNamespace> item;
    item.second.mUrn = urn;
    item.second.mSchemaLocation = location;
    item.first = prefix;
    
    m_streamsNsSet.insert(prefix);

    m_streamsNamespaces.insert(item);
  }

  void XmlPrinter::clearStreamsNamespaces() { m_streamsNamespaces.clear(); }

  void XmlPrinter::setSchemaVersion(const std::string &version) { m_schemaVersion = version; }

  const std::string &XmlPrinter::getSchemaVersion() { return m_schemaVersion; }

  string XmlPrinter::getStreamsUrn(const std::string &prefix)
  {
    auto ns = m_streamsNamespaces.find(prefix);
    if (ns != m_streamsNamespaces.end())
      return ns->second.mUrn;
    else
      return "";
  }

  string XmlPrinter::getStreamsLocation(const std::string &prefix)
  {
    auto ns = m_streamsNamespaces.find(prefix);
    if (ns != m_streamsNamespaces.end())
      return ns->second.mSchemaLocation;
    else
      return "";
  }

  void XmlPrinter::addAssetsNamespace(const std::string &urn, const std::string &location,
                                      const std::string &prefix)
  {
    pair<string, SchemaNamespace> item;
    item.second.mUrn = urn;
    item.second.mSchemaLocation = location;
    item.first = prefix;
    
    m_assetsNsSet.insert(prefix);

    m_assetsNamespaces.insert(item);
  }

  void XmlPrinter::clearAssetsNamespaces() { m_assetsNamespaces.clear(); }

  string XmlPrinter::getAssetsUrn(const std::string &prefix)
  {
    auto ns = m_assetsNamespaces.find(prefix);
    if (ns != m_assetsNamespaces.end())
      return ns->second.mUrn;
    else
      return "";
  }

  string XmlPrinter::getAssetsLocation(const std::string &prefix)
  {
    auto ns = m_assetsNamespaces.find(prefix);
    if (ns != m_assetsNamespaces.end())
      return ns->second.mSchemaLocation;
    else
      return "";
  }

  void XmlPrinter::setStreamStyle(const std::string &style) { m_streamsStyle = style; }

  void XmlPrinter::setDevicesStyle(const std::string &style) { m_devicesStyle = style; }

  void XmlPrinter::setErrorStyle(const std::string &style) { m_errorStyle = style; }

  void XmlPrinter::setAssetsStyle(const std::string &style) { m_assetsStyle = style; }

  static inline void printRawContent(xmlTextWriterPtr writer, const char *element,
                                     const std::string &text)
  {
    if (!text.empty())
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST element));
      THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST text.c_str()));
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
    }
  }

  static inline void addAttribute(xmlTextWriterPtr writer, const char *key,
                                  const std::string &value)
  {
    if (!value.empty())
      THROW_IF_XML2_ERROR(
          xmlTextWriterWriteAttribute(writer, BAD_CAST key, BAD_CAST value.c_str()));
  }

  void addAttributes(xmlTextWriterPtr writer, const std::map<string, string> &attributes)
  {
    for (const auto &attr : attributes)
    {
      if (!attr.second.empty())
      {
        THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST attr.first.c_str(),
                                                        BAD_CAST attr.second.c_str()));
      }
    }
  }

  static inline void openElement(xmlTextWriterPtr writer, const char *name)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST name));
  }

  static inline void closeElement(xmlTextWriterPtr writer)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
  }

  class AutoElement
  {
  public:
    AutoElement(xmlTextWriterPtr writer) : m_writer(writer) {}
    AutoElement(xmlTextWriterPtr writer, const char *name, string key = "")
      : m_writer(writer), m_name(name), m_key(std::move(key))
    {
      openElement(writer, name);
    }
    AutoElement(xmlTextWriterPtr writer, const string &name, string key = "")
      : m_writer(writer), m_name(name), m_key(std::move(key))
    {
      openElement(writer, name.c_str());
    }
    bool reset(const string &name, const string &key = "")
    {
      if (name != m_name || m_key != key)
      {
        if (!m_name.empty())
          closeElement(m_writer);
        if (!name.empty())
          openElement(m_writer, name.c_str());
        m_name = name;
        m_key = key;
        return true;
      }
      else
      {
        return false;
      }
    }
    ~AutoElement()
    {
      if (!m_name.empty())
        xmlTextWriterEndElement(m_writer);
    }

    const string &key() const { return m_key; }
    const string &name() const { return m_name; }

  protected:
    xmlTextWriterPtr m_writer;
    string m_name;
    string m_key;
  };

  void addSimpleElement(xmlTextWriterPtr writer, const string &element, const string &body,
                        const map<string, string> &attributes = {}, bool raw = false)
  {
    AutoElement ele(writer, element);

    if (!attributes.empty())
      addAttributes(writer, attributes);

    if (!body.empty())
    {
      xmlChar *text = nullptr;
      if (!raw)
        text = xmlEncodeEntitiesReentrant(nullptr, BAD_CAST body.c_str());
      else
        text = BAD_CAST body.c_str();
      THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
      if (!raw)
        xmlFree(text);
    }
  }

  std::string XmlPrinter::printErrors(const unsigned int instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const ProtoErrorList &list) const
  {
    string ret;

    try
    {
      XmlWriter writer(m_pretty);

      initXmlDoc(writer, eERROR, instanceId, bufferSize, 0, 0, nextSeq, nextSeq - 1);

      {
        AutoElement e1(writer, "Errors");
        for (auto &e : list)
        {
          addSimpleElement(writer, "Error", e.second, {{"errorCode", e.first}});
        }
      }
      closeElement(writer);  // MTConnectError

      // Cleanup
      ret = writer.getContent();
    }
    catch (string error)
    {
      g_logger << dlib::LERROR << "printError: " << error;
    }
    catch (...)
    {
      g_logger << dlib::LERROR << "printError: unknown error";
    }

    return ret;
  }

  string XmlPrinter::printProbe(const unsigned int instanceId, const unsigned int bufferSize,
                                const uint64_t nextSeq, const unsigned int assetBufferSize,
                                const unsigned int assetCount, const list<Device *> &deviceList,
                                const std::map<std::string, int> *count) const
  {
    string ret;

    try
    {
      XmlWriter writer(m_pretty);

      initXmlDoc(writer, eDEVICES, instanceId, bufferSize, assetBufferSize, assetCount, nextSeq, 0,
                 nextSeq - 1, count);

      {
        AutoElement devices(writer, "Devices");
        for (const auto dev : deviceList)
          printProbeHelper(writer, dev, dev->getClass().c_str());
      }
      closeElement(writer);  // MTConnectDevices

      ret = writer.getContent();
    }
    catch (string error)
    {
      g_logger << dlib::LERROR << "printProbe: " << error;
    }
    catch (...)
    {
      g_logger << dlib::LERROR << "printProbe: unknown error";
    }

    return ret;
  }

  void printConfiguration(xmlTextWriterPtr writer,
                          const unique_ptr<Configuration> &configuration)
  {
    AutoElement configEle(writer, "Configuration");
    entity::XmlPrinter printer;

    printer.print(writer, configuration->getEntity(), {});
  }

  void printComposition(xmlTextWriterPtr writer,
                          const unique_ptr<Composition> &compositions)
  {
    AutoElement configEle(writer, "Compositions");
    entity::XmlPrinter printer;

    printer.print(writer, compositions->getEntity(), {});
  }

  void XmlPrinter::printProbeHelper(xmlTextWriterPtr writer, Component *component,
                                    const char *name) const
  {
    AutoElement ele(writer, name);
    addAttributes(writer, component->getAttributes());

    const auto &desc = component->getDescription();
    const auto &body = component->getDescriptionBody();

    if (!desc.empty() || !body.empty())
      addSimpleElement(writer, "Description", body, desc);

    const auto &configurations = component->getConfiguration();

    if (!configurations.empty())
    {
      printConfiguration(writer, configurations.front());
    }

    auto datum = component->getDataItems();

    if (!datum.empty())
    {
      AutoElement ele(writer, "DataItems");

      for (const auto data : datum)
        printDataItem(writer, data);
    }

    const auto children = component->getChildren();

    if (!children.empty())
    {
      AutoElement ele(writer, "Components");

      for (const auto &child : children)
      {
        const char *name = nullptr;
        if (!child->getPrefix().empty())
        {
          const auto ns = m_devicesNamespaces.find(child->getPrefix());
          if (ns != m_devicesNamespaces.end())
            name = child->getPrefixedClass().c_str();
        }

        if (!name)
          name = child->getClass().c_str();

        printProbeHelper(writer, child, name);
      }
    }

    if (!component->getCompositions().empty())
    {
      const auto &compositions = component->getCompositions();
      printComposition(writer, compositions.front());

    }

    if (!component->getReferences().empty())
    {
      AutoElement ele(writer, "References");

      for (const auto &ref : component->getReferences())
      {
        if (m_schemaVersion >= "1.4")
        {
          if (ref.m_type == Component::Reference::DATA_ITEM)
          {
            addSimpleElement(writer, "DataItemRef", "",
                             {{"idRef", ref.m_id}, {"name", ref.m_name}});
          }
          else if (ref.m_type == Component::Reference::COMPONENT)
          {
            addSimpleElement(writer, "ComponentRef", "",
                             {{"idRef", ref.m_id}, {"name", ref.m_name}});
          }
        }
        else if (ref.m_type == Component::Reference::DATA_ITEM)
        {
          addSimpleElement(writer, "Reference", "",
                           {{"dataItemId", ref.m_id}, {"name", ref.m_name}});
        }
      }
    }
  }

  void XmlPrinter::printDataItem(xmlTextWriterPtr writer, DataItem *dataItem) const
  {
    AutoElement ele(writer, "DataItem");

    addAttributes(writer, dataItem->getAttributes());

    if (!dataItem->getSource().empty() || !dataItem->getSourceDataItemId().empty() ||
        !dataItem->getSourceComponentId().empty() || !dataItem->getSourceCompositionId().empty())
    {
      addSimpleElement(writer, "Source", dataItem->getSource(),
                       {{"dataItemId", dataItem->getSourceDataItemId()},
                        {"componentId", dataItem->getSourceComponentId()},
                        {"compositionId", dataItem->getSourceCompositionId()}});
    }

    if (dataItem->hasConstraints())
    {
      AutoElement ele(writer, "Constraints");

      auto s = dataItem->getMaximum();

      if (!s.empty())
        addSimpleElement(writer, "Maximum", s);

      s = dataItem->getMinimum();

      if (!s.empty())
        addSimpleElement(writer, "Minimum", s);

      const auto &values = dataItem->getConstrainedValues();
      for (const auto &value : values)
        addSimpleElement(writer, "Value", value);
    }

    if (dataItem->hasMinimumDelta() || dataItem->hasMinimumPeriod())
    {
      AutoElement ele(writer, "Filters");
      if (dataItem->hasMinimumDelta())
      {
        map<string, string> attributes;
        auto value = format(dataItem->getFilterValue());
        addSimpleElement(writer, "Filter", value, {{"type", "MINIMUM_DELTA"}});
      }

      if (dataItem->hasMinimumPeriod())
      {
        map<string, string> attributes;
        auto value = format(dataItem->getFilterPeriod());
        attributes["type"] = "PERIOD";
        addSimpleElement(writer, "Filter", value, {{"type", "PERIOD"}});
      }
    }

    if (dataItem->hasInitialValue())
      addSimpleElement(writer, "InitialValue", dataItem->getInitialValue());

    if (dataItem->hasResetTrigger())
      addSimpleElement(writer, "ResetTrigger", dataItem->getResetTrigger());

    if (dataItem->hasDefinition())
      printDataItemDefinition(writer, dataItem->getDefinition());

    printDataItemRelationships(writer, dataItem->getRelationships());
  }

  void XmlPrinter::printDataItemDefinition(xmlTextWriterPtr writer,
                                           const DataItemDefinition &definition) const
  {
    AutoElement ele(writer, "Definition");

    if (!definition.m_description.empty())
      addSimpleElement(writer, "Description", definition.m_description);

    if (!definition.m_entries.empty())
    {
      AutoElement ele(writer, "EntryDefinitions");
      for (const auto &entry : definition.m_entries)
      {
        AutoElement ele(writer, "EntryDefinition");
        addAttributes(writer, {{string("key"), entry.m_key},
                               {string("keyType"), entry.m_keyType},
                               {string("units"), entry.m_units},
                               {string("type"), entry.m_type},
                               {string("subType"), entry.m_subType}});
        if (!entry.m_description.empty())
          addSimpleElement(writer, "Description", entry.m_description);
        printCellDefinitions(writer, entry.m_cells);
      }
    }

    printCellDefinitions(writer, definition.m_cells);
  }

  void XmlPrinter::printCellDefinitions(xmlTextWriterPtr writer,
                                        const std::set<CellDefinition> &definitions) const
  {
    if (!definitions.empty())
    {
      AutoElement ele(writer, "CellDefinitions");
      for (const auto &entry : definitions)
      {
        AutoElement ele(writer, "CellDefinition");
        addAttributes(writer, {{string("key"), entry.m_key},
                               {string("keyType"), entry.m_keyType},
                               {string("units"), entry.m_units},
                               {string("type"), entry.m_type},
                               {string("subType"), entry.m_subType}});
        if (!entry.m_description.empty())
          addSimpleElement(writer, "Description", entry.m_description);
      }
    }
  }

  void XmlPrinter::printDataItemRelationships(
      xmlTextWriterPtr writer, const std::list<DataItem::Relationship> &relations) const
  {
    if (relations.size() > 0)
    {
      AutoElement ele(writer, "Relationships");
      for (const auto &rel : relations)
      {
        addSimpleElement(writer, rel.m_relation, "",
                         {{string("name"), rel.m_name},
                          {string("type"), rel.m_type},
                          {string("idRef"), rel.m_idRef}});
      }
    }
  }

  string XmlPrinter::printSample(const unsigned int instanceId, const unsigned int bufferSize,
                                 const uint64_t nextSeq, const uint64_t firstSeq,
                                 const uint64_t lastSeq, ObservationList &observations) const
  {
    string ret;

    try
    {
      XmlWriter writer(m_pretty);

      initXmlDoc(writer, eSTREAMS, instanceId, bufferSize, 0, 0, nextSeq, firstSeq, lastSeq);

      AutoElement streams(writer, "Streams");

      // Sort the vector by category.
      if (observations.size() > 0)
      {
        observations.sort(ObservationCompare);

        AutoElement deviceElement(writer);
        {
          AutoElement componentStreamElement(writer);
          {
            AutoElement categoryElement(writer);

            for (auto &observation : observations)
            {
              const auto &dataItem = observation->getDataItem();
              const auto &component = dataItem->getComponent();
              const auto &device = component->getDevice();

              if (deviceElement.key() != device->getId())
              {
                categoryElement.reset("");
                componentStreamElement.reset("");

                deviceElement.reset("DeviceStream", device->getId());
                addAttribute(writer, "name", device->getName());
                addAttribute(writer, "uuid", device->getUuid());
              }

              if (componentStreamElement.key() != component->getId())
              {
                categoryElement.reset("");

                componentStreamElement.reset("ComponentStream", component->getId());
                addAttribute(writer, "component", component->getClass());
                addAttribute(writer, "name", component->getName());
                addAttribute(writer, "componentId", component->getId());
              }

              categoryElement.reset(dataItem->getCategoryText());

              addObservation(writer, observation);
            }
          }
        }
      }

      streams.reset("");
      closeElement(writer);  // MTConnectStreams

      ret = writer.getContent();
    }
    catch (string error)
    {
      g_logger << dlib::LERROR << "printSample: " << error;
    }
    catch (...)
    {
      g_logger << dlib::LERROR << "printSample: unknown error";
    }

    return ret;
  }

  string XmlPrinter::printAssets(const unsigned int instanceId, const unsigned int bufferSize,
                                 const unsigned int assetCount, const AssetList &assets) const
  {
    string ret;
    try
    {
      XmlWriter writer(m_pretty);
      initXmlDoc(writer, eASSETS, instanceId, 0u, bufferSize, assetCount, 0ull);

      {
        AutoElement ele(writer, "Assets");
        entity::XmlPrinter printer;

        for (const auto &asset : assets)
        {
          printer.print(writer, asset, m_assetsNsSet);
        }
      }

      ret = writer.getContent();
    }
    catch (string error)
    {
      g_logger << dlib::LERROR << "printAssets: " << error;
    }
    catch (...)
    {
      g_logger << dlib::LERROR << "printAssets: unknown error";
    }

    return ret;
  }

  void XmlPrinter::addObservation(xmlTextWriterPtr writer, ObservationPtr result) const
  {
    entity::XmlPrinter printer;
    printer.print(writer, result, m_streamsNsSet);
  }

  void XmlPrinter::initXmlDoc(xmlTextWriterPtr writer, EDocumentType aType,
                              const unsigned int instanceId, const unsigned int bufferSize,
                              const unsigned int assetBufferSize, const unsigned int assetCount,
                              const uint64_t nextSeq, const uint64_t firstSeq,
                              const uint64_t lastSeq, const map<string, int> *count) const
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartDocument(writer, nullptr, "UTF-8", nullptr));

    // TODO: Cache the locations and header attributes.
    // Write the root element
    string xmlType, style;
    const map<string, SchemaNamespace> *namespaces;

    switch (aType)
    {
      case eERROR:
        namespaces = &m_errorNamespaces;
        style = m_errorStyle;
        xmlType = "Error";
        break;

      case eSTREAMS:
        namespaces = &m_streamsNamespaces;
        style = m_streamsStyle;
        xmlType = "Streams";
        break;

      case eDEVICES:
        namespaces = &m_devicesNamespaces;
        style = m_devicesStyle;
        xmlType = "Devices";
        break;

      case eASSETS:
        namespaces = &m_assetsNamespaces;
        style = m_assetsStyle;
        xmlType = "Assets";
        break;
    }

    if (!style.empty())
    {
      string pi = R"(xml-stylesheet type="text/xsl" href=")" + style + '"';
      THROW_IF_XML2_ERROR(xmlTextWriterStartPI(writer, BAD_CAST pi.c_str()));
      THROW_IF_XML2_ERROR(xmlTextWriterEndPI(writer));
    }

    string rootName = "MTConnect" + xmlType;
    string xmlns = "urn:mtconnect.org:" + rootName + ":" + m_schemaVersion;
    string location;

    openElement(writer, rootName.c_str());

    // Always make the default namespace and the m: namespace MTConnect default.
    addAttribute(writer, "xmlns:m", xmlns);
    addAttribute(writer, "xmlns", xmlns);

    // Alwats add the xsi namespace
    addAttribute(writer, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");

    string mtcLocation;

    // Add in the other namespaces if they exist
    for (const auto &ns : *namespaces)
    {
      // Skip the mtconnect ns (always m)
      if (ns.first != "m")
      {
        string attr = "xmlns:" + ns.first;
        addAttribute(writer, attr.c_str(), ns.second.mUrn);

        if (location.empty() && !ns.second.mSchemaLocation.empty())
        {
          // Always take the first location. There should only be one location!
          location = ns.second.mUrn + " " + ns.second.mSchemaLocation;
        }
      }
      else if (!ns.second.mSchemaLocation.empty())
      {
        // This is the mtconnect namespace
        mtcLocation = xmlns + " " + ns.second.mSchemaLocation;
      }
    }

    // Write the schema location
    if (location.empty() && !mtcLocation.empty())
      location = mtcLocation;
    else if (location.empty())
      location = xmlns + " http://schemas.mtconnect.org/schemas/" + rootName + "_" +
                 m_schemaVersion + ".xsd";

    addAttribute(writer, "xsi:schemaLocation", location);

    // Create the header
    AutoElement header(writer, "Header");

    addAttribute(writer, "creationTime", getCurrentTime(GMT));

    static std::string sHostname;
    if (sHostname.empty())
    {
      if (dlib::get_local_hostname(sHostname))
        sHostname = "localhost";
    }
    addAttribute(writer, "sender", sHostname);
    addAttribute(writer, "instanceId", to_string(instanceId));

    char version[32] = {0};
    sprintf(version, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR, AGENT_VERSION_PATCH,
            AGENT_VERSION_BUILD);
    addAttribute(writer, "version", version);

    if (aType == eASSETS || aType == eDEVICES)
    {
      addAttribute(writer, "assetBufferSize", to_string(assetBufferSize));
      addAttribute(writer, "assetCount", to_string(assetCount));
    }

    if (aType == eDEVICES || aType == eERROR || aType == eSTREAMS)
    {
      addAttribute(writer, "bufferSize", to_string(bufferSize));
    }

    if (aType == eSTREAMS)
    {
      // Add additional attribtues for streams
      addAttribute(writer, "nextSequence", to_string(nextSeq));
      addAttribute(writer, "firstSequence", to_string(firstSeq));
      addAttribute(writer, "lastSequence", to_string(lastSeq));
    }

    if (aType == eDEVICES && count && !count->empty())
    {
      AutoElement ele(writer, "AssetCounts");

      for (const auto &pair : *count)
      {
        addSimpleElement(writer, "AssetCount", to_string(pair.second), {{"assetType", pair.first}});
      }
    }
  }
}  // namespace mtconnect
