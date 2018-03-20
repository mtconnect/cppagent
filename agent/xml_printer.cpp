//
// Copyright Copyright 2012, System Insights, Inc.
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
#include "dlib/sockets.h"
#include "dlib/logger.h"
#include "version.h"
#include <set>


static dlib::logger sLogger("xml.printer");

#define strfy(line) #line
#define THROW_IF_XML2_ERROR(expr) \
  if ((expr) < 0) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }
#define THROW_IF_XML2_NULL(expr) \
  if ((expr) == NULL) { throw string("XML Error at " __FILE__ "(" strfy(__LINE__) "): " #expr); }

using namespace std;

struct SchemaNamespace {
  string mUrn;
  string mSchemaLocation;
};

static map<string, SchemaNamespace> sDevicesNamespaces;
static map<string, SchemaNamespace> sStreamsNamespaces;
static map<string, SchemaNamespace> sErrorNamespaces;
static map<string, SchemaNamespace> sAssetsNamespaces;
static string sSchemaVersion("1.3");
static string mStreamsStyle;
static string mDevicesStyle;
static string mErrorStyle;
static string mAssetsStyle;

enum EDocumentType {
  eERROR,
  eSTREAMS,
  eDEVICES,
  eASSETS
};

namespace XmlPrinter {
  /***** Helper Methods *****/
  /* Initiate all documents */
  void initXmlDoc(xmlTextWriterPtr writer,
                  EDocumentType aDocType,
                  const unsigned int instanceId,
                  const unsigned int bufferSize,
                  const unsigned int aAssetBufferSize,
                  const unsigned int aAssetCount,
                  const uint64_t nextSeq,
                  const uint64_t firstSeq = 0,
                  const uint64_t lastSeq = 0,
                  const map<string, int> *aCounts = NULL);  

  /* Helper to print individual components and details */
  void printProbeHelper(xmlTextWriterPtr writer, Component *component);
  void printDataItem(xmlTextWriterPtr writer, DataItem *dataItem);


  /* Add attributes to an xml element */
  void addDeviceStream(xmlTextWriterPtr writer, Device *device);
  void addComponentStream(xmlTextWriterPtr writer, Component *component);
  void addCategory(xmlTextWriterPtr writer, DataItem::ECategory category);
  void addSimpleElement(xmlTextWriterPtr writer, std::string element, const std::string &body,
                        const std::map<std::string, std::string> *attributes = NULL);

  void addAttributes(xmlTextWriterPtr writer,
                     const std::map<std::string, std::string> *attributes);
  void addAttributes(xmlTextWriterPtr writer,
                     const std::map<std::string, std::string> &attributes) {
    addAttributes(writer, &attributes);
  }
  void addAttributes(xmlTextWriterPtr writer,
                     const AttributeList *attributes);

  void addEvent(xmlTextWriterPtr writer, ComponentEvent *result);
  
  // Asset printing
  void printCuttingToolValue(xmlTextWriterPtr writer, CuttingToolPtr aTool,
                             const char *aValue, std::set<string> *aRemaining = NULL);
  void printCuttingToolValue(xmlTextWriterPtr writer, CuttingItemPtr aItem,
                             const char *aValue, std::set<string> *aRemaining = NULL);
  void printCuttingToolValue(xmlTextWriterPtr writer, CuttingToolValuePtr aValue);
  void printCuttingToolItem(xmlTextWriterPtr writer, CuttingItemPtr aItem);
  void printAssetNode(xmlTextWriterPtr writer, Asset *anAsset);

};


void XmlPrinter::addDevicesNamespace(const std::string &aUrn, const std::string &aLocation, 
                         const std::string &aPrefix)
{
  pair<string, SchemaNamespace> item;
  item.second.mUrn = aUrn;
  item.second.mSchemaLocation = aLocation;
  item.first = aPrefix;
  
  sDevicesNamespaces.insert(item);
}

void XmlPrinter::clearDevicesNamespaces()
{
  sDevicesNamespaces.clear();
}

const string XmlPrinter::getDevicesUrn(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sDevicesNamespaces.find(aPrefix);
  if (ns != sDevicesNamespaces.end())
    return ns->second.mUrn;
  else
    return "";
}

const string XmlPrinter::getDevicesLocation(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sDevicesNamespaces.find(aPrefix);
  if (ns != sDevicesNamespaces.end())
    return ns->second.mSchemaLocation;
  else
    return "";
}


void XmlPrinter::addErrorNamespace(const std::string &aUrn, const std::string &aLocation, 
                       const std::string &aPrefix)
{
  pair<string, SchemaNamespace> item;
  item.second.mUrn = aUrn;
  item.second.mSchemaLocation = aLocation;
  item.first = aPrefix;
  
  sErrorNamespaces.insert(item);  
}

void XmlPrinter::clearErrorNamespaces()
{
  sErrorNamespaces.clear();
}

const string XmlPrinter::getErrorUrn(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sErrorNamespaces.find(aPrefix);
  if (ns != sErrorNamespaces.end())
    return ns->second.mUrn;
  else
    return "";
}

const string XmlPrinter::getErrorLocation(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sErrorNamespaces.find(aPrefix);
  if (ns != sErrorNamespaces.end())
    return ns->second.mSchemaLocation;
  else
    return "";
}

void XmlPrinter::addStreamsNamespace(const std::string &aUrn, const std::string &aLocation, 
                         const std::string &aPrefix)
{
  pair<string, SchemaNamespace> item;
  item.second.mUrn = aUrn;
  item.second.mSchemaLocation = aLocation;
  item.first = aPrefix;
  
  sStreamsNamespaces.insert(item);  
}

void XmlPrinter::clearStreamsNamespaces()
{
  sStreamsNamespaces.clear();
}

void XmlPrinter::setSchemaVersion(const std::string &aVersion)
{
  sSchemaVersion = aVersion;
}

const std::string &XmlPrinter::getSchemaVersion()
{
  return sSchemaVersion;
}

const string XmlPrinter::getStreamsUrn(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sStreamsNamespaces.find(aPrefix);
  if (ns != sStreamsNamespaces.end())
    return ns->second.mUrn;
  else
    return "";
}

const string XmlPrinter::getStreamsLocation(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sStreamsNamespaces.find(aPrefix);
  if (ns != sStreamsNamespaces.end())
    return ns->second.mSchemaLocation;
  else
    return "";
}

void XmlPrinter::addAssetsNamespace(const std::string &aUrn, const std::string &aLocation, 
                         const std::string &aPrefix)
{
  pair<string, SchemaNamespace> item;
  item.second.mUrn = aUrn;
  item.second.mSchemaLocation = aLocation;
  item.first = aPrefix;
  
  sAssetsNamespaces.insert(item);  
}

void XmlPrinter::clearAssetsNamespaces()
{
  sAssetsNamespaces.clear();
}

const string XmlPrinter::getAssetsUrn(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sAssetsNamespaces.find(aPrefix);
  if (ns != sAssetsNamespaces.end())
    return ns->second.mUrn;
  else
    return "";
}

const string XmlPrinter::getAssetsLocation(const std::string &aPrefix)
{
  map<string, SchemaNamespace>::iterator ns = sAssetsNamespaces.find(aPrefix);
  if (ns != sAssetsNamespaces.end())
    return ns->second.mSchemaLocation;
  else
    return "";
}

void XmlPrinter::setStreamStyle(const std::string &aStyle)
{
  mStreamsStyle = aStyle;
}

void XmlPrinter::setDevicesStyle(const std::string &aStyle)
{
  mDevicesStyle = aStyle;
}

void XmlPrinter::setErrorStyle(const std::string &aStyle)
{
  mErrorStyle = aStyle;
}

void XmlPrinter::setAssetsStyle(const std::string &aStyle)
{
  mAssetsStyle = aStyle;
}

/* XmlPrinter main methods */
string XmlPrinter::printError(const unsigned int instanceId,
                              const unsigned int bufferSize,
                              const uint64_t nextSeq,
                              const string& errorCode,
                              const string& errorText
  )
{
  xmlTextWriterPtr writer = NULL;
  xmlBufferPtr buf = NULL;
  string ret;
  
  try {
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
  
    initXmlDoc(writer, eERROR, instanceId,
               bufferSize, 0, 0, nextSeq, nextSeq - 1);
  
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Errors"));
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Error"));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "errorCode", 
                                                    BAD_CAST errorCode.c_str()));
    xmlChar *text = xmlEncodeEntitiesReentrant(NULL, BAD_CAST errorText.c_str());
    THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
    xmlFree(text);

    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Error
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Errors
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectError
    THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
    
    // Cleanup
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);
  }
  catch (string error) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printError: " << error;
  }
  catch (...) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printError: unknown error";
  }
  
  return ret;
}

string XmlPrinter::printProbe(const unsigned int instanceId,
                              const unsigned int bufferSize,
                              const uint64_t nextSeq,
                              const unsigned int aAssetBufferSize,
                              const unsigned int aAssetCount,
                              vector<Device *>& deviceList,
                              const std::map<std::string, int> *aCount)
{
  xmlTextWriterPtr writer = NULL;
  xmlBufferPtr buf = NULL;
  string ret;
  
  try {
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
    
    initXmlDoc(writer, eDEVICES,
                instanceId,
                bufferSize,
                aAssetBufferSize,
                aAssetCount,
                nextSeq, 0, nextSeq - 1,
                aCount);
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Devices"));
    
    
    vector<Device *>::iterator dev;
    for (dev = deviceList.begin(); dev != deviceList.end(); dev++)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Device"));
      printProbeHelper(writer, *dev);
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Device
    }      

    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Devices
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectDevices
    THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
    
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);    
  }
  catch (string error) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printProbe: " << error;
  }
  catch (...) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  
  return ret;
 }

void XmlPrinter::printProbeHelper(xmlTextWriterPtr writer,
                                  Component * component)
{
  addAttributes(writer, component->getAttributes());
  
  std::map<string, string> desc = component->getDescription();
  string body = component->getDescriptionBody();
  
  if (desc.size() > 0 || !body.empty())
  {
    addSimpleElement(writer, "Description", body, &desc);
  }
  
  if (!component->getConfiguration().empty()) {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Configuration"));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST component->getConfiguration().c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Configuration    
  }
  
  list<DataItem *> datum = component->getDataItems();
  if (datum.size() > 0)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DataItems"));
    
    list<DataItem *>::iterator data;
    for (data = datum.begin(); data!= datum.end(); data++)
    {
      printDataItem(writer, *data);
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DataItems    
  }
  
  list<Component *> children = component->getChildren();
  
  if (children.size() > 0)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Components"));
    
    list<Component *>::iterator child;
    for (child = children.begin(); child != children.end(); child++)
    {
      xmlChar *name = NULL;
      if (!(*child)->getPrefix().empty()) 
      {
        map<string, SchemaNamespace>::iterator ns = sDevicesNamespaces.find((*child)->getPrefix());
        if (ns != sDevicesNamespaces.end()) {
          name = BAD_CAST (*child)->getPrefixedClass().c_str();
        }
      }
      if (name == NULL) name = BAD_CAST (*child)->getClass().c_str();
      
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, name));
      
      printProbeHelper(writer, *child);
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Component 
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Components    
  }
  
  if (component->getCompositions().size() > 0)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Compositions"));

    for (auto comp : component->getCompositions() )
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Composition"));
      addAttributes(writer, comp->getAttributes());
      const Composition::Description *desc = comp->getDescription();
      if (desc != NULL)
      {
        addSimpleElement(writer, "Description", desc->getBody(), &desc->getAttributes());
      }
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Composition
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Compositions
  }
  
  if (component->getReferences().size() > 0)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "References"));
    
    list<Component::Reference>::const_iterator ref;
    for (ref = component->getReferences().begin(); ref != component->getReferences().end(); ref++)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Reference"));
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "dataItemId",
                                                      BAD_CAST ref->mId.c_str()));
      if (ref->mName.length() > 0) {
        THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "name",
                                                        BAD_CAST ref->mName.c_str()));
      }
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Reference
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // References
  }
}

void XmlPrinter::printDataItem(xmlTextWriterPtr writer, DataItem *dataItem)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DataItem"));

  addAttributes(writer, dataItem->getAttributes());
  string source = dataItem->getSource();
  if (!source.empty())
  {
    addSimpleElement(writer, "Source", source);
  }
  
  if (dataItem->hasConstraints())
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Constraints"));

    string s = dataItem->getMaximum();
    if (!s.empty())
    {
      addSimpleElement(writer, "Maximum", s);
    }
    
    s = dataItem->getMinimum();
    if (!s.empty())
    {
      addSimpleElement(writer, "Minimum", s);
    }
    
    vector<string> values = dataItem->getConstrainedValues();
    vector<string>::iterator value;
    for (value = values.begin(); value != values.end(); value++)
    {
      addSimpleElement(writer, "Value", *value);
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Constraints
  }
  
  if (dataItem->hasMinimumDelta() || dataItem->hasMinimumPeriod())
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Filters"));
    
    if (dataItem->hasMinimumDelta())
    {
      map<string, string> attributes;
      string value = floatToString(dataItem->getFilterValue());
      attributes["type"] = "MINIMUM_DELTA";
      addSimpleElement(writer, "Filter", value, &attributes);
    }

    if (dataItem->hasMinimumPeriod())
    {
      map<string, string> attributes;
      string value = floatToString(dataItem->getFilterPeriod());
      attributes["type"] = "PERIOD";
      addSimpleElement(writer, "Filter", value, &attributes);
    }
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Filters
  }
  
  if (dataItem->hasInitialValue())
  {
    addSimpleElement(writer, "InitialValue", dataItem->getInitialValue());
  }
  
  if (dataItem->hasResetTrigger())
  {
    addSimpleElement(writer, "ResetTrigger", dataItem->getResetTrigger());
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DataItem
}

typedef bool (*EventComparer)(ComponentEventPtr &aE1, ComponentEventPtr &aE2);

static bool EventCompare(ComponentEventPtr &aE1, ComponentEventPtr &aE2)
{
  return aE1 < aE2;
}

string XmlPrinter::printSample(const unsigned int instanceId,
                               const unsigned int bufferSize,
                               const uint64_t nextSeq,
                               const uint64_t firstSeq,
                               const uint64_t lastSeq,
                               ComponentEventPtrArray& results
  )
{
  xmlTextWriterPtr writer;
  xmlBufferPtr buf;
  string ret;
  
  try {
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
    
    initXmlDoc(writer, eSTREAMS,
               instanceId,
               bufferSize,
               0, 0,
               nextSeq,
               firstSeq,
               lastSeq);
        
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Streams"));
    
    // Sort the vector by category.
    if (results.size() > 1)
      dlib::qsort_array<ComponentEventPtrArray,EventComparer>(results, 0, results.size() - 1, EventCompare);

    Device *lastDevice = NULL;
    Component *lastComponent = NULL;
    int lastCategory = -1;
    
    for (unsigned int i = 0; i < results.size(); i++)
    {
      ComponentEventPtr result = results[i];
      DataItem *dataItem = result->getDataItem();
      Component *component = dataItem->getComponent();
      Device *device = component->getDevice();
      
      if (device != lastDevice)
      {
        if (lastDevice != NULL)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DeviceStream
        lastDevice = device;
        if (lastComponent != NULL)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // ComponentStream
        lastComponent = NULL;
        if (lastCategory != -1)
          THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Category
        lastCategory = -1;
        addDeviceStream(writer, device);
      }
      
      if (component != lastComponent)
      {
        if (lastComponent != NULL)
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
    if (lastDevice != NULL)
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // DeviceStream
    if (lastComponent != NULL)
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // ComponentStream
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Streams    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectStreams
    THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(writer));
    
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);    
  }
  catch (string error) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printProbe: " << error;
  }
  catch (...) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  
  return ret;
}

string XmlPrinter::printAssets(const unsigned int instanceId,
                               const unsigned int aBufferSize,
                               const unsigned int anAssetCount,
                               std::vector<AssetPtr> &anAssets)
{
  xmlTextWriterPtr writer = NULL;
  xmlBufferPtr buf = NULL;
  string ret;
  
  try {
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
    
    initXmlDoc(writer, eASSETS, instanceId, 0, aBufferSize, anAssetCount, 0);
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Assets"));

    vector<AssetPtr>::iterator iter;
    for (iter = anAssets.begin(); iter != anAssets.end(); ++iter)
    {
      if ((*iter)->getType() == "CuttingTool" || (*iter)->getType() == "CuttingToolArchetype") {
        THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST (*iter)->getContent().c_str()));
      } else {
        printAssetNode(writer, (*iter));
        THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST (*iter)->getContent().c_str()));        
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
      }
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Assets    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // MTConnectAssets
    
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);
  }
  catch (string error) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printProbe: " << error;
  }
  catch (...) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  
  return ret;
}

void XmlPrinter::printAssetNode(xmlTextWriterPtr writer, Asset *anAsset)
{
  // TODO: Check if cutting tool or archetype - should be in type
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST anAsset->getType().c_str()));
  
  addAttributes(writer, &anAsset->getIdentity());
  
  // Add the timestamp and device uuid fields.
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "timestamp",
                                                  BAD_CAST anAsset->getTimestamp().c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "deviceUuid",
                                                  BAD_CAST anAsset->getDeviceUuid().c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "assetId",
                                                  BAD_CAST anAsset->getAssetId().c_str()));
  if (anAsset->isRemoved())
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                    BAD_CAST "removed",
                                                    BAD_CAST "true"));
  }

  if (!anAsset->getArchetype().empty()) {
    addSimpleElement(writer, "AssetArchetypeRef", "", &anAsset->getArchetype());
  }

  if (!anAsset->getDescription().empty()) {
    string body = anAsset->getDescription();
    addSimpleElement(writer, "Description", body, NULL);
  }
  
}



void XmlPrinter::addDeviceStream(xmlTextWriterPtr writer, Device *device)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "DeviceStream"));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "name", 
                                                  BAD_CAST device->getName().c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "uuid", 
                                                  BAD_CAST device->getUuid().c_str()));
}

void XmlPrinter::addComponentStream(xmlTextWriterPtr writer, Component *component)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "ComponentStream"));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "component", 
                                                  BAD_CAST component->getClass().c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "name", 
                                                  BAD_CAST component->getName().c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "componentId", 
                                                  BAD_CAST component->getId().c_str()));
}

void XmlPrinter::addCategory(xmlTextWriterPtr writer, DataItem::ECategory category)
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

void XmlPrinter::addEvent(xmlTextWriterPtr writer, ComponentEvent *result)
{
  DataItem *dataItem = result->getDataItem();
  if (dataItem->isCondition()) {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST result->getLevelString().c_str()));
  } else {
    xmlChar *element = NULL;
    if (!dataItem->getPrefix().empty()) 
    {
      map<string, SchemaNamespace>::iterator ns = sStreamsNamespaces.find(dataItem->getPrefix());
      if (ns != sStreamsNamespaces.end()) {
        element = BAD_CAST dataItem->getPrefixedElementName().c_str();
      }
    }
    if (element == NULL) element = BAD_CAST dataItem->getElementName().c_str();
    
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, element));
  }
  
  addAttributes(writer, result->getAttributes());

  if (result->isTimeSeries() && result->getValue() != "UNAVAILABLE") {
    ostringstream ostr;
    ostr.precision(6);
    const vector<float> &v = result->getTimeSeries();
    for (size_t i = 0; i < v.size(); i++)
      ostr << v[i] << ' ';
    string str = ostr.str();
    THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST str.c_str()));
  } else if (!result->getValue().empty()) {
    xmlChar *text = xmlEncodeEntitiesReentrant(NULL, BAD_CAST result->getValue().c_str());
    THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
    xmlFree(text);
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Streams    
}

void XmlPrinter::addAttributes(xmlTextWriterPtr writer,
                               const std::map<string, string> *attributes)
{
  std::map<string, string>::iterator attr;
  for (auto attr : *attributes)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST attr.first.c_str(), 
                                                    BAD_CAST attr.second.c_str()));

  }
}

void XmlPrinter::addAttributes(xmlTextWriterPtr writer,
                               const AttributeList *attributes)
{
  AttributeList::iterator attr;
  for (auto attr : *attributes)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST attr.first, 
                                                    BAD_CAST attr.second.c_str()));
    
  }
}

/* XmlPrinter helper Methods */
void XmlPrinter::initXmlDoc(xmlTextWriterPtr writer, 
                            EDocumentType aType,
                            const unsigned int instanceId,
                            const unsigned int bufferSize,
                            const unsigned int aAssetBufferSize,
                            const unsigned int aAssetCount,
                            const uint64_t nextSeq,
                            const uint64_t firstSeq,
                            const uint64_t lastSeq,
                            const map<string, int> *aCount
                            )
{  
  THROW_IF_XML2_ERROR(xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL));
 
  // TODO: Cache the locations and header attributes.
  // Write the root element  
  string xmlType, style;
  map<string, SchemaNamespace> *namespaces;
  switch (aType) {
  case eERROR:
      namespaces = &sErrorNamespaces;
      style = mErrorStyle;
      xmlType = "Error";
      break;
      
  case eSTREAMS:
      namespaces = &sStreamsNamespaces;
      style = mStreamsStyle;
      xmlType = "Streams";
      break;
      
  case eDEVICES:
      namespaces = &sDevicesNamespaces;
      style = mDevicesStyle;
      xmlType = "Devices";
      break;
      
    case eASSETS:
      namespaces = &sAssetsNamespaces;
      style = mAssetsStyle;
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
  string xmlns = "urn:mtconnect.org:" + rootName + ":" + sSchemaVersion;  
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
  map<string, SchemaNamespace>::iterator ns;
  for (ns = namespaces->begin(); ns != namespaces->end(); ns++)
  {
    // Skip the mtconnect ns (always m)
    if (ns->first != "m")
    {
      string attr = "xmlns:" + ns->first;
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                      BAD_CAST attr.c_str(),
                                                      BAD_CAST ns->second.mUrn.c_str()));
      
      if (location.empty() && !ns->second.mSchemaLocation.empty())
      {
        // Always take the first location. There should only be one location!
        location = ns->second.mUrn + " " + ns->second.mSchemaLocation;
      }
    } else if (!ns->second.mSchemaLocation.empty()) {
      // This is the mtconnect namespace
      mtcLocation = xmlns + " " + ns->second.mSchemaLocation;
    }    
  }
  
  // Write the schema location
  if (location.empty() && !mtcLocation.empty()) {
    location = mtcLocation;
  } else if (location.empty()) {
    location = xmlns + " http://schemas.mtconnect.org/schemas/" + rootName + "_" + sSchemaVersion + ".xsd";
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                  BAD_CAST "xsi:schemaLocation",
                                                  BAD_CAST location.c_str()));
  
  
  // Create the header
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Header"));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "creationTime", BAD_CAST getCurrentTime(GMT).c_str()));
  
  static std::string sHostname;
  if (sHostname.empty()) {
    if (dlib::get_local_hostname(sHostname) != 0)
      sHostname = "localhost";
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "sender", BAD_CAST sHostname.c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "instanceId", BAD_CAST intToString(instanceId).c_str()));
  char version[32];
  sprintf(version, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR, AGENT_VERSION_PATCH,
          AGENT_VERSION_BUILD);
  THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST version));

  if (aType == eASSETS || aType == eDEVICES) 
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "assetBufferSize", BAD_CAST intToString(aAssetBufferSize).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "assetCount", BAD_CAST int64ToString(aAssetCount).c_str()));
  }
  
  if (aType == eDEVICES || aType == eERROR)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "bufferSize", BAD_CAST intToString(bufferSize).c_str()));
  }
    
  if (aType == eSTREAMS)
  {
    // Add additional attribtues for streams
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "bufferSize", BAD_CAST intToString(bufferSize).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "nextSequence", 
                                                    BAD_CAST int64ToString(nextSeq).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST  "firstSequence", 
                                                    BAD_CAST int64ToString(firstSeq).c_str()));
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "lastSequence", 
                                                    BAD_CAST int64ToString(lastSeq).c_str()));
  }
  
  if (aType == eDEVICES && aCount != NULL && aCount->size() > 0)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "AssetCounts"));

    map<string,int>::const_iterator iter;
    for (iter = aCount->begin(); iter != aCount->end(); ++iter)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "AssetCount"));
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "assetType", 
                                                      BAD_CAST iter->first.c_str()));
      THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST int64ToString(iter->second).c_str()));
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
    }
    
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
}


void XmlPrinter::addSimpleElement(xmlTextWriterPtr writer, string element, const string &body,
                      const map<string, string> *attributes)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST element.c_str()));
  
  if (attributes != NULL && attributes->size() > 0) 
    addAttributes(writer, attributes);
  
  if (!body.empty())
  {
    xmlChar *text = xmlEncodeEntitiesReentrant(NULL, BAD_CAST body.c_str());
    THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
    xmlFree(text);
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer)); // Element    
}

// Cutting tools
void XmlPrinter::printCuttingToolValue(xmlTextWriterPtr writer, CuttingToolValuePtr aValue)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST aValue->mKey.c_str()));
  map<string,string>::iterator iter;
  
  for (iter = aValue->mProperties.begin(); iter != aValue->mProperties.end(); iter++) {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                    BAD_CAST (*iter).first.c_str(),
                                                    BAD_CAST (*iter).second.c_str()));
  }
  
  THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST aValue->mValue.c_str()));
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));  
}

void XmlPrinter::printCuttingToolValue(xmlTextWriterPtr writer, CuttingToolPtr aTool,
                                       const char *aValue, std::set<string> *aRemaining)
{
  if (aTool->mValues.count(aValue) > 0)
  {
    if (aRemaining != NULL) aRemaining->erase(aValue);
    CuttingToolValuePtr ptr = aTool->mValues[aValue];
    printCuttingToolValue(writer, ptr);
  }
}

void XmlPrinter::printCuttingToolValue(xmlTextWriterPtr writer, CuttingItemPtr aItem,
                                       const char *aValue, std::set<string> *aRemaining)
{
  if (aItem->mValues.count(aValue) > 0)
  {
    if (aRemaining != NULL) aRemaining->erase(aValue);
    CuttingToolValuePtr ptr = aItem->mValues[aValue];
    printCuttingToolValue(writer, ptr);
  }
}


void XmlPrinter::printCuttingToolItem(xmlTextWriterPtr writer, CuttingItemPtr aItem)
{
  THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "CuttingItem"));
  
  map<string,string>::iterator iter;
  for (iter = aItem->mIdentity.begin(); iter != aItem->mIdentity.end(); iter++) {
    THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                                    BAD_CAST (*iter).first.c_str(),
                                                    BAD_CAST (*iter).second.c_str()));
  }

  set<string> remaining;
  std::map<std::string,CuttingToolValuePtr>::const_iterator viter;
  for (viter = aItem->mValues.begin(); viter != aItem->mValues.end(); viter++)
    remaining.insert(viter->first);

  
  printCuttingToolValue(writer, aItem, "Description", &remaining);
  printCuttingToolValue(writer, aItem, "Locus", &remaining);
  
  vector<CuttingToolValuePtr>::iterator life;
  for (life = aItem->mLives.begin(); life != aItem->mLives.end(); life++) {
    printCuttingToolValue(writer, *life);
  }
  
  // Print extended items...
  set<string>::iterator prop;
  for(prop = remaining.begin(); prop != remaining.end(); prop++)
    printCuttingToolValue(writer, aItem, prop->c_str());


  // Print Measurements
  if (aItem->mMeasurements.size() > 0) {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Measurements"));
    map<string,CuttingToolValuePtr>::iterator meas;
    for (meas = aItem->mMeasurements.begin(); meas != aItem->mMeasurements.end(); meas++) {
      printCuttingToolValue(writer, *(meas->second));
    }
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));      
  }
  
  // CuttingItem
  THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));

}


string XmlPrinter::printCuttingTool(CuttingToolPtr aTool)
{
  
  xmlTextWriterPtr writer = NULL;
  xmlBufferPtr buf = NULL;
  string ret;
  
  try {
    THROW_IF_XML2_NULL(buf = xmlBufferCreate());
    THROW_IF_XML2_NULL(writer = xmlNewTextWriterMemory(buf, 0));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(writer, 1));
    THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(writer, BAD_CAST "  "));
    
    printAssetNode(writer, aTool);
    
    set<string> remaining;
    std::map<std::string,CuttingToolValuePtr>::const_iterator viter;
    for (viter = aTool->mValues.begin(); viter != aTool->mValues.end(); viter++)
      if (viter->first != "Description")
        remaining.insert(viter->first);
    
    // Check for cutting tool definition
    printCuttingToolValue(writer, aTool, "CuttingToolDefinition", &remaining);
    
    // Print the cutting tool life cycle.
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "CuttingToolLifeCycle"));
    
    // Status...
    if (aTool->mStatus.size() > 0)
    {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "CutterStatus"));
      vector<string>::iterator status;
      for (status = aTool->mStatus.begin(); status != aTool->mStatus.end(); status++) {
        THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Status"));
        THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST status->c_str()));
        THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
      }
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
    }
    
    // Other values
    printCuttingToolValue(writer, aTool, "ReconditionCount", &remaining);
    
    // Tool life
    vector<CuttingToolValuePtr>::iterator life;
    for (life = aTool->mLives.begin(); life != aTool->mLives.end(); life++) {
      printCuttingToolValue(writer, *life);
    }
        
    // Remaining items
    printCuttingToolValue(writer, aTool, "ProgramToolGroup", &remaining);
    printCuttingToolValue(writer, aTool, "ProgramToolNumber", &remaining);
    printCuttingToolValue(writer, aTool, "Location", &remaining);
    printCuttingToolValue(writer, aTool, "ProcessSpindleSpeed", &remaining);
    printCuttingToolValue(writer, aTool, "ProcessFeedRate", &remaining);
    printCuttingToolValue(writer, aTool, "ConnectionCodeMachineSide", &remaining);
    
    // Print extended items...
    set<string>::iterator prop;
    for(prop = remaining.begin(); prop != remaining.end(); prop++)
      printCuttingToolValue(writer, aTool, prop->c_str());
    
    // Print Measurements
    if (aTool->mMeasurements.size() > 0) {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "Measurements"));
      map<string,CuttingToolValuePtr>::iterator meas;
      for (meas = aTool->mMeasurements.begin(); meas != aTool->mMeasurements.end(); meas++) {
        printCuttingToolValue(writer, *(meas->second));
      }
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));      
    }
    
    // Print Cutting Items
    if (aTool->mItems.size() > 0) {
      THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST "CuttingItems"));
      THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST "count", 
                                                      BAD_CAST aTool->mItemCount.c_str()));

      vector<CuttingItemPtr>::iterator item;
      for (item = aTool->mItems.begin(); item != aTool->mItems.end(); item++) {
        printCuttingToolItem(writer, *(item));
      }
      THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));      
    }
    
    // CuttingToolLifeCycle
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
    
    // CuttingTool
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
  
    xmlFreeTextWriter(writer);
    ret = (string) ((char*) buf->content);
    xmlBufferFree(buf);    
  }
  catch (string error) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printCuttingTool: " << error;
  }
  catch (...) {
    if (buf != NULL)
      xmlBufferFree(buf);
    if (writer != NULL)
      xmlFreeTextWriter(writer);
    sLogger << dlib::LERROR << "printCuttingTool: unknown error";
  }
  
  return ret;
}


