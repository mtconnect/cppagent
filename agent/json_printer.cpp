/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "json_printer.hpp"
#include "dlib/sockets.h"
#include "dlib/logger.h"
#include "version.h"
#include <set>
#include "../rapidjson/prettywriter.h"       
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/document.h"


static dlib::logger sLogger("json.printer");

using namespace std;
using namespace rapidjson;

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

namespace JsonPrinter {
  /***** Helper Methods *****/
  /* Initiate all documents */
  template <typename Writer>
  void initJsonDoc(Writer& writer,
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
  template <typename Writer>
  void printProbeHelper(Writer& writer, Component *component);
  
  template <typename Writer>
  void printDataItem(Writer& writer, DataItem *dataItem);


  /* Add attributes to an json element */
  template <typename Writer>
  void addDeviceStream(Writer& writer, Device *device);
  template <typename Writer>
  void addComponentStream(Writer& writer, Component *component);
  template <typename Writer>
  void addCategory(Writer& writer, DataItem::ECategory category);
  
  template <typename Writer>
  void addSimpleElement(Writer& writer, std::string element, std::string &body, 
                        std::map<std::string, std::string> *attributes = NULL);

  
  template <typename Writer>
  void addAttributes(Writer& writer,
                     std::map<std::string, std::string> *attributes);
  
  template <typename Writer>
  void addAttributes( Writer& writer,
                     AttributeList *attributes);

  template <typename Writer>
  void addEvent(Writer& writer, ComponentEvent *result);
  
  // Asset printing
  template <typename Writer>
  void printCuttingToolValue(Writer& writer, CuttingToolPtr aTool,
                             const char *aValue, std::set<string> *aRemaining = NULL);
  
  template <typename Writer>
  void printCuttingToolValue(Writer& writer, CuttingItemPtr aItem,
                             const char *aValue, std::set<string> *aRemaining = NULL);
  
  template <typename Writer>
  void printCuttingToolValue(Writer& writer, CuttingToolValuePtr aValue);
  
  template <typename Writer>
  void printCuttingToolItem(Writer& writer, CuttingItemPtr aItem);
  
  template <typename Writer>
  void printAssetNode(Writer& writer, Asset *anAsset);

};


/* JsonPrinter main methods */
string JsonPrinter::printError(const unsigned int instanceId,
                              const unsigned int bufferSize,
                              const uint64_t nextSeq,
                              const string& errorCode,
                              const string& errorText)
{
  string ret;
  StringBuffer buffer;
  PrettyWriter <StringBuffer> writer(buffer);
  
  try {
    initJsonDoc(writer, eERROR, instanceId,
               bufferSize, 0, 0, nextSeq, nextSeq - 1);
  
    
    writer.String("Errors");
    writer.StartObject();
    writer.String("Error");
    writer.StartObject();
    writer.String("errorCode");
    writer.String(errorCode.c_str());

    writer.String("Raw");
    writer.String(errorText.c_str());
    
    writer.EndObject(); // Error
    writer.EndObject(); // Errors
    writer.EndObject(); // MTConnectError
    writer.EndObject(); // JsonDocument

    // Cleanup
    ret = (string) ((char*) buffer.GetString());
    buffer.Clear();
  }
  catch (...) {
    buffer.Clear();
    sLogger << dlib::LERROR << "printError: unknown error";
  }
  
  return ret;
}

string JsonPrinter::printProbe(const unsigned int instanceId,
                              const unsigned int bufferSize,
                              const uint64_t nextSeq,
                              const unsigned int aAssetBufferSize,
                              const unsigned int aAssetCount,
                              vector<Device *>& deviceList,
                              const std::map<std::string, int> *aCount)
{
  string ret;
  StringBuffer buffer;
  PrettyWriter <StringBuffer> writer(buffer);
  try {
  
    initJsonDoc(writer, eDEVICES,
                instanceId,
                bufferSize,
                aAssetBufferSize,
                aAssetCount,
                nextSeq, 0, nextSeq - 1,
                aCount);
    
    writer.String("Devices");
    writer.StartArray();
    
    vector<Device *>::iterator dev;
    for (dev = deviceList.begin(); dev != deviceList.end(); dev++)
    {
      writer.StartObject();
      writer.String("Device");
      printProbeHelper(writer, *dev);
      writer.EndObject();
    }      

    writer.EndArray();  // Devices
    writer.EndObject(); // MTConnectDevices
    writer.EndObject(); // JsonDocument

    
    ret = (string) ((char*) buffer.GetString());
    buffer.Clear();    
    
  }
  catch (...) {
    buffer.Clear();
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  return ret;
 }

template <typename Writer>
void JsonPrinter::printProbeHelper(Writer& writer,
                                  Component * component)
{
  writer.StartObject();
  addAttributes(writer, component->getAttributes());
  writer.EndObject(); // attrs
  std::map<string, string> desc = component->getDescription();
  string body = component->getDescriptionBody();
  
  if (desc.size() > 0 || !body.empty())
  {
    addSimpleElement(writer, "Description", body, &desc);
  }
  
  if (!component->getConfiguration().empty()) {
    writer.String("Configuration");
    writer.StartObject();
    writer.String("Raw");
    writer.String(component->getConfiguration().c_str());
    writer.EndObject(); // Configuration 
  }
  
  list<DataItem *> datum = component->getDataItems();
  if (datum.size() > 0)
  {
    writer.String("DataItems");
    writer.StartArray();
    
    list<DataItem *>::iterator data;
    for (data = datum.begin(); data!= datum.end(); data++)
    {
      writer.StartObject();
      printDataItem(writer, *data);
      writer.EndObject();
    }
    writer.EndArray(); // DataItems    
  }
  
  if (component->getReferences().size() > 0)
  {
    writer.String("References");
    writer.StartArray();
    
    list<Component::Reference>::const_iterator ref;
    for (ref = component->getReferences().begin(); ref != component->getReferences().end(); ref++)
    {
      writer.StartObject();
      writer.String("Reference");
      writer.StartObject();
      writer.String("attrs");
      writer.StartObject();
      writer.String("dataItemId");
      writer.String(ref->mId.c_str());

      if (ref->mName.length() > 0) {
        writer.String("name");
        writer.String(ref->mName.c_str());
      }
      
      writer.EndObject(); // attrs
      writer.EndObject(); // Reference
      writer.EndObject();
    }
    writer.EndArray(); // References
  }
  
  list<Component *> children = component->getChildren();
  
  if (children.size() > 0)
  {
    writer.String("Components");
    writer.StartArray();
    
    list<Component *>::iterator child;
    for (child = children.begin(); child != children.end(); child++)
    {
      const char *name = NULL;
      if (!(*child)->getPrefix().empty()) 
      {
        map<string, SchemaNamespace>::iterator ns = sDevicesNamespaces.find((*child)->getPrefix());
        
        if (ns != sDevicesNamespaces.end()) {
          name = (*child)->getPrefixedClass().c_str();
        }
      }
      if (name == NULL) name = (*child)->getClass().c_str();
      
      writer.StartObject();
      writer.String(name);
      printProbeHelper(writer, *child);
      writer.EndObject(); // Component 
    }
    writer.EndArray(); // Components    
  }
  writer.EndObject();  
}




template <typename Writer>
void JsonPrinter::printDataItem(Writer& writer, DataItem *dataItem)
{
  writer.String("DataItem");
  writer.StartObject();
  addAttributes(writer, dataItem->getAttributes());
  writer.EndObject(); // attrs
  string source = dataItem->getSource();
  if (!source.empty())
  {
    addSimpleElement(writer, "Source", source);
  }
  
  if (dataItem->hasConstraints())
  {
    writer.String("Constraints");
    writer.StartObject();
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
    
    writer.String("ConstraintValues");
    writer.StartArray();
    
    for (value = values.begin(); value != values.end(); value++)
    {
      writer.StartObject();
      addSimpleElement(writer, "Value", *value);
      writer.EndObject();
    }
    
    writer.EndArray(); // ConstraintValues
    
    if (dataItem->getFilterType() == DataItem::FILTER_MINIMUM_DELTA)
    {
      map<string, string> attributes;
      string value = floatToString(dataItem->getFilterValue());
      attributes["type"] = "MINIMUM_DELTA";
      addSimpleElement(writer, "Filter", value, &attributes);      
    }
    
    writer.EndObject(); // Constraints   
  }
  
  writer.EndObject(); // DataItem   
}

typedef bool (*EventComparer)(ComponentEventPtr &aE1, ComponentEventPtr &aE2);

static bool EventCompare(ComponentEventPtr &aE1, ComponentEventPtr &aE2)
{
  return aE1 < aE2;
}

string JsonPrinter::printSample(const unsigned int instanceId,
                               const unsigned int bufferSize,
                               const uint64_t nextSeq,
                               const uint64_t firstSeq,
                               const uint64_t lastSeq,
                               ComponentEventPtrArray& results
  )
{
  string ret;
  StringBuffer buffer;
  PrettyWriter <StringBuffer> writer(buffer);
  
  try {
    initJsonDoc(writer, eSTREAMS,
               instanceId,
               bufferSize,
               0, 0,
               nextSeq,
               firstSeq,
               lastSeq);
        
    writer.String("Streams");
    writer.StartArray();
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
        if (lastCategory != -1)
        {
          writer.EndArray(); // Category
        }
        lastCategory = -1;

        if (lastComponent != NULL)
        {
          writer.EndObject(); // ComponentStream
          writer.EndObject(); // ComponentStream
        }
        lastComponent = NULL;
        
        if (lastDevice != NULL)
        {
          writer.EndArray();  // ComponentStreams
          writer.EndObject(); // DeviceStream
          writer.EndObject(); // DeviceStream
        }
        lastDevice = device;
        addDeviceStream(writer, device);
      }
      
      if (component != lastComponent)
      {
        if (lastCategory != -1)
        {
          writer.EndArray(); // Category
        }
        lastCategory = -1;

        if (lastComponent != NULL)
        {
          writer.EndObject(); // ComponentStream
          writer.EndObject();
        }

        lastComponent = component;
        addComponentStream(writer, component);        
      }
      
      if (lastCategory != dataItem->getCategory())
      {
        if (lastCategory != -1)
        {
            writer.EndArray(); // Category
        }

        lastCategory = dataItem->getCategory();
        addCategory(writer, dataItem->getCategory());
      }
      addEvent(writer, result);
    }
    
    if (lastCategory != -1)
    {
      writer.EndArray(); // Category
    }
    if (lastComponent != NULL)
    {
      writer.EndObject(); // ComponentStream
      writer.EndObject();
    }
    if (lastDevice != NULL)
    {
      writer.EndArray(); // ComponentStreams
      writer.EndObject(); // DeviceStream
      writer.EndObject(); // DeviceStream
    }

    writer.EndArray();  // Streams
    writer.EndObject(); // MTConnectStreams
    writer.EndObject(); // JsonDocument
    
    ret = (string) ((char*) buffer.GetString());
    buffer.Clear();    
  }
  catch (...) {
    buffer.Clear();
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  
  return ret;
}

string JsonPrinter::printAssets(const unsigned int instanceId,
                               const unsigned int aBufferSize,
                               const unsigned int anAssetCount,
                               std::vector<AssetPtr> &anAssets)
{
  string ret;
  StringBuffer buffer;
  PrettyWriter <StringBuffer> writer(buffer);
  Document d;
  
  try {
    initJsonDoc(writer, eASSETS, instanceId, 0, aBufferSize, anAssetCount, 0);
    
    writer.String("Assets");
    writer.StartArray();

    vector<AssetPtr>::iterator iter;
    for (iter = anAssets.begin(); iter != anAssets.end(); ++iter)
    {
      if ((*iter)->getType() == "CuttingTool" || (*iter)->getType() == "CuttingToolArchetype") {
        d.Parse((*iter)->getContent("JSON").c_str());
        d.Accept(writer);
      } 
      else {
        printAssetNode(writer, (*iter));
        d.Parse((*iter)->getContent("JSON").c_str());
        d.Accept(writer);
      }
    }
    
    writer.EndArray(); // Assets    
    writer.EndObject(); // MTConnectAssets
    writer.EndObject(); // EndDocument
    ret = (string) ((char*) buffer.GetString());
    buffer.Clear();
  }
   catch (...) {
    buffer.Clear();
    sLogger << dlib::LERROR << "printProbe: unknown error";
  }
  
  return ret;
}

template <typename Writer>
void JsonPrinter::printAssetNode(Writer& writer, Asset *anAsset)
{
  // TODO: Check if cutting tool or archetype - should be in type
  writer.String(anAsset->getType().c_str());
  writer.StartObject();
  addAttributes(writer, &anAsset->getIdentity());

  // Add the timestamp and device uuid fields.
  writer.String("timestamp");
  writer.String(anAsset->getTimestamp().c_str());
  writer.String("deviceUuid");
  writer.String(anAsset->getDeviceUuid().c_str());
  writer.String("assetId");
  writer.String(anAsset->getAssetId().c_str());
  
  if (anAsset->isRemoved())
  {
    writer.String("removed");
    writer.String("true");
  }
  writer.EndObject(); // attrs
}

template <typename Writer>
void JsonPrinter::addDeviceStream(Writer& writer, Device *device)
{
  writer.StartObject();
  writer.String("DeviceStream");
  writer.StartObject();
  writer.String("attrs");
  writer.StartObject();
  writer.String("name");
  writer.String(device->getName().c_str());
  writer.String("uuid");
  writer.String(device->getUuid().c_str());
  writer.EndObject();
  writer.String("ComponentStreams");
  writer.StartArray();
}

template <typename Writer>
void JsonPrinter::addComponentStream(Writer& writer, Component *component)
{
  writer.StartObject();
  writer.String("ComponentStream");
  writer.StartObject();
  writer.String("attrs");
  writer.StartObject();
  writer.String("component");
  writer.String(component->getClass().c_str());
  writer.String("name");
  writer.String(component->getName().c_str());
  writer.String("componentId");
  writer.String(component->getId().c_str());
  writer.EndObject();
}

template <typename Writer>
void JsonPrinter::addCategory(Writer& writer, DataItem::ECategory category)
{
  switch (category)
  {
    case DataItem::SAMPLE:
      writer.String("Samples");
      break;
      
    case DataItem::EVENT:
      writer.String("Events");
      break;
      
    case DataItem::CONDITION:
      writer.String("Condition");
      break;
  }
  writer.StartArray();
}

template <typename Writer>
void JsonPrinter::addEvent(Writer& writer, ComponentEvent *result)
{
  DataItem *dataItem = result->getDataItem();
  writer.StartObject();

  if (dataItem->isCondition()) {
    writer.String(result->getLevelString().c_str());
  } 
  else {
    const char *element = NULL;
    if (!dataItem->getPrefix().empty()) 
    {
      map<string, SchemaNamespace>::iterator ns = sStreamsNamespaces.find(dataItem->getPrefix());
      if (ns != sStreamsNamespaces.end()) {
        element = dataItem->getPrefixedElementName().c_str();
      }
    }
    if (element == NULL) element = dataItem->getElementName().c_str();
    
    writer.String(element);
  }
  writer.StartObject();
  addAttributes(writer, result->getAttributes());
  
  if (result->isTimeSeries() && result->getValue() != "UNAVAILABLE") {
    ostringstream ostr;
    ostr.precision(6);
    const vector<float> &v = result->getTimeSeries();
    for (size_t i = 0; i < v.size(); i++)
      ostr << v[i] << ' ';
    string str = ostr.str();
    writer.String("Raw");
    writer.String(str.c_str());
  } 
  else if (!result->getValue().empty()) {
    writer.String("Raw");
    writer.String(result->getValue().c_str());
  }
  writer.EndObject();
  writer.EndObject();
}

template <typename Writer>
void JsonPrinter::addAttributes(Writer& writer,
                               std::map<string, string> *attributes)
{
  std::map<string, string>::iterator attr;
  writer.String("attrs");
  writer.StartObject();
  
  for (attr= attributes->begin(); attr!=attributes->end(); attr++)
  {
    writer.String(attr->first.c_str());
    writer.String(attr->second.c_str());
  }
}


template <typename Writer>
void JsonPrinter::addAttributes(Writer& writer,
                               AttributeList *attributes)
{
  AttributeList::iterator attr;
  writer.String("attrs");
  writer.StartObject();
  
  for (attr= attributes->begin(); attr!=attributes->end(); attr++)
  {
    writer.String(attr->first);
    writer.String(attr->second.c_str());
  }
  writer.EndObject();
}

/* JsonPrinter helper Methods */
template <typename Writer>
void JsonPrinter::initJsonDoc(Writer& writer, 
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
  writer.StartObject();
  // TODO: Cache the locations and header attributes.
  // Write the root element  
  string jsonType, style;
  map<string, SchemaNamespace> *namespaces;
  
  switch (aType) {
  case eERROR:
      namespaces = &sErrorNamespaces;
      style = mErrorStyle;
      jsonType = "Error";
      break;
      
  case eSTREAMS:
      namespaces = &sStreamsNamespaces;
      style = mStreamsStyle;
      jsonType = "Streams";
      break;
      
  case eDEVICES:
      namespaces = &sDevicesNamespaces;
      style = mDevicesStyle;
      jsonType = "Devices";
      break;
      
    case eASSETS:
      namespaces = &sAssetsNamespaces;
      style = mAssetsStyle;
      jsonType = "Assets";
      break;
  }
  
  if (!style.empty())
  {
    string pi = "xml-stylesheet type=\"text/xsl\" href=\"" + style + '"';
    writer.String("Pi");
    writer.String(pi.c_str());
  }
  
  string rootName = "MTConnect" + jsonType;
  string xmlns = "urn:mtconnect.org:" + rootName + ":" + sSchemaVersion;  
  string location;
  
  writer.String(rootName.c_str());
  
  // Always make the default namespace and the m: namespace MTConnect default.
  writer.StartObject();
  writer.String("attrs");
  writer.StartObject();
  writer.String("xmlns:m");
  writer.String(xmlns.c_str());
  writer.String("xmlns");
  writer.String(xmlns.c_str());
  
  // Alwats add the xsi namespace
  writer.String("xmlns:xsi");
  writer.String("http://www.w3.org/2001/XMLSchema-instance");
  string mtcLocation;
  
  // Add in the other namespaces if they exist
  map<string, SchemaNamespace>::iterator ns;
  for (ns = namespaces->begin(); ns != namespaces->end(); ns++)
  {
    // Skip the mtconnect ns (always m)
    if (ns->first != "m")
    {
      string attr = "xmlns:" + ns->first;
      
      writer.String(attr.c_str());
      writer.String(ns->second.mUrn.c_str());

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
    location = xmlns + " http://www.mtconnect.org/schemas/" + rootName + "_" + sSchemaVersion + ".xsd";
  }
  
  writer.String("xsi:schemaLocation");
  writer.String(location.c_str());
  writer.EndObject();
  
  // Create the header
  writer.String("Header");
  writer.StartObject();
  writer.String("attrs");
  writer.StartObject();

  static std::string sHostname;
  if (sHostname.empty()) {
    if (dlib::get_local_hostname(sHostname) != 0)
      sHostname = "localhost";
  }
  
  writer.String("sender");
  writer.String(sHostname.c_str());
  writer.String("instanceId");
  writer.String(intToString(instanceId).c_str());
  char version[32];
  sprintf(version, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR, AGENT_VERSION_PATCH,
          AGENT_VERSION_BUILD);
  writer.String("version");
  writer.String(version);

  if (aType == eASSETS || aType == eDEVICES) 
  {
    writer.String("assetBufferSize");
    writer.String(intToString(aAssetBufferSize).c_str());
    writer.String("assetCount");
    writer.String(intToString(aAssetCount).c_str());
  }
  
  if (aType == eDEVICES || aType == eERROR)
  {
    writer.String("bufferSize");
    writer.String(intToString(bufferSize).c_str());
  }
    
  if (aType == eSTREAMS)
  {
    // Add additional attribtues for streams
    writer.String("bufferSize");
    writer.String(intToString(bufferSize).c_str());
    writer.String("nextSequence");
    writer.String(intToString(nextSeq).c_str());
    writer.String("lastSequence");
    writer.String(intToString(lastSeq).c_str());
  }
  writer.EndObject();
  
  if (aType == eDEVICES && aCount != NULL && aCount->size() > 0)
  {
    writer.String("AssetCounts");
    writer.StartArray();  
  
    map<string,int>::const_iterator iter;
    for (iter = aCount->begin(); iter != aCount->end(); ++iter)
    {
      writer.StartObject();
      writer.String("AssetCount");
      writer.StartObject();
      writer.String("attr");
      writer.StartObject();
      writer.String("assetType");
      writer.String(iter->first.c_str());
      writer.EndObject();
      writer.String("String");
      writer.String(int64ToString(iter->second).c_str());
      writer.EndObject();
      writer.EndObject();
    }
    writer.EndArray();
  }
  writer.EndObject();
}

template <typename Writer>
void JsonPrinter::addSimpleElement(Writer& writer, string element, string &body, 
                      map<string, string> *attributes)
{
  writer.String(element.c_str());
  writer.StartObject();
  if (attributes != NULL && attributes->size() > 0) 
  {
    addAttributes(writer, attributes);
    writer.EndObject(); // attrs
  }
  if (!body.empty())
  {
    writer.String("Raw");
    writer.String(body.c_str());
  }   
  writer.EndObject(); // Element 

}

// Cutting tools
template <typename Writer>
void JsonPrinter::printCuttingToolValue(Writer& writer, CuttingToolValuePtr aValue)
{
  writer.String(aValue->mKey.c_str());

  map<string,string>::iterator iter;
  writer.StartObject();
  writer.String("attrs");
  writer.StartObject();

  for (iter = aValue->mProperties.begin(); iter != aValue->mProperties.end(); iter++) {
    writer.String((*iter).first.c_str());
    writer.String((*iter).second.c_str());
  }

  writer.EndObject();
  writer.String("Raw");
  writer.String(aValue->mValue.c_str());
  writer.EndObject();
}

template <typename Writer>
void JsonPrinter::printCuttingToolValue(Writer& writer, CuttingToolPtr aTool,
                                       const char *aValue, std::set<string> *aRemaining)
{
  if (aTool->mValues.count(aValue) > 0)
  {
    if (aRemaining != NULL) aRemaining->erase(aValue);
    CuttingToolValuePtr ptr = aTool->mValues[aValue];
    printCuttingToolValue(writer, ptr);
  }
}

template <typename Writer>
void JsonPrinter::printCuttingToolValue(Writer& writer, CuttingItemPtr aItem,
                                       const char *aValue, std::set<string> *aRemaining)
{
  if (aItem->mValues.count(aValue) > 0)
  {
    if (aRemaining != NULL) aRemaining->erase(aValue);
    CuttingToolValuePtr ptr = aItem->mValues[aValue];
    printCuttingToolValue(writer, ptr);
  }
}

template <typename Writer>
void JsonPrinter::printCuttingToolItem(Writer& writer, CuttingItemPtr aItem)
{
  writer.String("CuttingItem");
  writer.StartObject();
  writer.String("attrs");
  writer.StartObject();
 
  map<string,string>::iterator iter;
  for (iter = aItem->mIdentity.begin(); iter != aItem->mIdentity.end(); iter++) {
    writer.String((*iter).first.c_str());
    writer.String((*iter).second.c_str());
   }
  writer.EndObject();

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
    writer.String("Measurements");
    writer.StartObject();
    map<string,CuttingToolValuePtr>::iterator meas;
    for (meas = aItem->mMeasurements.begin(); meas != aItem->mMeasurements.end(); meas++) {
      printCuttingToolValue(writer, *(meas->second));
    }
    writer.EndObject();
  }
  writer.EndObject(); // CuttingItem
  
}

string JsonPrinter::printCuttingTool(CuttingToolPtr aTool)
{
  
  string ret;
  StringBuffer buffer;
  PrettyWriter <StringBuffer> writer(buffer);
  
  try {
    writer.StartObject();
    printAssetNode(writer, aTool);
    
    set<string> remaining;
    std::map<std::string,CuttingToolValuePtr>::const_iterator viter;
    for (viter = aTool->mValues.begin(); viter != aTool->mValues.end(); viter++)
      if (viter->first != "Description")
        remaining.insert(viter->first);
    
    // Check for cutting tool definition
    printCuttingToolValue(writer, aTool, "CuttingToolDefinition", &remaining);
    
    // Print the cutting tool life cycle.
    writer.String("CuttingToolLifeCycle");
    writer.StartObject();
 
    // Status...
    writer.String("CutterStatus");
    writer.StartArray();
 
    vector<string>::iterator status;
    for (status = aTool->mStatus.begin(); status != aTool->mStatus.end(); status++) {
      writer.StartObject();
      writer.String("Status");
      writer.String(status->c_str());
      writer.EndObject(); 
    }
    writer.EndArray(); // CutterStatus
    
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
      writer.String("Measurements");
      writer.StartObject();
      map<string,CuttingToolValuePtr>::iterator meas;
      for (meas = aTool->mMeasurements.begin(); meas != aTool->mMeasurements.end(); meas++) {
        printCuttingToolValue(writer, *(meas->second));
      }
      writer.EndObject();
    }
    
    // Print Cutting Items
    if (aTool->mItems.size() > 0) {
      writer.String("CuttingItems");
      writer.StartObject();
 
      writer.String("attrs");
      writer.StartObject();
      writer.String("count");
      writer.String(aTool->mItemCount.c_str());
      writer.EndObject();
 
      vector<CuttingItemPtr>::iterator item;
      for (item = aTool->mItems.begin(); item != aTool->mItems.end(); item++) {
        printCuttingToolItem(writer, *(item));
      }
      writer.EndObject(); // CuttingItems
    }
    
    writer.EndObject(); // CuttingToolLifeCycle
    
    writer.EndObject(); // CuttingTool

    writer.EndObject(); // JsonDocument
  
    ret = (string) ((char*) buffer.GetString());
    buffer.Clear();    
  }
  catch (...) {
    buffer.Clear();
    sLogger << dlib::LERROR << "printCuttingTool: unknown error";
  }
  
  return ret;
}