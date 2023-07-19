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

#include "response_document.hpp"

#include <date/date.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/entity/data_set.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"

using namespace std;

namespace mtconnect::pipeline {
  using namespace mtconnect;
  using namespace entity;

  inline bool eachElement(xmlNodePtr node, std::function<bool(xmlNodePtr)> cb)
  {
    for (auto child = node->children; child != nullptr; child = child->next)
    {
      if (child->type == XML_ELEMENT_NODE)
      {
        if (!cb(child))
          return true;
      }
    }

    return false;
  }

  inline bool eachElement(xmlNodePtr node, const char *name, std::function<bool(xmlNodePtr)> cb)
  {
    for (auto child = node->children; child != nullptr; child = child->next)
    {
      if (child->type == XML_ELEMENT_NODE && xmlStrcmp(BAD_CAST name, child->name) == 0)
      {
        if (!cb(child))
          return true;
      }
    }

    return false;
  }

  inline bool eachAttribute(xmlNodePtr node, std::function<bool(xmlAttrPtr)> cb)
  {
    for (auto attr = node->properties; attr != nullptr; attr = attr->next)
    {
      if (attr->type == XML_ATTRIBUTE_NODE)
      {
        if (!cb(attr))
          return true;
      }
    }

    return false;
  }

  inline string attributeValue(xmlNodePtr node, const char *name, bool optional = false)
  {
    string res;
    if (!eachAttribute(node, [&name, &res](xmlAttrPtr attr) {
          if (xmlStrcmp(BAD_CAST name, attr->name) == 0)
          {
            res = (const char *)(attr->children->content);
            return false;
          }
          return true;
        }))
    {
      if (!optional)
        LOG(debug) << "Cannot find attribute " << name << " in resonse doc";
    }

    return res;
  }

  inline static xmlNodePtr findChild(xmlNodePtr node, const char *name, bool optional = false)
  {
    xmlNodePtr child;
    if (!eachElement(node, name, [&child](xmlNodePtr node) {
          child = node;
          return false;
        }))
    {
      if (!optional)
        LOG(debug) << "Cannot find element " << name << " in resonse doc";
      return nullptr;
    }

    return child;
  }

  static inline bool parseHeader(ResponseDocument &out, xmlNodePtr root)
  {
    auto header = findChild(root, "Header");
    if (header)
    {
      out.m_instanceId =
          boost::lexical_cast<SequenceNumber_t>(attributeValue(header, "instanceId"));

      if (xmlStrcmp(root->name, BAD_CAST "MTConnectStreams") == 0)
      {
        auto next = attributeValue(header, "nextSequence", false);
        if (!next.empty())
          out.m_next = boost::lexical_cast<SequenceNumber_t>(next);
      }
      return true;
    }

    LOG(error) << "Received incorred document: " << (const char *)root->name;

    return false;
  }

  const string text(xmlNodePtr node)
  {
    for (auto n = node->children; n != nullptr; n = n->next)
    {
      if (n->type == XML_TEXT_NODE)
      {
        return trim((const char *)n->content);
      }
    }

    return "";
  }

  inline static bool parseDevices(ResponseDocument &out, xmlNodePtr node,
                                  pipeline::PipelineContextPtr context,
                                  const std::optional<std::string> &device,
                                  const std::optional<std::string> &uuid)
  {
    using namespace entity;
    using namespace device_model;

    auto header = findChild(node, "Header");
    if (header)
    {
      out.m_instanceId =
          boost::lexical_cast<SequenceNumber_t>(attributeValue(header, "instanceId"));
      out.m_agentVersion = IntSchemaVersion(attributeValue(header, "version"));
    }

    auto devices = findChild(node, "Devices");
    if (devices == nullptr)
    {
      LOG(warning) << "Cannot find Devices node in MTConnectDevices Document";
      return false;
    }

    entity::XmlParser parser;
    eachElement(devices, [&out, &parser, &device, &uuid](xmlNodePtr n) {
      ErrorList errors;
      auto dev = parser.parseXmlNode(Device::getRoot(), n, errors);
      if (!errors.empty())
      {
        LOG(warning) << "Could not parse asset: " << (const char *)n->name;
        for (auto &e : errors)
        {
          LOG(warning) << "    Message: " << e->what();
        }
      }

      auto devicePtr = dynamic_pointer_cast<device_model::Device>(dev);
      if (!devicePtr)
      {
        LOG(error) << "Device could not be parsed from XML";
        return false;
      }

      if (device && *device != *(devicePtr->getComponentName()))
      {
        LOG(warning) << "Source and Target Device Name mismatch: " << *device << " and "
                     << *(devicePtr->getComponentName());
        LOG(warning) << "Setting device name to " << *device;

        devicePtr->setComponentName(*device);
      }
      if (uuid && *uuid != *(devicePtr->getUuid()))
      {
        LOG(warning) << "Source and Target Device uuid mismatch: " << *uuid << " and "
                     << *(devicePtr->getUuid());
        LOG(warning) << "Setting device uuid to " << *uuid;
        devicePtr->setUuid(*uuid);
      }

      out.m_entities.emplace_back(dev);

      return true;
    });

    return true;
  }

  inline DataSetValue type(const string &s)
  {
    using namespace boost;
    if (s.empty())
      return std::monostate();

    bool dbl = false;
    for (const char c : s)
    {
      if (!isdigit(c) && c != '.')
      {
        return s;
      }
      else if (c == '.' && dbl)
      {
        return s;
      }
      else if (c == '.')
      {
        dbl = true;
      }
    }

    if (dbl)
      return lexical_cast<double>(s);
    else
      return lexical_cast<int64_t>(s);
  }

  inline void dataSet(xmlNodePtr node, bool table, DataSet &ds)
  {
    eachElement(node, "Entry", [table, &ds](xmlNodePtr n) {
      DataSetEntry entry;
      entry.m_key = attributeValue(n, "key");
      entry.m_removed = attributeValue(n, "removed", true) == "true";

      if (table)
      {
        entry.m_value.emplace<DataSet>();
        DataSet &row = get<DataSet>(entry.m_value);

        eachElement(n, "Cell", [&row](xmlNodePtr c) {
          row.emplace(attributeValue(c, "key"), type(text(c)));
          return true;
        });

        if (row.empty())
          entry.m_value.emplace<monostate>();
      }
      else
      {
        entry.m_value = type(text(n));
      }

      ds.insert(entry);
      return true;
    });
  }

  inline static Timestamp parseTimestamp(const std::string value)
  {
    Timestamp ts;
    istringstream in(value);
    in >> std::setw(6) >> date::parse("%FT%T", ts);
    if (!in.good())
    {
      LOG(error) << "Cound not parse XML timestamp: " << value;
      ts = std::chrono::system_clock::now();
    }

    return ts;
  }

  inline static DataItemPtr findDataItem(const std::string &name, DevicePtr device,
                                         const entity::Properties &properties)
  {
    auto id = properties.find("dataItemId");
    if (id == properties.end())
    {
      const string &uuid = *(device->getUuid());
      LOG(warning) << "Device: " << uuid << ": Cannot find dataItemId for " << name;
      return nullptr;
    }

    auto di = device->getDeviceDataItem(get<std::string>(id->second));
    if (!di)
    {
      auto diName = properties.find("name");
      if (diName == properties.end())
      {
        const string &uuid = *(device->getUuid());
        LOG(warning) << "Device: " << uuid
                     << ": Cannot data item for id and no name:" << get<std::string>(id->second);
        return nullptr;
      }
      di = device->getDeviceDataItem(get<std::string>(diName->second));
      if (!di)
      {
        const string &uuid = *(device->getUuid());
        LOG(warning) << "Device: " << uuid << ": Cannot data item for id "
                     << get<std::string>(id->second)
                     << " or name:" << get<std::string>(diName->second);
        return nullptr;
      }
    }

    return di;
  }

  inline static bool parseObservations(ResponseDocument &out, xmlNodePtr node,
                                       pipeline::PipelineContextPtr context,
                                       const std::optional<std::string> &deviceName)
  {
    auto streams = findChild(node, "Streams");
    if (streams == nullptr)
      return false;

    auto contract = context->m_contract.get();

    eachElement(streams, "DeviceStream", [&out, &contract, &deviceName](xmlNodePtr dev) {
      DevicePtr device;
      if (deviceName)
      {
        device = contract->findDevice(*deviceName);
        if (!device)
        {
          LOG(warning) << "Parsing XML document: cannot find device by uuid: " << *deviceName
                       << ", skipping device";
          return true;
        }
      }
      else
      {
        auto uuid = attributeValue(dev, "uuid");
        device = contract->findDevice(uuid);
        if (!device)
        {
          LOG(warning) << "Parsing XML document: cannot find device by uuid: " << uuid
                       << ", skipping device";
          return true;
        }
      }

      eachElement(dev, "ComponentStream", [&out, &device](xmlNodePtr comp) {
        eachElement(comp, [&out, &device](xmlNodePtr org) {
          eachElement(org, [&out, &device](xmlNodePtr o) {
            Properties properties;

            eachAttribute(o, [&properties](xmlAttrPtr attr) {
              if (xmlStrcmp(BAD_CAST "sequence", attr->name) != 0)
              {
                string s((const char *)attr->children->content);
                properties.insert({(const char *)attr->name, s});
              }

              return true;
            });

            // Check for table or data set
            string name((const char *)o->name);
            auto di = findDataItem(name, device, properties);
            if (!di)
            {
              return true;
            }

            // Remove old properties
            properties.erase("name");
            properties.erase("dataItemId");

            auto ts = properties["timestamp"];
            auto timestamp = parseTimestamp(get<string>(ts));

            auto val = text(o);
            if (val == "UNAVAILABLE" || (!di->isDataSet() && !di->isAssetRemoved()))
            {
              properties.insert({"VALUE", val});
            }
            else if (di->isAssetRemoved())
            {
              auto ac = make_shared<pipeline::AssetCommand>(
                  "AssetCommand", Properties {{"assetId"s, val},
                                              {"device"s, *(device->getUuid())},
                                              {"VALUE"s, "RemoveAsset"s}});
              out.m_entities.emplace_back(ac);
              return true;
            }
            else  // isDataSet
            {
              Value &v = properties["VALUE"];
              v.emplace<DataSet>();
              DataSet &ds = get<DataSet>(v);
              dataSet(o, di->isTable(), ds);
            }

            ErrorList errors;
            auto obs = observation::Observation::make(di, properties, timestamp, errors);
            if (!errors.empty())
            {
              for (auto &e : errors)
              {
                LOG(warning) << "Error while parsing XML: " << e->what();
              }
              return true;
            }

            if (di->isAssetChanged())
              out.m_assetEvents.emplace_back((obs));
            else
              out.m_entities.emplace_back(obs);
            return true;
          });
          return true;
        });

        return true;
      });

      return true;
    });

    return true;
  }

  inline static bool parseAssets(ResponseDocument &out, xmlNodePtr node,
                                 const std::optional<std::string> &device)
  {
    using namespace entity;
    using namespace asset;

    auto assets = findChild(node, "Assets");
    if (assets == nullptr)
      return false;

    entity::XmlParser parser;
    eachElement(assets, [&out, &parser](xmlNodePtr n) {
      ErrorList errors;
      auto res = parser.parseXmlNode(Asset::getRoot(), n, errors);
      if (!errors.empty())
      {
        LOG(warning) << "Could not parse asset: " << (const char *)n->name;
        for (auto &e : errors)
        {
          LOG(warning) << "    Message: " << e->what();
        }
      }

      out.m_entities.emplace_back(res);

      return true;
    });

    return true;
  }

  inline static void parseErrors(ResponseDocument &out, xmlNodePtr node)
  {
    auto errors = findChild(node, "Errors");
    if (errors == nullptr)
    {
      auto error = findChild(node, "Error");
      if (error)
      {
        auto code = attributeValue(error, "errorCode");
        auto msg = text(error);
        out.m_errors.emplace_back(ResponseDocument::Error {code, msg});

        LOG(error) << "Received protocol error: " << code << " " << msg;
      }
    }
    else
    {
      eachElement(errors, "Error", [&out](xmlNodePtr error) {
        auto code = attributeValue(error, "errorCode");
        auto msg = text(error);
        out.m_errors.emplace_back(ResponseDocument::Error {code, msg});

        LOG(error) << "Received protocol error: " << code << " " << msg;

        return true;
      });
    }
  }

  bool ResponseDocument::parse(const std::string_view &content, ResponseDocument &out,
                               pipeline::PipelineContextPtr context,
                               const std::optional<std::string> &device,
                               const std::optional<std::string> &uuid)
  {
    // xmlInitParser();
    // xmlXPathInit();
    unique_ptr<xmlDoc, function<void(xmlDocPtr)>> doc(
        xmlReadMemory(content.data(), static_cast<int>(content.length()), "incoming.xml", nullptr,
                      XML_PARSE_NOBLANKS),
        [](xmlDocPtr d) { xmlFreeDoc(d); });
    xmlNodePtr root = xmlDocGetRootElement(doc.get());
    if (root != nullptr)
    {
      if (!parseHeader(out, root))
      {
        LOG(error) << "Cannot find next in header for streams doc";
        return false;
      }
      if (xmlStrcmp(BAD_CAST "MTConnectStreams", root->name) == 0)
      {
        return parseObservations(out, root, context, device);
      }
      else if (xmlStrcmp(BAD_CAST "MTConnectDevices", root->name) == 0)
      {
        return parseDevices(out, root, context, device, uuid);
      }
      else if (xmlStrcmp(BAD_CAST "MTConnectAssets", root->name) == 0)
      {
        return parseAssets(out, root, device);
      }
      else if (xmlStrcmp(BAD_CAST "MTConnectError", root->name) == 0)
      {
        parseErrors(out, root);
        return false;
      }
      else
      {
        LOG(error) << "Unknown document type: " << (const char *)root->name;
        return false;
      }
    }
    else
    {
      return false;
    }
  }
}  // namespace mtconnect::pipeline
