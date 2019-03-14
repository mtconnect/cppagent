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

#include <set>
#include <libxml/xmlwriter.h>
#include "dlib/sockets.h"
#include "dlib/logger.h"
#include "version.h"

#include "xml_printer.hpp"
#include "composition.hpp"
#include "device.hpp"
#include "cutting_tool.hpp"
#include "sensor_configuration.hpp"

#define strfy(line) #line
#define THROW_IF_XML2_ERROR(expr) \
if ((expr) < 0) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }
#define THROW_IF_XML2_NULL(expr) \
if (!(expr)) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }

using namespace std;

namespace mtconnect {
  static dlib::logger g_logger("xml.printer");
    
  
  XmlPrinter::XmlPrinter(const string version)
    : m_schemaVersion(version)
  {
    if (m_schemaVersion.empty())
      m_schemaVersion = "1.5";
  }
  
  void XmlPrinter::addDevicesNamespace(
                                       const std::string &urn,
                                       const std::string &location,
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
  
  
  void XmlPrinter::addErrorNamespace(
                                     const std::string &urn,
                                     const std::string &location,
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
  
  
  void XmlPrinter::addStreamsNamespace(
                                       const std::string &urn,
                                       const std::string &location,
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
  
  
  void XmlPrinter::addAssetsNamespace(
                                      const std::string &urn,
                                      const std::string &location,
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
  
  inline void addAttribute(xmlTextWriterPtr writer, const char *key,
                           const std::string &value)
  {
    if (!value.empty())
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST key,
                                                    BAD_CAST value.c_str()));

  }
  
  string XmlPrinter::printError(
                                const unsigned int instanceId,
                                const unsigned int bufferSize,
                                const uint64_t nextSeq,
                                const string &errorCode,
                                const string &errorText ) const
  {
    xmlTextWriterPtr writer = nullptr;
    xmlBufferPtr buf = nullptr;
    string ret;
    
    try
    {
      THROW_IF_XML2_NULL(buf = xmlBufferCreate());
      THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
      
      initXmlDoc(
                 writer,
                 eERROR,
                 instanceId,
                 bufferSize,
                 0,
                 0,
                 nextSeq,
                 nextSeq - 1);
      
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Errors"));
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Error"));

      addAttribute(writer, "errorCode", errorCode);

      auto text = xmlEncodeEntitiesReentrant(nullptr, BAD_CAST errorText.c_str());
      THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
      xmlFree(text); text = nullptr;
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Error
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Errors
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectError
      THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
      
      // Cleanup
      xmlFreeTextWriter(writer); writer = nullptr;
      ret = (string)((char *) buf->content);
      xmlBufferFree(buf); buf = nullptr;
    }
    catch (string error)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printError: " << error;
    }
    catch (...)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printError: unknown error";
    }
    
    return ret;
  }
  
  
  string XmlPrinter::printProbe(
                                const unsigned int instanceId,
                                const unsigned int bufferSize,
                                const uint64_t nextSeq,
                                const unsigned int assetBufferSize,
                                const unsigned int assetCount,
                                const vector<Device *> &deviceList,
                                const std::map<std::string, int> *count ) const
  {
    xmlTextWriterPtr writer = nullptr;
    xmlBufferPtr buf = nullptr;
    string ret;
    
    try
    {
      THROW_IF_XML2_NULL(buf = xmlBufferCreate());
      THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
      
      initXmlDoc(
                 writer,
                 eDEVICES,
                 instanceId,
                 bufferSize,
                 assetBufferSize,
                 assetCount,
                 nextSeq,
                 0,
                 nextSeq - 1,
                 count);
      
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Devices"));
      
      for (const auto dev : deviceList)
      {
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Device"));
        printProbeHelper(writer, dev);
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Device
      }
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Devices
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectDevices
      THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
      
      xmlFreeTextWriter(writer); writer = nullptr;
      ret = (string)((char *) buf->content);
      xmlBufferFree(buf); buf = nullptr;
    }
    catch (string error)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printProbe: " << error;
    }
    catch (...)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printProbe: unknown error";
    }
    
    return ret;
  }
  
  void XmlPrinter::printSensorConfiguration(xmlTextWriterPtr writer, const SensorConfiguration* sensor) const
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "SensorConfiguration"));
    addSimpleElement(writer, "FirmwareVersion", sensor->getFirmwareVersion());
      
    auto &cal = sensor->getCalibration();
    addSimpleElement(writer, "FirmwareVersion", sensor->getFirmwareVersion());
    addSimpleElement(writer, "CalibrationDate", cal.m_date);
    addSimpleElement(writer, "NextCalibrationDate", cal.m_nextDate);
    addSimpleElement(writer, "CalibrationInitials", cal.m_initials);
    
    THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST sensor->getRest().c_str()));

    if (sensor->getChannels().size() > 0)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Channels"));

      for (const auto &channel : sensor->getChannels())
      {
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Channel"));
        addAttributes(writer, channel.getAttributes());
        auto &cal = channel.getCalibration();
        addSimpleElement(writer, "Description", channel.getDescription());
        addSimpleElement(writer, "CalibrationDate", cal.m_date);
        addSimpleElement(writer, "NextCalibrationDate", cal.m_nextDate);
        addSimpleElement(writer, "CalibrationInitials", cal.m_initials);
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Channel
      }
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Channels
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // SensorConfiguration
  }
  
  void XmlPrinter::printProbeHelper(xmlTextWriterPtr writer, Component *component)  const
  {
    addAttributes(writer, component->getAttributes());
    
    const auto &desc = component->getDescription();
    const auto &body = component->getDescriptionBody();
    
    if (desc.size() > 0 || !body.empty())
      addSimpleElement(writer, "Description", body, desc);
    
    if (component->getConfiguration())
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Configuration"));

      auto configuration = component->getConfiguration();
      if (typeid(*configuration) == typeid(ExtendedComponentConfiguration))
      {
        auto ext = static_cast<const ExtendedComponentConfiguration*>(configuration);
        printRawContent(writer, "Configuration", ext->getContent());
      } else if (typeid(*configuration) == typeid(SensorConfiguration)) {
        auto sensor = static_cast<const SensorConfiguration*>(configuration);
        printSensorConfiguration(writer, sensor);
      }
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Configuration
    }
    
    auto datum = component->getDataItems();
    
    if (datum.size() > 0)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DataItems"));
      
      for (const auto data : datum)
        printDataItem(writer, data);
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DataItems
    }
    
    const auto children = component->getChildren();
    
    if (children.size() > 0)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Components"));
      
      for (const auto &child : children)
      {
        xmlChar *name = nullptr;
        if (!child->getPrefix().empty())
        {
          const auto ns = m_devicesNamespaces.find(child->getPrefix());
          if (ns != m_devicesNamespaces.end())
            name = BAD_CAST child->getPrefixedClass().c_str();
        }
        
        if (!name)
          name = BAD_CAST child->getClass().c_str();
        
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, name));
        
        printProbeHelper(writer, child);
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Component
      }
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Components
    }
    
    if (component->getCompositions().size() > 0)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Compositions"));
      
      for (auto comp : component->getCompositions())
      {
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Composition"));
        addAttributes(writer, comp->getAttributes());
        const Composition::Description *desc = comp->getDescription();
        
        if (desc)
          addSimpleElement(writer, "Description", desc->getBody(), desc->getAttributes());
        
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Composition
      }
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Compositions
    }
    
    if (component->getReferences().size() > 0)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "References"));
      
      for (const auto &ref : component->getReferences())
      {
        if (m_schemaVersion >= "1.4")
        {
          if (ref.m_type == Component::Reference::DATA_ITEM)
          {
            addSimpleElement(writer, "DataItemRef", "", { { "idRef", ref.m_id }, { "name", ref.m_name } });
          }
          else if (ref.m_type == Component::Reference::COMPONENT)
          {
            addSimpleElement(writer, "ComponentRef", "", { { "idRef", ref.m_id }, { "name", ref.m_name } });
          }
        }
        else if (ref.m_type == Component::Reference::DATA_ITEM)
        {
          addSimpleElement(writer, "Reference", "", { { "dataItemId", ref.m_id }, { "name", ref.m_name } });
        }
      }
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // References
    }
  }
  
  
  void XmlPrinter::printDataItem(xmlTextWriterPtr writer, DataItem *dataItem)  const
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DataItem"));
    
    addAttributes(writer, dataItem->getAttributes());
    
    if (!dataItem->getSource().empty() ||
        !dataItem->getSourceDataItemId().empty() ||
        !dataItem->getSourceComponentId().empty() ||
        !dataItem->getSourceCompositionId().empty())
    {
      addSimpleElement(writer, "Source", dataItem->getSource(), {
        { "dataItemId", dataItem->getSourceDataItemId() },
        { "componentId", dataItem->getSourceComponentId() },
        { "compositionId", dataItem->getSourceCompositionId() }
      });
    }
    
    if (dataItem->hasConstraints())
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Constraints"));
      
      auto s = dataItem->getMaximum();
      
      if (!s.empty())
        addSimpleElement(writer, "Maximum", s);
      
      s = dataItem->getMinimum();
      
      if (!s.empty())
        addSimpleElement(writer, "Minimum", s);
      
      const auto &values = dataItem->getConstrainedValues();
      for (const auto &value : values)
        addSimpleElement(writer, "Value", value);
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Constraints
    }
    
    if (dataItem->hasMinimumDelta() || dataItem->hasMinimumPeriod())
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Filters"));
      
      if (dataItem->hasMinimumDelta())
      {
        map<string, string> attributes;
        auto value = floatToString(dataItem->getFilterValue());
        addSimpleElement(writer, "Filter", value, { { "type", "MINIMUM_DELTA" } });
      }
      
      if (dataItem->hasMinimumPeriod())
      {
        map<string, string> attributes;
        auto value = floatToString(dataItem->getFilterPeriod());
        attributes["type"] = "PERIOD";
        addSimpleElement(writer, "Filter", value, { { "type", "PERIOD" } });
      }
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Filters
    }
    
    if (dataItem->hasInitialValue())
      addSimpleElement(writer, "InitialValue", dataItem->getInitialValue());
    
    if (dataItem->hasResetTrigger())
      addSimpleElement(writer, "ResetTrigger", dataItem->getResetTrigger());
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DataItem
  }
  
  
  typedef bool (*EventComparer)(ComponentEventPtr &aE1, ComponentEventPtr &aE2);
  
  
  static bool EventCompare(ComponentEventPtr &aE1, ComponentEventPtr &aE2)
  {
    return aE1 < aE2;
  }
  
  
  string XmlPrinter::printSample(
                                 const unsigned int instanceId,
                                 const unsigned int bufferSize,
                                 const uint64_t nextSeq,
                                 const uint64_t firstSeq,
                                 const uint64_t lastSeq,
                                 ComponentEventPtrArray &results )  const
  {
    xmlTextWriterPtr writer(nullptr);
    xmlBufferPtr buf(nullptr);
    string ret;
    
    try
    {
      THROW_IF_XML2_NULL(buf = xmlBufferCreate());
      THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
      
      initXmlDoc(
                 writer,
                 eSTREAMS,
                 instanceId,
                 bufferSize,
                 0,
                 0,
                 nextSeq,
                 firstSeq,
                 lastSeq);
      
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Streams"));
      
      // Sort the vector by category.
      if (results.size() > 1)
        dlib::qsort_array<ComponentEventPtrArray, EventComparer>(results, 0ul, results.size() - 1ul,
                                                                 EventCompare);
      
      Device *lastDevice = nullptr;
      Component *lastComponent = nullptr;
      int lastCategory = -1;
      
      for (auto i = 0ul; i < results.size(); i++)
      {
        auto result = results[i];
        const auto dataItem = result->getDataItem();
        const auto component = dataItem->getComponent();
        const auto device = component->getDevice();
        
        if (device != lastDevice)
        {
          if (lastDevice)
            THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DeviceStream
          
          lastDevice = device;
          
          if (lastComponent)
            THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // ComponentStream
          
          lastComponent = nullptr;
          
          if (lastCategory != -1)
            THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
          
          lastCategory = -1;
          addDeviceStream(writer, device);
        }
        
        if (component != lastComponent)
        {
          if (lastComponent)
            THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // ComponentStream
          
          lastComponent = component;
          
          if (lastCategory != -1)
            THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
          
          lastCategory = -1;
          addComponentStream(writer, component);
        }
        
        if (lastCategory != dataItem->getCategory())
        {
          if (lastCategory != -1)
            THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
          
          lastCategory = dataItem->getCategory();
          addCategory(writer, dataItem->getCategory());
        }
        
        addEvent(writer, result);
      }
      
      if (lastCategory != -1)
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
      
      if (lastDevice)
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DeviceStream
      
      if (lastComponent)
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // ComponentStream
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Streams
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectStreams
      THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
      
      xmlFreeTextWriter(writer); writer = nullptr;
      ret = (string)((char *) buf->content);
      xmlBufferFree(buf); buf = nullptr;
    }
    catch (string error)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printProbe: " << error;
    }
    catch (...)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printProbe: unknown error";
    }
    
    return ret;
  }
  
  
  string XmlPrinter::printAssets(
                                 const unsigned int instanceId,
                                 const unsigned int bufferSize,
                                 const unsigned int assetCount,
                                 std::vector<AssetPtr> const &assets)  const
  {
    xmlTextWriterPtr writer = nullptr;
    xmlBufferPtr buf = nullptr;
    string ret;
    
    try
    {
      THROW_IF_XML2_NULL(buf = xmlBufferCreate());
      THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
      
      initXmlDoc(writer, eASSETS, instanceId, 0u, bufferSize, assetCount, 0ull);
      
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Assets"));
      
      for (const auto asset : assets)
      {
        if (asset->getType() == "CuttingTool" || asset->getType() == "CuttingToolArchetype")
        {
          THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST asset->getContent(this).c_str()));
        }
        else
        {
          printAssetNode(writer, asset);
          THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST asset->getContent(this).c_str()));
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
        }
      }
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Assets
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectAssets
      
      xmlFreeTextWriter(writer); writer = nullptr;
      ret = (string)((char *) buf->content);
      xmlBufferFree(buf); buf = nullptr;
    }
    catch (string error)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printProbe: " << error;
    }
    catch (...)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printProbe: unknown error";
    }
    
    return ret;
  }
  
  
  void XmlPrinter::printAssetNode(xmlTextWriterPtr writer, Asset *asset)  const
  {
    // TODO: Check if cutting tool or archetype - should be in type
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST asset->getType().c_str()));
    
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
  
  
  void XmlPrinter::addDeviceStream(xmlTextWriterPtr writer, const Device *device)  const
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DeviceStream"));
    addAttribute(writer, "name", device->getName());
    addAttribute(writer, "uuid", device->getUuid());
  }
  
  
  void XmlPrinter::addComponentStream(xmlTextWriterPtr writer, const Component *component)  const
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "ComponentStream"));
    addAttribute(writer, "component", component->getClass());
    addAttribute(writer, "name", component->getName());
    addAttribute(writer, "componentId", component->getId());
  }
  
  
  void XmlPrinter::addCategory(xmlTextWriterPtr writer, DataItem::ECategory category)  const
  {
    switch (category)
    {
      case DataItem::SAMPLE:
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Samples"));
        break;
        
      case DataItem::EVENT:
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Events"));
        break;
        
      case DataItem::CONDITION:
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Condition"));
        break;
    }
    
  }
  
  
  void XmlPrinter::addEvent(xmlTextWriterPtr writer, ComponentEvent *result)  const
  {
    auto dataItem = result->getDataItem();
    
    if (dataItem->isCondition())
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST result->getLevelString().c_str()));
    }
    else
    {
      xmlChar *element = nullptr;
      
      if (!dataItem->getPrefix().empty())
      {
        auto ns = m_streamsNamespaces.find(dataItem->getPrefix());
        if (ns != m_streamsNamespaces.end())
          element = BAD_CAST dataItem->getPrefixedElementName().c_str();
      }
      
      if (!element)
        element = BAD_CAST dataItem->getElementName().c_str();
      
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, element));
    }
    
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
      ostringstream ostr;
      const DataSet &set = result->getDataSet();
      for (auto &e : set)
        ostr << e.first << '=' << e.second << ' ';
      
      string str = ostr.str();
      THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST str.c_str()));
    }
    else if (!result->getValue().empty())
    {
      auto text = xmlEncodeEntitiesReentrant(nullptr, BAD_CAST result->getValue().c_str());
      THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
      xmlFree(text); text = nullptr;
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Streams
  }
  
  
  void XmlPrinter::addAttributes(xmlTextWriterPtr writer, const std::map<string, string> &attributes)  const
  {
    for (const auto &attr : attributes)
    {
      if (!attr.second.empty()) {
        THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                        BAD_CAST attr.first.c_str(),
                                                      BAD_CAST attr.second.c_str()));
      }
    }
  }
  
  
  void XmlPrinter::addAttributes(xmlTextWriterPtr writer, const AttributeList &attributes) const
  {
    for (const auto & attr : attributes)
    {
      if (!attr.second.empty()) {
        THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST attr.first,
                                                        BAD_CAST attr.second.c_str()));
      }
    }
  }
  
  
  void XmlPrinter::initXmlDoc(
                              xmlTextWriterPtr writer,
                              EDocumentType aType,
                              const unsigned int instanceId,
                              const unsigned int bufferSize,
                              const unsigned int assetBufferSize,
                              const unsigned int assetCount,
                              const uint64_t nextSeq,
                              const uint64_t firstSeq,
                              const uint64_t lastSeq,
                              const map<string, int> *count ) const
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
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST rootName.c_str()));
    
    // Always make the default namespace and the m: namespace MTConnect default.
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                    BAD_CAST "xmlns:m",
                                                    BAD_CAST xmlns.c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                    BAD_CAST "xmlns",
                                                    BAD_CAST xmlns.c_str()));
    
    // Alwats add the xsi namespace
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                    BAD_CAST "xmlns:xsi",
                                                    BAD_CAST "http://www.w3.org/2001/XMLSchema-instance"));
    
    string mtcLocation;
    
    // Add in the other namespaces if they exist
    for (const auto ns : *namespaces)
    {
      // Skip the mtconnect ns (always m)
      if (ns.first != "m")
      {
        string attr = "xmlns:" + ns.first;
        THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                        BAD_CAST attr.c_str(),
                                                        BAD_CAST ns.second.mUrn.c_str()));
        
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
      location = xmlns + " http://schemas.mtconnect.org/schemas/" + rootName + "_" + m_schemaVersion + ".xsd";
    
    
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                    BAD_CAST "xsi:schemaLocation",
                                                    BAD_CAST location.c_str()));
    
    
    // Create the header
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Header"));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "creationTime",
                                                    BAD_CAST getCurrentTime(GMT).c_str()));
    
    static std::string sHostname;
    
    if (sHostname.empty())
    {
      if (dlib::get_local_hostname(sHostname))
        sHostname = "localhost";
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "sender",
                                                    BAD_CAST sHostname.c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "instanceId",
                                                    BAD_CAST intToString(instanceId).c_str()));
    char version[32] = {0};
    sprintf(version, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR, AGENT_VERSION_PATCH,
            AGENT_VERSION_BUILD);
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST version));
    
    if (aType == eASSETS || aType == eDEVICES)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "assetBufferSize",
                                                      BAD_CAST intToString(assetBufferSize).c_str()));
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "assetCount",
                                                      BAD_CAST int64ToString(assetCount).c_str()));
    }
    
    if (aType == eDEVICES || aType == eERROR || aType == eSTREAMS)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "bufferSize",
                                                      BAD_CAST intToString(bufferSize).c_str()));
    }
    
    if (aType == eSTREAMS)
    {
      // Add additional attribtues for streams
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "nextSequence",
                                                      BAD_CAST int64ToString(nextSeq).c_str()));
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST  "firstSequence",
                                                      BAD_CAST int64ToString(firstSeq).c_str()));
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "lastSequence",
                                                      BAD_CAST int64ToString(lastSeq).c_str()));
    }
    
    if (aType == eDEVICES && count && count->size() > 0)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "AssetCounts"));
      
      for (const auto &pair : *count)
      {
        addSimpleElement(writer, "AssetCount", intToString(pair.second),
                         { { "assetType", pair.first } });
        
      }
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
  }
  
  
  void XmlPrinter::addSimpleElement(
                                    xmlTextWriterPtr writer,
                                    const string &element,
                                    const string &body,
                                    const map<string, string> &attributes) const
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST element.c_str()));
    
    if (attributes.size() > 0)
      addAttributes(writer, attributes);
    
    if (!body.empty())
    {
      auto text = xmlEncodeEntitiesReentrant(nullptr, BAD_CAST body.c_str());
      THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
      xmlFree(text); text = nullptr;
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Element
  }
  
  
  // Cutting tools
  void XmlPrinter::printCuttingToolValue(xmlTextWriterPtr writer, CuttingToolValuePtr value) const
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST value->m_key.c_str()));
    
    addAttributes(writer, value->m_properties);
    
    THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST value->m_value.c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
  }
  
  
  void XmlPrinter::printCuttingToolValue(
                                         xmlTextWriterPtr writer,
                                         CuttingToolPtr tool,
                                         const char *value,
                                         std::set<string> *remaining) const
  {
    if (tool->m_values.count(value) > 0)
    {
      if (remaining)
        remaining->erase(value);
      
      auto ptr = tool->m_values[value];
      printCuttingToolValue(writer, ptr);
    }
  }
  
  
  void XmlPrinter::printCuttingToolValue(
                                         xmlTextWriterPtr writer,
                                         CuttingItemPtr item,
                                         const char *value,
                                         std::set<string> *remaining) const
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
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "CuttingItem"));
    
    for (const auto pair : item->m_identity)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                      BAD_CAST pair.first.c_str(),
                                                      BAD_CAST pair.second.c_str()));
    }
    
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
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Measurements"));
      
      for (const auto &meas : item->m_measurements)
        printCuttingToolValue(writer, meas.second);
      
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
    }
    
    // CuttingItem
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
  }
  
  
  string XmlPrinter::printCuttingTool(CuttingToolPtr const tool) const
  {
    
    xmlTextWriterPtr writer = nullptr;
    xmlBufferPtr buf = nullptr;
    string ret;
    
    try
    {
      THROW_IF_XML2_NULL(buf = xmlBufferCreate());
      THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
      THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
      
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
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "CuttingToolLifeCycle"));
      
      // Status...
      if (tool->m_status.size() > 0)
      {
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "CutterStatus"));
        
        for (const auto &status : tool->m_status)
        {
          THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Status"));
          THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST status.c_str()));
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
        }
        
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
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
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Measurements"));
        
        for (const auto &meas : tool->m_measurements)
          printCuttingToolValue(writer, meas.second);
        
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
      }
      
      // Print Cutting Items
      if (tool->m_items.size() > 0)
      {
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "CuttingItems"));
        addAttribute(writer, "count", tool->m_itemCount);
        
        for (const auto &item : tool->m_items)
          printCuttingToolItem(writer, item);
        
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
      }
      
      // CuttingToolLifeCycle
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
      
      // CuttingTool
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
      
      xmlFreeTextWriter(writer); writer = nullptr;
      ret = (string)((char *) buf->content);
      xmlBufferFree(buf); buf = nullptr;
    }
    catch (string error)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printCuttingTool: " << error;
    }
    catch (...)
    {
      if (buf)
      {
        xmlBufferFree(buf);
        buf = nullptr;
      }
      
      if (writer)
      {
        xmlFreeTextWriter(writer);
        writer = nullptr;
      }
      
      g_logger << dlib::LERROR << "printCuttingTool: unknown error";
    }
    
    return ret;
  }
}
