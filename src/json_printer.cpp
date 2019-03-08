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

using namespace std;
using json = nlohmann::json;

namespace mtconnect {
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
  
  static inline void addText(json &doc, const std::string &text)
  {
    if (!text.empty()) doc["#text"] = text;
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
              { "#text", errorText }
            }
          }
        }
      } } }
    });
    
    return print(doc, m_pretty);
  }
  
  static inline json printDataItem(DataItem *item)
  {
    json obj = json::object();
    addAttributes(obj, item->getAttributes());
    json dataItem = json::object({ { "DataItem", obj } });
    
    return dataItem;
  }
  
  
  static json printComponent(Component *component)
  {
    json desc = json::object();
    addAttributes(desc, component->getDescription());
    addText(desc, component->getDescriptionBody());
    
    json comp = json::object({ { "Description", desc } });
    addAttributes(comp, component->getAttributes());
    
    // Add data items
    if (component->getDataItems().size() > 0) {
      json items = json::array();
      for (auto &item : component->getDataItems())
        items.push_back(printDataItem(item));

      comp["DataItems"] = items;
    }
    
    // Add sub-components
    if (component->getChildren().size() > 0) {
      json components = json::array();
      for (auto &sub : component->getChildren())
        components.push_back(printComponent(sub));
      comp["Components"] = components;
    }
    
    // Add compositions
    
    // Add references
    
    // Add configuration
    
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
      devicesDoc.push_back(printComponent(device));
    
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

