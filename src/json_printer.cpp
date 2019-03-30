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

#include "json_printer.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <cstdlib>
#include <set>

#include "dlib/sockets.h"
#include "device.hpp"
#include "composition.hpp"
#include "sensor_configuration.hpp"

using namespace std;
using json = nlohmann::json;

namespace mtconnect {
  static inline std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
      return !std::isspace(ch);
    }));
    return s;
  }
  
  // trim from end (in place)
  static inline std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
      return !std::isspace(ch);
    }).base(), s.end());
    return s;
  }
  
  // trim from both ends (in place)
  static inline std::string trim(std::string s) {
    return rtrim(ltrim(s));
  }
  
  const string &JsonPrinter::hostname() const
  {
    if (m_hostname.empty())
    {
      string name;
      if (dlib::get_local_hostname(name))
        name = "localhost";
      // Breaking the rules, this is a one off
      const_cast<JsonPrinter*>(this)->m_hostname = name;
    }
    
    return m_hostname;
  }
  
  inline std::string print(json &doc, bool pretty)
  {
    stringstream buffer;
    if (pretty)
      buffer << std::setw(2);
    buffer << doc;
    if (pretty)
      buffer << "\n";
    return buffer.str();
  }
  
  static inline void addAttributes(json &doc, const map<string,string> &attrs)
  {
    for (const auto &attr : attrs)
      doc[string("@") + attr.first] = attr.second;
  }

  static inline void addAttributes(json &doc, const AttributeList &attrs)
  {
    for (const auto &attr : attrs)
      doc[string("@") + attr.first] = attr.second;
  }

  
  static inline void add(json &doc, const char *key, const string &value)
  {
    if (!value.empty())
      doc[key] = value;
  }
  
  static inline void addDouble(json &doc, const char *key, const string &value)
  {
    if (!value.empty())
      doc[key] = atof(value.c_str());
  }

  
  static inline void addText(json &doc, const std::string &text)
  {
    add(doc, "#text", trim(text));
  }

  
  inline json header(
                     const string &version,
                     const string &hostname,
                     const unsigned int instanceId,
                     const unsigned int bufferSize)
  {
    json doc = json::object(
      { 
        { "@version", version},
        { "@creationTime", getCurrentTime(GMT) },
        { "@testIndicator", false },
        { "@instanceId", instanceId },
        { "@sender", hostname }
      });
    if (bufferSize > 0)
      doc["@bufferSize"] = bufferSize;
    return doc;
  }
  
  inline json probeAssetHeader(const string &version,
                          const string &hostname,
                          const unsigned int instanceId,
                          const unsigned int bufferSize,
                          const unsigned int assetBufferSize,
                          const unsigned int assetCount)
  {
    json doc = header(version, hostname,
                      instanceId, bufferSize);
    doc["@assetBufferSize"] = assetBufferSize;
    doc["@assetCount"] = assetCount;
    
    return doc;
  }
  
  inline json streamHeader(const string &version,
                           const string &hostname,
                           const unsigned int instanceId,
                           const unsigned int bufferSize,
                           const uint64_t nextSequence,
                           const uint64_t firstSequence,
                           const uint64_t lastSequence)
  {
    json doc = header(version, hostname,
                      instanceId, bufferSize);
    doc["@nextSequence"] = nextSequence;
    doc["@lastSequence"] = lastSequence;
    doc["@firstSequence"] = firstSequence;
    return doc;
  }

  
  std::string JsonPrinter::printError(
                                 const unsigned int instanceId,
                                 const unsigned int bufferSize,
                                 const uint64_t nextSeq,
                                 const std::string &errorCode,
                                 const std::string &errorText
                                 )  const
  {
    json doc = json::object({ { "MTConnectError", {
      { "Header",
        header(m_version, hostname(), instanceId, bufferSize) },
      { "Errors" ,
        {
          { "Error",
            {
              { "@errorCode", errorCode },
              { "#text", trim(errorText) }
            }
          }
        }
      } } }
    });
    
    return print(doc, m_pretty);
  }
  
  static inline json toJson(DataItem *item)
  {
    json obj = json::object();
    addAttributes(obj, item->getAttributes());
    
    // Data Item Source
    json source = json::object();
    addText(source, item->getSource());
    add(source, "@dataItemId", item->getSourceDataItemId());
    add(source, "@componentId", item->getSourceComponentId());
    add(source, "@compositionId", item->getSourceCompositionId());

    if (source.begin() != source.end())
      obj["Source"] = source;
    
    // Data Item Constraints
    if (item->hasConstraints()) {
      json constraints = json::object();
      addDouble(constraints, "Maximum", item->getMaximum());
      addDouble(constraints, "Minimum", item->getMinimum());
      
      if (item->getConstrainedValues().size() > 0) {
        json values(item->getConstrainedValues());
        constraints["Value"] = values;
      }
      
      obj["Constraints"] = constraints;
    }
    
    if (item->hasMinimumDelta() or item->hasMinimumPeriod()) {
      json filters = json::array();
      if (item->hasMinimumDelta())
        filters.push_back(json::object({
          { "Filter", {
            { "Value", item->getFilterValue() },
            { "@type", "MINIMUM_DELTA" }
          }}}));
      if (item->hasMinimumPeriod())
        filters.push_back(json::object({
          { "Filter", {
            { "Value", item->getFilterPeriod() },
            { "@type", "PERIOD" }
          }}}));
      obj["Filters"] = filters;
    }
    
    if (item->hasInitialValue()) {
      char *ep;
      obj["InitialValue"] = strtod(item->getInitialValue().c_str(), &ep);
    }


    json dataItem = json::object({ { "DataItem", obj } });

    return dataItem;
  }
  
  static inline json toJson(Composition *composition)
  {
    json obj = json::object();
    
    addAttributes(obj, composition->getAttributes());
    if (composition->getDescription() != nullptr) {
      json desc = json::object();
      addAttributes(desc, composition->getDescription()->getAttributes());
      addText(desc, composition->getDescription()->getBody());
      obj["Description"] = desc;
    }
    
    json comp = json::object({ { "Composition", obj } });
    
    return comp;
  }
  
  static inline json jsonReference(const Component::Reference &reference)
  {
    json ref = json::object({ {
      "@idRef", reference.m_id
    } });
    add(ref, "@name", reference.m_name);
    return ref;
  }
  
  static inline json toJson(const Component::Reference &reference)
  {
    json obj;
    if (reference.m_type == reference.DATA_ITEM)
      obj["DataItemRef"] = jsonReference(reference);
    else if (reference.m_type == reference.COMPONENT)
      obj["ComponentRef"] = jsonReference(reference);

    return obj;
  }
  
  template<class T>
  inline void toJson(json &parent, const string &collection, T &list)
  {
    json obj;
    if (list.size() > 0) {
      json items = json::array();
      for (auto &item : list)
        items.push_back(toJson(item));
      
      parent[collection] = items;
    }
  }
  
  inline void toJson(json &parent, const SensorConfiguration::Calibration &cal)
  {
    if (!cal.m_date.empty())
      parent["CalibrationDate"] = cal.m_date;
    if (!cal.m_nextDate.empty())
      parent["NextCalibrationDate"] = cal.m_nextDate;
    if (!cal.m_initials.empty())
      parent["CalibrationInitials"] = cal.m_initials;
  }
  
  inline void toJson(json &parent, const ComponentConfiguration *config)
  {
    if (typeid(*config) == typeid(SensorConfiguration))
    {
      auto obj = static_cast<const SensorConfiguration*>(config);
      json sensor = json::object();
      if (!obj->getFirmwareVersion().empty())
        sensor["FirmwareVersion"] = obj->getFirmwareVersion();
      auto &cal = obj->getCalibration();
      toJson(sensor, cal);
      
      if (obj->getChannels().size() > 0) {
        json channels = json::array();
        
        for (auto &channel : obj->getChannels()) {
          json chan = json::object();
          addAttributes(chan, channel.getAttributes());
          
          if (!channel.getDescription().empty())
            chan["Description"] = channel.getDescription();
          
          auto &cal = channel.getCalibration();
          toJson(chan, cal);
          
          json chanObj = json::object({ { "Channel", chan } });
          channels.push_back(chanObj);
        }
        
        sensor["Channels"] = channels;
      }
      parent["Configuration"] = json::object({ { "SensorConfiguration", sensor } });
    } else if (typeid(*config) == typeid(ExtendedComponentConfiguration)) {
      auto obj = static_cast<const ExtendedComponentConfiguration*>(config);
      parent["Configuration"] = obj->getContent();
    }
  }
  
  static json toJson(Component *component)
  {
    json desc = json::object();
    addAttributes(desc, component->getDescription());
    addText(desc, component->getDescriptionBody());
    
    json comp = json::object();
    addAttributes(comp, component->getAttributes());
    
    if (desc.begin() != desc.end())
      comp["Description"] = desc;
    
    if (component->getConfiguration())
      toJson(comp, component->getConfiguration());
    toJson(comp, "DataItems", component->getDataItems());
    toJson(comp, "Components", component->getChildren());
    toJson(comp, "Compositions", component->getCompositions());
    toJson(comp, "References", component->getReferences());
    
    json doc = json::object({ { component->getClass(), comp } });
    
    return doc;
  }
  
  std::string JsonPrinter::printProbe(
                                 const unsigned int instanceId,
                                 const unsigned int bufferSize,
                                 const uint64_t nextSeq,
                                 const unsigned int assetBufferSize,
                                 const unsigned int assetCount,
                                 const std::vector<Device *> &devices,
                                 const std::map<std::string, int> *count)  const
  {
    json devicesDoc = json::array();
    for (const auto device : devices)
      devicesDoc.push_back(toJson(device));
    
    json doc = json::object({ { "MTConnectDevices", {
      { "Header",
        probeAssetHeader(m_version, hostname(), instanceId,
                         bufferSize, assetBufferSize, assetCount) },
      { "Devices", devicesDoc }
    } } });

    return print(doc, m_pretty);
  }
  
  inline json toJson(const ComponentEventPtr &observation)
  {
    auto dataItem = observation->getDataItem();
    string name;
    
    if (dataItem->isCondition())
    {
      name = observation->getLevelString();
    }
    else
    {
      if (!dataItem->getPrefix().empty())
      {
        name = dataItem->getPrefixedElementName();
      }
      
      if (name.empty())
        name = dataItem->getElementName();
    }
    
    json value;
    
    if (observation->isTimeSeries() && observation->getValue() != "UNAVAILABLE")
    {
      ostringstream ostr;
      ostr.precision(6);
      const auto &v = observation->getTimeSeries();
      
      value = json::array();
      
      for (auto &e : v)
        value.push_back(e);
    }
    else if (observation->isDataSet() && observation->getValue() != "UNAVAILABLE")
    {
      value = json::object();
      
      const DataSet &set = observation->getDataSet();
      for (auto &e : set)
        value[e.first] = e.second;
    }
    else if (!observation->getValue().empty())
    {
      if (dataItem->getCategory() == DataItem::SAMPLE) {
        if (dataItem->is3D()) {
          value = json::array();
          stringstream s(observation->getValue());
          int i;
          for (i = 0; s && i < 3; i++) {
            double v;
            s >> v;
            value.push_back(v);
          }
          if (i < 3)
          {
            for (; i < 3; i++)
              value.push_back(0.0);
          }
        }
        else
        {
          char *ep;
          value = strtod(observation->getValue().c_str(), &ep);
        }
      } else {
        value = observation->getValue();
      }
    }

    json obj = json::object();
    for (const auto &attr : observation->getAttributes())
    {
      if (strcmp(attr.first, "sequence") == 0) {
        obj["@sequence"] = observation->getSequence();
      } else if (strcmp(attr.first, "sampleCount") == 0 or
               strcmp(attr.first, "sampleRate") == 0 or
               strcmp(attr.first, "duration") == 0) {
        char *ep;
        obj[string("@") + attr.first] = strtod(attr.second.c_str(), &ep);
      } else {
        obj[string("@") + attr.first] = attr.second;
      }
    }
    obj["Value"] = value;
    
    return json::object({ { name, obj } });
  }
  
  class CategoryRef {
  public:
    CategoryRef(const char *cat) : m_category(cat) {}
    CategoryRef(const CategoryRef &other)
    : m_category(other.m_category), m_events(other.m_events) {}
    
    bool addObservation(const ComponentEventPtr &observation) {
      m_events.push_back(observation);
      return true;
    }
    
    bool isCategory(const char *cat) {
      return m_category == cat;
    }
    
    pair<string,json> toJson() {
      pair<string,json> ret;
      if (!m_category.empty()) {
        json items = json::array();
        for (auto &event : m_events)
          items.push_back(::mtconnect::toJson(event));
        
        ret = make_pair(m_category, items);
      }
      return ret;
    }
    
  protected:
    string m_category;
    vector<ComponentEventPtr> m_events;
  };
  
  class ComponentRef {
  public:
    ComponentRef(const Component *component) : m_component(component), m_categoryRef(nullptr) {}
    ComponentRef(const ComponentRef &other)
    : m_component(other.m_component), m_categories(other.m_categories), m_categoryRef(nullptr)  {}
    
    bool isComponent(const Component *component) {
      return m_component == component;
    }
    
    bool addObservation(const ComponentEventPtr &observation,
                        const Component *component, const DataItem *dataItem)
    {
      if (m_component == component)
      {
        auto cat = dataItem->getCategoryText();
        if (m_categoryRef == nullptr || !m_categoryRef->isCategory(cat))
        {
          m_categories.emplace_back(cat);
          m_categoryRef = &m_categories.back();
        }
        
        if (m_categoryRef != nullptr)
          return m_categoryRef->addObservation(observation);
      }
      return false;
    }
    
    json toJson() {
      json ret;
      if (m_component != nullptr && !m_categories.empty()) {
        json obj = json::object({
          { "@component", m_component->getClass() },
          { "@componentId", m_component->getId() }
        });
        
        if (!m_component->getName().empty())
          obj["@name"] = m_component->getName();
        
        for (auto &cat : m_categories) {
          auto c = cat.toJson();
          if (!c.first.empty())
            obj[c.first] = c.second;
        }
        
        ret = json::object({ { "ComponentStream", obj } });
      }
      return ret;
    }
  protected:
    const Component *m_component;
    vector<CategoryRef> m_categories;
    CategoryRef *m_categoryRef;
  };
  
  class DeviceRef {
  public:
    DeviceRef(const Device *device) : m_device(device), m_componentRef(nullptr) {}
    DeviceRef(const DeviceRef &other)
    : m_device(other.m_device), m_components(other.m_components) {}
    
    bool isDevice(const Device *device) {
      return device == m_device;
    }
    
    bool addObservation(const ComponentEventPtr &observation, const Device *device,
                        const Component *component, const DataItem *dataItem)
    {
      if (m_device == device)
      {
        if (m_componentRef == nullptr || !m_componentRef->isComponent(component))
        {
          m_components.emplace_back(component);
          m_componentRef = &m_components.back();
        }
        
        if (m_componentRef != nullptr)
          return m_componentRef->addObservation(observation, component, dataItem);
      }
      return false;
    }
    
    json toJson() {
      json ret;
      if (m_device != nullptr && !m_components.empty())
      {
        json obj = json::object({
          { "@name", m_device->getName() },
          { "@uuid", m_device->getUuid() }
        });
        json items = json::array();
        for (auto &comp : m_components)
          items.push_back(comp.toJson());
        obj["ComponentStreams"] = items;
        ret = json::object({{ "DeviceStream", obj }});
      }
      return ret;
    }
    
  protected:
    const Device *m_device;
    vector<ComponentRef> m_components;
    ComponentRef *m_componentRef;
  };

  std::string JsonPrinter::printSample(
                                  const unsigned int instanceId,
                                  const unsigned int bufferSize,
                                  const uint64_t nextSeq,
                                  const uint64_t firstSeq,
                                  const uint64_t lastSeq,
                                  ComponentEventPtrArray &observations
                                  ) const
  {
    json streams = json::array();
    
    if (observations.size() > 0)
    {
      dlib::qsort_array<ComponentEventPtrArray,
      EventComparer>(observations, 0ul, observations.size() - 1ul,
                     EventCompare);
      
      vector<DeviceRef> devices;
      DeviceRef *deviceRef = nullptr;
      
      for (auto &observation : observations)
      {
        const auto dataItem = observation->getDataItem();
        const auto component = dataItem->getComponent();
        const auto device = component->getDevice();
        
        if (deviceRef == nullptr || !deviceRef->isDevice(device))
        {
          devices.emplace_back(device);
          deviceRef = &devices.back();
        }
        
        deviceRef->addObservation(observation, device, component, dataItem);
      }
      
      for (auto &ref : devices)
        streams.push_back(ref.toJson());
    }
    
    json doc = json::object({ { "MTConnectStreams", {
      { "Header",
        streamHeader(m_version, hostname(), instanceId,
                         bufferSize, nextSeq, firstSeq, lastSeq) },
      { "Streams", streams }
    } } });
    
    return print(doc, m_pretty);

    return print(doc, m_pretty);
  }
  
  inline static json split(const string &v, const char s = ',')
  {
    json array = json::array();
    stringstream ss(v);
    while (ss.good())
    {
      string ele;
      getline(ss, ele, s);
      if (!ele.empty())
        array.push_back(ele);
    }
           
    return array;
  }
  
  inline static void addIdentity(json &obj, Asset *asset)
  {
    auto &identity = asset->getIdentity();
    for (auto &key : identity) {
      obj[string("@") + key.first] = key.second;
    }
  }

  inline static void addCuttingToolIdentity(json &obj, const AssetKeys &identity)
  {
    for (auto &key : identity) {
      if (key.first == "manufacturers") {
        obj["@manufacturers"] = split(key.second);
      } else
        obj[string("@") + key.first] = key.second;
    }
  }
  
  static set<string> IntegerKeys = {
    "negativeOverlap", "positiveOverlap", "Location",
    "maximumCount", "ReconditionCount", "significantDigits",
    "ToolLife"
  };
  
  static set<string> DoubleKeys = {
    "maximum", "minimum", "nominal",
    "ProcessSpindleSpeed", "ProcessFeedRate",
    "Measurement"
  };
  
  inline static void addValue(json &obj, const string &key, const string &value,
                              bool attr)
  {
    string name = attr ? "@" + key : "Value";
    if (IntegerKeys.count(key) > 0)
      obj[name] = atoi(value.c_str());
    else if (DoubleKeys.count(key) > 0) {
      char *ep;
      obj[name] = strtod(value.c_str(), &ep);
    } else {
      obj[name] = value;
    }
  }
  
  inline static json toJson(CuttingToolValuePtr &value,
                            const std::string &kind = "")
  {
    json obj = json::object();
    string type = kind.empty() ? value->m_key : kind;
    for (auto &p : value->m_properties)
      addValue(obj, p.first, p.second, true);
    if (!value->m_value.empty())
      addValue(obj, type, value->m_value, false);
    return obj;
  }
  
  inline static json toJson(CuttingItemPtr &item)
  {
    json obj = json::object();
    
    addCuttingToolIdentity(obj, item->m_identity);
    
    for (auto &prop : item->m_values)
      obj[prop.first] = toJson(prop.second);
    
    if (!item->m_lives.empty())
    {
      json lives = json::array();
      for (auto &s : item->m_lives) {
        lives.push_back(toJson(s));
      }
      obj["ItemLife"] = lives;
    }
    
    if (!item->m_measurements.empty())
    {
      json measurements = json::array();
      for (auto &m : item->m_measurements) {
        measurements.push_back(json::object({
          { m.first, toJson(m.second, "Measurement") }
        }));
      }
      
      obj["Measurements"] = measurements;
    }
    
    return json::object({ { "CuttingItem", obj } });
  }
  
  inline static json toJson(CuttingTool *tool)
  {
    json obj = json::object();
    if (!tool->m_status.empty())
    {
      json status = json::array();
      for (auto &s : tool->m_status)
        status.push_back(s);
      obj["CutterStatus"] = status;
    }
    
    if (!tool->m_lives.empty())
    {
      json lives = json::array();
      for (auto &s : tool->m_lives) {
        lives.push_back(toJson(s));
      }
      obj["ToolLife"] = lives;
    }

    for (auto &prop : tool->m_values) {
      if (prop.first != "CuttingToolDefinition")
        obj[prop.first] = toJson(prop.second);
    }
    
    if (!tool->m_measurements.empty())
    {
      json measurements = json::array();
      for (auto &m : tool->m_measurements) {
        measurements.push_back(json::object({
          { m.first, toJson(m.second, "Measurement") }
        }));
      }
      
      obj["Measurements"] = measurements;
    }
    
    if (!tool->m_items.empty())
    {
      json items = json::array();
      for (auto &item : tool->m_items)
        items.push_back(toJson(item));
      
      obj["CuttingItems"] = items;
    }
    
    return obj;
  }

  inline static json toJson(AssetPtr asset)
  {
    json obj = json::object({
      { "@assetId", asset->getAssetId() },
      { "@timestamp", asset->getTimestamp() }
     });
    
    if (!asset->getDeviceUuid().empty())
      obj["@deviceUuid"] = asset->getDeviceUuid();
    
    if (asset->getType() == "CuttingTool" ||
        asset->getType() == "CuttingToolArchetype")
    {
      CuttingTool *tool = dynamic_cast<CuttingTool*>(asset.getObject());
      addCuttingToolIdentity(obj, tool->getIdentity());
      obj["Description"] = tool->getDescription();

      if (tool->m_values.count("CuttingToolDefinition") > 0) {
        auto &def = tool->m_values["CuttingToolDefinition"];
        obj["CuttingToolDefinition"] = json::object({
          { "@format", def->m_properties["format"] },
          { "#text", def->m_value }
        });
      }
      auto life = toJson(tool);
      if (!life.empty())
        obj["CuttingToolLifeCycle"] = toJson(tool);
    } else {
      addIdentity(obj, asset.getObject());
      obj["#text"] = asset->getContent(nullptr);
    }
    
    json doc = json::object({
      { asset->getType(), obj }
    });
    
    return doc;
  }
  
  std::string JsonPrinter::printAssets(
                                  const unsigned int instanceId,
                                  const unsigned int bufferSize,
                                  const unsigned int assetCount,
                                  std::vector<AssetPtr> const &assets) const
  {
    json assetDoc = json::array();
    for (const auto asset : assets)
      assetDoc.push_back(toJson(asset));
    
    json doc = json::object({ { "MTConnectAssets", {
      { "Header",
        probeAssetHeader(m_version, hostname(), instanceId,
                         0, bufferSize, assetCount) },
      { "Assets", assetDoc }
    } } });
    
    return print(doc, m_pretty);
  }
  
  std::string JsonPrinter::printCuttingTool(CuttingToolPtr const tool) const
  {
    AssetPtr asset(tool);
    return toJson(asset);
  }
}

