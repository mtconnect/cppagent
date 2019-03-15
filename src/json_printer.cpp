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
        { "@bufferSize", bufferSize },
        { "@sender", hostname }
      });
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
                           const uint64_t lastSequence,
                           const uint64_t firstSequence)
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
            { "#text", item->getFilterValue() },
            { "@type", "MINIMUM_DELTA" }
          }}}));
      if (item->hasMinimumPeriod())
        filters.push_back(json::object({
          { "Filter", {
            { "#text", item->getFilterPeriod() },
            { "@type", "PERIOD" }
          }}}));
      obj["Filters"] = filters;
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
  
  std::string JsonPrinter::printSample(
                                  const unsigned int instanceId,
                                  const unsigned int bufferSize,
                                  const uint64_t nextSeq,
                                  const uint64_t firstSeq,
                                  const uint64_t lastSeq,
                                  ComponentEventPtrArray &results
                                  ) const
  {
    json doc = {"MTConnectStreams", ""};
    return print(doc, m_pretty);
  }
  
  std::string JsonPrinter::printAssets(
                                  const unsigned int anInstanceId,
                                  const unsigned int bufferSize,
                                  const unsigned int assetCount,
                                  std::vector<AssetPtr> const &assets) const
  {
    json doc = {"MTConnectAssets", ""};
    return print(doc, m_pretty);
  }
  
  std::string JsonPrinter::printCuttingTool(CuttingToolPtr const tool) const
  {
    json doc = {"CuttingTool", ""};
    return print(doc, m_pretty);
  }
}

