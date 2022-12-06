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

#include "entity/xml_printer.hpp"

#include <boost/asio/ip/host_name.hpp>

#include <set>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include <libxml/xmlwriter.h>

#include "asset/asset.hpp"
#include "asset/cutting_tool.hpp"
#include "device_model/composition.hpp"
#include "device_model/configuration/configuration.hpp"
#include "device_model/device.hpp"
#include "logging.hpp"
#include "version.h"
#include "xml_printer.hpp"

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

namespace mtconnect::printer {
  using namespace observation;
  using namespace asset;
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

  XmlPrinter::XmlPrinter(bool pretty) : Printer(pretty) { NAMED_SCOPE("xml.printer"); }

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

    m_assetNsSet.insert(prefix);

    m_assetNamespaces.insert(item);
  }

  void XmlPrinter::clearAssetsNamespaces() { m_assetNamespaces.clear(); }

  string XmlPrinter::getAssetsUrn(const std::string &prefix)
  {
    auto ns = m_assetNamespaces.find(prefix);
    if (ns != m_assetNamespaces.end())
      return ns->second.mUrn;
    else
      return "";
  }

  string XmlPrinter::getAssetsLocation(const std::string &prefix)
  {
    auto ns = m_assetNamespaces.find(prefix);
    if (ns != m_assetNamespaces.end())
      return ns->second.mSchemaLocation;
    else
      return "";
  }

  void XmlPrinter::setStreamStyle(const std::string &style) { m_streamsStyle = style; }

  void XmlPrinter::setDevicesStyle(const std::string &style) { m_devicesStyle = style; }

  void XmlPrinter::setErrorStyle(const std::string &style) { m_errorStyle = style; }

  void XmlPrinter::setAssetsStyle(const std::string &style) { m_assetStyle = style; }

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

  std::string XmlPrinter::printErrors(const uint64_t instanceId, const unsigned int bufferSize,
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
      LOG(error) << "printError: " << error;
    }
    catch (...)
    {
      LOG(error) << "printError: unknown error";
    }

    return ret;
  }

  string XmlPrinter::printProbe(const uint64_t instanceId, const unsigned int bufferSize,
                                const uint64_t nextSeq, const unsigned int assetBufferSize,
                                const unsigned int assetCount, const list<DevicePtr> &deviceList,
                                const std::map<std::string, size_t> *count) const
  {
    string ret;

    try
    {
      XmlWriter writer(m_pretty);

      initXmlDoc(writer, eDEVICES, instanceId, bufferSize, assetBufferSize, assetCount, nextSeq, 0,
                 nextSeq - 1, count);

      {
        AutoElement devices(writer, "Devices");
        entity::XmlPrinter printer;

        for (auto &device : deviceList)
          printer.print(writer, device, m_deviceNsSet);
      }
      closeElement(writer);  // MTConnectDevices

      ret = writer.getContent();
    }
    catch (string error)
    {
      LOG(error) << "printProbe: " << error;
    }
    catch (...)
    {
      LOG(error) << "printProbe: unknown error";
    }

    return ret;
  }

  string XmlPrinter::printSample(const uint64_t instanceId, const unsigned int bufferSize,
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
              if (!observation->isOrphan())
              {
                const auto &dataItem = observation->getDataItem();
                const auto &component = dataItem->getComponent();
                const auto &device = component->getDevice();

                if (deviceElement.key() != device->getId())
                {
                  categoryElement.reset("");
                  componentStreamElement.reset("");

                  deviceElement.reset("DeviceStream", device->getId());
                  addAttribute(writer, "name", *device->getComponentName());
                  addAttribute(writer, "uuid", *device->getUuid());
                }

                if (componentStreamElement.key() != component->getId())
                {
                  categoryElement.reset("");

                  componentStreamElement.reset("ComponentStream", component->getId());
                  addAttribute(writer, "component", component->getName());
                  if (component->getComponentName())
                    addAttribute(writer, "name", *component->getComponentName());
                  addAttribute(writer, "componentId", component->getId());
                }

                categoryElement.reset(dataItem->getCategoryText());

                addObservation(writer, observation);
              }
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
      LOG(error) << "printSample: " << error;
    }
    catch (...)
    {
      LOG(error) << "printSample: unknown error";
    }

    return ret;
  }

  string XmlPrinter::printAssets(const uint64_t instanceId, const unsigned int bufferSize,
                                 const unsigned int assetCount, const AssetList &asset) const
  {
    string ret;
    try
    {
      XmlWriter writer(m_pretty);
      initXmlDoc(writer, eASSETS, instanceId, 0u, bufferSize, assetCount, 0ull);

      {
        AutoElement ele(writer, "Assets");
        entity::XmlPrinter printer;

        for (const auto &asset : asset)
        {
          printer.print(writer, asset, m_assetNsSet);
        }
      }

      ret = writer.getContent();
    }
    catch (string error)
    {
      LOG(error) << "printAssets: " << error;
    }
    catch (...)
    {
      LOG(error) << "printAssets: unknown error";
    }

    return ret;
  }

  void XmlPrinter::addObservation(xmlTextWriterPtr writer, ObservationPtr result) const
  {
    entity::XmlPrinter printer;
    printer.print(writer, result, m_streamsNsSet);
  }

  void XmlPrinter::initXmlDoc(xmlTextWriterPtr writer, EDocumentType aType,
                              const uint64_t instanceId, const unsigned int bufferSize,
                              const unsigned int assetBufferSize, const unsigned int assetCount,
                              const uint64_t nextSeq, const uint64_t firstSeq,
                              const uint64_t lastSeq, const map<string, size_t> *count) const
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
        namespaces = &m_assetNamespaces;
        style = m_assetStyle;
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
    string xmlns = "urn:mtconnect.org:" + rootName + ":" + *m_schemaVersion;
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
                 *m_schemaVersion + ".xsd";

    addAttribute(writer, "xsi:schemaLocation", location);

    // Create the header
    AutoElement header(writer, "Header");

    addAttribute(writer, "creationTime", getCurrentTime(GMT));

    static std::string sHostname;
    if (sHostname.empty())
    {
      boost::system::error_code ec;
      sHostname = boost::asio::ip::host_name(ec);
      if (ec)
        sHostname = "localhost";
    }
    addAttribute(writer, "sender", sHostname);
    addAttribute(writer, "instanceId", to_string(instanceId));

    char version[32] = {0};
    sprintf(version, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR, AGENT_VERSION_PATCH,
            AGENT_VERSION_BUILD);
    addAttribute(writer, "version", version);

    int major, minor;
    char c;
    stringstream v(*m_schemaVersion);
    v >> major >> c >> minor;

    if (major > 1 || (major == 1 && minor >= 7))
    {
      addAttribute(writer, "deviceModelChangeTime", m_modelChangeTime);
    }

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

    if (major < 2 && aType == eDEVICES && count && !count->empty())
    {
      AutoElement ele(writer, "AssetCounts");

      for (const auto &pair : *count)
      {
        addSimpleElement(writer, "AssetCount", to_string(pair.second), {{"assetType", pair.first}});
      }
    }
  }
}  // namespace mtconnect::printer
