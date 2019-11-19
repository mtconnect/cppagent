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

#include "xml_printer.hpp"

#include "composition.hpp"
#include "cutting_tool.hpp"
#include "device.hpp"
#include "sensor_configuration.hpp"
#include "version.h"

#include <dlib/logger.h>
#include <dlib/sockets.h>

#include <libxml/xmlwriter.h>

#include <set>

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

    operator xmlTextWriterPtr()
    {
      return m_writer;
    }

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
      m_schemaVersion = "1.5";
  }

  void XmlPrinter::addDevicesNamespace(const std::string &urn, const std::string &location,
                                       const std::string &prefix)
  {
    pair<string, SchemaNamespace> item;
    item.second.mUrn = urn;
    item.second.mSchemaLocation = location;
    item.first = prefix;

    m_devicesNamespaces.insert(item);
  }

  void XmlPrinter::clearDevicesNamespaces()
  {
    m_devicesNamespaces.clear();
  }

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

    m_errorNamespaces.insert(item);
  }

  void XmlPrinter::clearErrorNamespaces()
  {
    m_errorNamespaces.clear();
  }

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

    m_streamsNamespaces.insert(item);
  }

  void XmlPrinter::clearStreamsNamespaces()
  {
    m_streamsNamespaces.clear();
  }

  void XmlPrinter::setSchemaVersion(const std::string &version)
  {
    m_schemaVersion = version;
  }

  const std::string &XmlPrinter::getSchemaVersion()
  {
    return m_schemaVersion;
  }

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

    m_assetsNamespaces.insert(item);
  }

  void XmlPrinter::clearAssetsNamespaces()
  {
    m_assetsNamespaces.clear();
  }

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

  void XmlPrinter::setStreamStyle(const std::string &style)
  {
    m_streamsStyle = style;
  }

  void XmlPrinter::setDevicesStyle(const std::string &style)
  {
    m_devicesStyle = style;
  }

  void XmlPrinter::setErrorStyle(const std::string &style)
  {
    m_errorStyle = style;
  }

  void XmlPrinter::setAssetsStyle(const std::string &style)
  {
    m_assetsStyle = style;
  }

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
    AutoElement(xmlTextWriterPtr writer) : m_writer(writer)
    {
    }
    AutoElement(xmlTextWriterPtr writer, const char *name, const string &key = "")
        : m_writer(writer), m_name(name), m_key(key)
    {
      openElement(writer, name);
    }
    AutoElement(xmlTextWriterPtr writer, const string &name, const string &key = "")
        : m_writer(writer), m_name(name), m_key(key)
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
        closeElement(m_writer);
    }

    const string &key() const
    {
      return m_key;
    }
    const string &name() const
    {
      return m_name;
    }

   protected:
    xmlTextWriterPtr m_writer;
    string m_name;
    string m_key;
  };

  string XmlPrinter::printError(const unsigned int instanceId, const unsigned int bufferSize,
                                const uint64_t nextSeq, const string &errorCode,
                                const string &errorText) const
  {
    string ret;

    try
    {
      XmlWriter writer(m_pretty);

      initXmlDoc(writer, eERROR, instanceId, bufferSize, 0, 0, nextSeq, nextSeq - 1);

      {
        AutoElement e1(writer, "Errors");
        addSimpleElement(writer, "Error", errorText, {{"errorCode", errorCode}});
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
                                const unsigned int assetCount, const vector<Device *> &deviceList,
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
          printProbeHelper(writer, dev, "Device");
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

  void XmlPrinter::printSensorConfiguration(xmlTextWriterPtr writer,
                                            const SensorConfiguration *sensor) const
  {
    AutoElement sensorEle(writer, "SensorConfiguration");

    addSimpleElement(writer, "FirmwareVersion", sensor->getFirmwareVersion());

    auto &cal = sensor->getCalibration();
    addSimpleElement(writer, "FirmwareVersion", sensor->getFirmwareVersion());
    addSimpleElement(writer, "CalibrationDate", cal.m_date);
    addSimpleElement(writer, "NextCalibrationDate", cal.m_nextDate);
    addSimpleElement(writer, "CalibrationInitials", cal.m_initials);

    THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST sensor->getRest().c_str()));

    if (sensor->getChannels().size() > 0)
    {
      AutoElement channelsEle(writer, "Channels");
      for (const auto &channel : sensor->getChannels())
      {
        AutoElement channelEle(writer, "Channel");
        addAttributes(writer, channel.getAttributes());
        auto &cal = channel.getCalibration();
        addSimpleElement(writer, "Description", channel.getDescription());
        addSimpleElement(writer, "CalibrationDate", cal.m_date);
        addSimpleElement(writer, "NextCalibrationDate", cal.m_nextDate);
        addSimpleElement(writer, "CalibrationInitials", cal.m_initials);
      }
    }
  }

  void XmlPrinter::printProbeHelper(xmlTextWriterPtr writer, Component *component,
                                    const char *name) const
  {
    AutoElement ele(writer, name);
    addAttributes(writer, component->getAttributes());

    const auto &desc = component->getDescription();
    const auto &body = component->getDescriptionBody();

    if (desc.size() > 0 || !body.empty())
      addSimpleElement(writer, "Description", body, desc);

    if (component->getConfiguration())
    {
      AutoElement configEle(writer, "Configuration");
      auto configuration = component->getConfiguration();
      if (typeid(*configuration) == typeid(ExtendedComponentConfiguration))
      {
        auto ext = static_cast<const ExtendedComponentConfiguration *>(configuration);
        printRawContent(writer, "Configuration", ext->getContent());
      }
      else if (typeid(*configuration) == typeid(SensorConfiguration))
      {
        auto sensor = static_cast<const SensorConfiguration *>(configuration);
        printSensorConfiguration(writer, sensor);
      }
    }

    auto datum = component->getDataItems();

    if (datum.size() > 0)
    {
      AutoElement ele(writer, "DataItems");

      for (const auto data : datum)
        printDataItem(writer, data);
    }

    const auto children = component->getChildren();

    if (children.size() > 0)
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

    if (component->getCompositions().size() > 0)
    {
      AutoElement ele(writer, "Compositions");

      for (auto comp : component->getCompositions())
      {
        AutoElement ele2(writer, "Composition");

        addAttributes(writer, comp->getAttributes());
        const Composition::Description *desc = comp->getDescription();

        if (desc)
          addSimpleElement(writer, "Description", desc->getBody(), desc->getAttributes());
      }
    }

    if (component->getReferences().size() > 0)
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
        auto value = floatToString(dataItem->getFilterValue());
        addSimpleElement(writer, "Filter", value, {{"type", "MINIMUM_DELTA"}});
      }

      if (dataItem->hasMinimumPeriod())
      {
        map<string, string> attributes;
        auto value = floatToString(dataItem->getFilterPeriod());
        attributes["type"] = "PERIOD";
        addSimpleElement(writer, "Filter", value, {{"type", "PERIOD"}});
      }
    }

    if (dataItem->hasInitialValue())
      addSimpleElement(writer, "InitialValue", dataItem->getInitialValue());

    if (dataItem->hasResetTrigger())
      addSimpleElement(writer, "ResetTrigger", dataItem->getResetTrigger());
  }

  string XmlPrinter::printSample(const unsigned int instanceId, const unsigned int bufferSize,
                                 const uint64_t nextSeq, const uint64_t firstSeq,
                                 const uint64_t lastSeq, ObservationPtrArray &observations) const
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
        dlib::qsort_array<ObservationPtrArray, ObservationComparer>(
            observations, 0ul, observations.size() - 1ul, ObservationCompare);

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

              addEvent(writer, observation);
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
                                 const unsigned int assetCount,
                                 std::vector<AssetPtr> const &assets) const
  {
    string ret;

    try
    {
      XmlWriter writer(m_pretty);
      initXmlDoc(writer, eASSETS, instanceId, 0u, bufferSize, assetCount, 0ull);

      {
        AutoElement ele(writer, "Assets");

        for (const auto asset : assets)
        {
          if (asset->getType() == "CuttingTool" || asset->getType() == "CuttingToolArchetype")
          {
            THROW_IF_XML2_ERROR(
                xmlTextWriterWriteRaw(writer, BAD_CAST asset->getContent(this).c_str()));
          }
          else
          {
            AutoElement ele(writer, asset->getType());
            printAssetNode(writer, asset);
            THROW_IF_XML2_ERROR(
                xmlTextWriterWriteRaw(writer, BAD_CAST asset->getContent(this).c_str()));
          }
        }
      }
      closeElement(writer);  // MTConnectAssets

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

  void XmlPrinter::printAssetNode(xmlTextWriterPtr writer, Asset *asset) const
  {
    addAttributes(writer, asset->getIdentity());

    // Add the timestamp and device uuid fields.
    addAttribute(writer, "timestamp", asset->getTimestamp());
    addAttribute(writer, "deviceUuid", asset->getDeviceUuid());
    addAttribute(writer, "assetId", asset->getAssetId());

    if (asset->isRemoved())
      addAttribute(writer, "removed", "true");

    if (!asset->getArchetype().empty())
      addSimpleElement(writer, "AssetArchetypeRef", "", asset->getArchetype());

    if (!asset->getDescription().empty())
    {
      const auto &body = asset->getDescription();
      addSimpleElement(writer, "Description", body);
    }
  }

  void XmlPrinter::addEvent(xmlTextWriterPtr writer, Observation *result) const
  {
    auto dataItem = result->getDataItem();
    string name;

    if (dataItem->isCondition())
    {
      name = result->getLevelString();
    }
    else
    {
      if (!dataItem->getPrefix().empty())
      {
        auto ns = m_streamsNamespaces.find(dataItem->getPrefix());
        if (ns != m_streamsNamespaces.end())
          name = dataItem->getPrefixedElementName();
      }

      if (name.empty())
        name = dataItem->getElementName();
    }

    AutoElement ele(writer, name);
    addAttributes(writer, result->getAttributes());

    if (result->isTimeSeries() && result->getValue() != "UNAVAILABLE")
    {
      ostringstream ostr;
      ostr.precision(6);
      const auto &v = result->getTimeSeries();

      for (auto &e : v)
        ostr << e << ' ';

      string str = ostr.str();
      THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST str.c_str()));
    }
    else if (result->isDataSet() && result->getValue() != "UNAVAILABLE")
    {
      const DataSet &set = result->getDataSet();
      for (auto &e : set)
      {
        map<string, string> attrs = {{"key", e.m_key}};
        if (e.m_removed)
        {
          attrs["removed"] = "true";
        }
        addSimpleElement(writer, "Entry", e.m_value, attrs);
      }
    }
    else if (!result->getValue().empty())
    {
      auto text = xmlEncodeEntitiesReentrant(nullptr, BAD_CAST result->getValue().c_str());
      THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
      xmlFree(text);
      text = nullptr;
    }
  }

  void XmlPrinter::addAttributes(xmlTextWriterPtr writer,
                                 const std::map<string, string> &attributes) const
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

  void XmlPrinter::addAttributes(xmlTextWriterPtr writer, const AttributeList &attributes) const
  {
    for (const auto &attr : attributes)
    {
      if (!attr.second.empty())
      {
        THROW_IF_XML2_ERROR(
            xmlTextWriterWriteAttribute(writer, BAD_CAST attr.first, BAD_CAST attr.second.c_str()));
      }
    }
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
      string pi = "xml-stylesheet type=\"text/xsl\" href=\"" + style + '"';
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
    for (const auto ns : *namespaces)
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
    addAttribute(writer, "instanceId", intToString(instanceId));

    char version[32] = {0};
    sprintf(version, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR, AGENT_VERSION_PATCH,
            AGENT_VERSION_BUILD);
    addAttribute(writer, "version", version);

    if (aType == eASSETS || aType == eDEVICES)
    {
      addAttribute(writer, "assetBufferSize", intToString(assetBufferSize));
      addAttribute(writer, "assetCount", intToString(assetCount));
    }

    if (aType == eDEVICES || aType == eERROR || aType == eSTREAMS)
    {
      addAttribute(writer, "bufferSize", intToString(bufferSize));
    }

    if (aType == eSTREAMS)
    {
      // Add additional attribtues for streams
      addAttribute(writer, "nextSequence", int64ToString(nextSeq));
      addAttribute(writer, "firstSequence", int64ToString(firstSeq));
      addAttribute(writer, "lastSequence", int64ToString(lastSeq));
    }

    if (aType == eDEVICES && count && count->size() > 0)
    {
      AutoElement ele(writer, "AssetCounts");

      for (const auto &pair : *count)
      {
        addSimpleElement(writer, "AssetCount", intToString(pair.second),
                         {{"assetType", pair.first}});
      }
    }
  }

  void XmlPrinter::addSimpleElement(xmlTextWriterPtr writer, const string &element,
                                    const string &body, const map<string, string> &attributes,
                                    bool raw) const
  {
    AutoElement ele(writer, element);

    if (attributes.size() > 0)
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

  // Cutting tools
  void XmlPrinter::printCuttingToolValue(xmlTextWriterPtr writer, CuttingToolValuePtr value) const
  {
    addSimpleElement(writer, value->m_key, value->m_value, value->m_properties, true);
  }

  void XmlPrinter::printCuttingToolValue(xmlTextWriterPtr writer, CuttingToolPtr tool,
                                         const char *value, std::set<string> *remaining) const
  {
    if (tool->m_values.count(value) > 0)
    {
      if (remaining)
        remaining->erase(value);

      auto ptr = tool->m_values[value];
      printCuttingToolValue(writer, ptr);
    }
  }

  void XmlPrinter::printCuttingToolValue(xmlTextWriterPtr writer, CuttingItemPtr item,
                                         const char *value, std::set<string> *remaining) const
  {
    if (item->m_values.count(value) > 0)
    {
      if (remaining)
        remaining->erase(value);

      auto ptr = item->m_values[value];
      printCuttingToolValue(writer, ptr);
    }
  }

  void XmlPrinter::printCuttingToolItem(xmlTextWriterPtr writer, CuttingItemPtr item) const
  {
    AutoElement ele(writer, "CuttingItem");
    addAttributes(writer, item->m_identity);

    set<string> remaining;
    for (const auto &value : item->m_values)
      remaining.insert(value.first);

    printCuttingToolValue(writer, item, "Description", &remaining);
    printCuttingToolValue(writer, item, "Locus", &remaining);

    for (const auto &life : item->m_lives)
      printCuttingToolValue(writer, life);

    // Print extended items...
    for (const auto &prop : remaining)
      printCuttingToolValue(writer, item, prop.c_str());

    // Print Measurements
    if (item->m_measurements.size() > 0)
    {
      AutoElement ele(writer, "Measurements");

      for (const auto &meas : item->m_measurements)
        printCuttingToolValue(writer, meas.second);
    }
  }

  string XmlPrinter::printCuttingTool(CuttingToolPtr const tool) const
  {
    string ret;

    try
    {
      XmlWriter writer(m_pretty);

      {
        AutoElement ele(writer, tool->getType());
        printAssetNode(writer, tool);

        set<string> remaining;

        for (const auto &value : tool->m_values)
        {
          if (value.first != "Description")
            remaining.insert(value.first);
        }

        // Check for cutting tool definition
        printCuttingToolValue(writer, tool, "CuttingToolDefinition", &remaining);

        // Print the cutting tool life cycle.
        {
          AutoElement life(writer, "CuttingToolLifeCycle");

          // Status...
          if (tool->m_status.size() > 0)
          {
            AutoElement stat(writer, "CutterStatus");
            for (const auto &status : tool->m_status)
              addSimpleElement(writer, "Status", status);
          }

          // Other values
          printCuttingToolValue(writer, tool, "ReconditionCount", &remaining);

          // Tool life
          for (const auto &life : tool->m_lives)
            printCuttingToolValue(writer, life);

          // Remaining items
          printCuttingToolValue(writer, tool, "ProgramToolGroup", &remaining);
          printCuttingToolValue(writer, tool, "ProgramToolNumber", &remaining);
          printCuttingToolValue(writer, tool, "Location", &remaining);
          printCuttingToolValue(writer, tool, "ProcessSpindleSpeed", &remaining);
          printCuttingToolValue(writer, tool, "ProcessFeedRate", &remaining);
          printCuttingToolValue(writer, tool, "ConnectionCodeMachineSide", &remaining);

          // Print extended items...
          for (const auto &prop : remaining)
            printCuttingToolValue(writer, tool, prop.c_str());

          // Print Measurements
          if (tool->m_measurements.size() > 0)
          {
            AutoElement mes(writer, "Measurements");
            for (const auto &meas : tool->m_measurements)
              printCuttingToolValue(writer, meas.second);
          }

          // Print Cutting Items
          if (tool->m_items.size() > 0)
          {
            AutoElement mes(writer, "CuttingItems");
            addAttribute(writer, "count", tool->m_itemCount);

            for (const auto &item : tool->m_items)
              printCuttingToolItem(writer, item);
          }
        }
      }

      ret = writer.getContent();
    }
    catch (string error)
    {
      g_logger << dlib::LERROR << "printCuttingTool: " << error;
    }
    catch (...)
    {
      g_logger << dlib::LERROR << "printCuttingTool: unknown error";
    }

    return ret;
  }
}  // namespace mtconnect
