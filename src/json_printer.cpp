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

#include "json_printer.hpp"

#include <cstdlib>
#include <set>
#include <sstream>

#include "logging.hpp"
#include <boost/asio/ip/host_name.hpp>

#include <nlohmann/json.hpp>

#include "device_model/composition.hpp"
#include "device_model/configuration/configuration.hpp"
#include "device_model/device.hpp"
#include "device_model/reference.hpp"
#include "entity/json_printer.hpp"
#include "version.h"

using namespace std;
using json = nlohmann::json;

namespace mtconnect
{
  using namespace observation;
  using namespace device_model;

  JsonPrinter::JsonPrinter(const string version, bool pretty)
    : Printer(pretty), m_schemaVersion(version)
  {
    BOOST_LOG_NAMED_SCOPE("JsonPrinter::JsonPrinter");
    char appVersion[32] = {0};
    std::sprintf(appVersion, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR,
                 AGENT_VERSION_PATCH, AGENT_VERSION_BUILD);
    m_version = appVersion;
  }

  const string &JsonPrinter::hostname() const
  {
    if (m_hostname.empty())
    {
      string name;
      boost::system::error_code ec;
      name = boost::asio::ip::host_name(ec);
        if (ec)
          name = "localhost";
      // Breaking the rules, this is a one off
      const_cast<JsonPrinter *>(this)->m_hostname = name;
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

  inline json header(const string &version, const string &hostname, const unsigned int instanceId,
                     const unsigned int bufferSize, const string &schemaVersion)
  {
    json doc = json::object({{"version", version},
                             {"creationTime", getCurrentTime(GMT)},
                             {"testIndicator", false},
                             {"instanceId", instanceId},
                             {"sender", hostname},
                             {"schemaVersion", schemaVersion}});
    if (bufferSize > 0)
      doc["bufferSize"] = bufferSize;
    return doc;
  }

  inline json probeAssetHeader(const string &version, const string &hostname,
                               const unsigned int instanceId, const unsigned int bufferSize,
                               const unsigned int assetBufferSize, const unsigned int assetCount,
                               const string &schemaVersion)
  {
    json doc = header(version, hostname, instanceId, bufferSize, schemaVersion);
    doc["assetBufferSize"] = assetBufferSize;
    doc["assetCount"] = assetCount;

    return doc;
  }

  inline json streamHeader(const string &version, const string &hostname,
                           const unsigned int instanceId, const unsigned int bufferSize,
                           const uint64_t nextSequence, const uint64_t firstSequence,
                           const uint64_t lastSequence, const string &schemaVersion)
  {
    json doc = header(version, hostname, instanceId, bufferSize, schemaVersion);
    doc["nextSequence"] = nextSequence;
    doc["lastSequence"] = lastSequence;
    doc["firstSequence"] = firstSequence;
    return doc;
  }

  std::string JsonPrinter::printErrors(const unsigned int instanceId, const unsigned int bufferSize,
                                       const uint64_t nextSeq, const ProtoErrorList &list) const
  {
    json errors = json::array();
    for (auto &e : list)
    {
      string s(e.second);
      errors.push_back({{"Error", {{"errorCode", e.first}, {"value", trim(s)}}

      }});
    }

    json doc = json::object(
        {{"MTConnectError",
          {{"Header", header(m_version, hostname(), instanceId, bufferSize, m_schemaVersion)},
           {"Errors", errors}}}});

    return print(doc, m_pretty);
  }

  template <class T>
  inline void toJson(json &parent, const string &collection, T &list)
  {
    json obj;
    if (!list.empty())
    {
      json items = json::array();
      for (auto &item : list)
        items.emplace_back(toJson(item));

      parent[collection] = items;
    }
  }

  std::string JsonPrinter::printProbe(const unsigned int instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const unsigned int assetBufferSize,
                                      const unsigned int assetCount,
                                      const std::list<DevicePtr> &devices,
                                      const std::map<std::string, int> *count) const
  {
    entity::JsonPrinter printer;

    json devicesDoc = json::array();
    for (const auto &device : devices)
      devicesDoc.emplace_back(printer.print(device));

    json doc =
        json::object({{"MTConnectDevices",
                       {{"Header", probeAssetHeader(m_version, hostname(), instanceId, bufferSize,
                                                    assetBufferSize, assetCount, m_schemaVersion)},
                        {"Devices", devicesDoc}}}});

    return print(doc, m_pretty);
  }

  inline json toJson(const ObservationPtr &observation)
  {
    entity::JsonPrinter printer;
    return printer.print(observation);
  }

  class CategoryRef
  {
  public:
    CategoryRef(const char *cat) : m_category(cat) {}
    CategoryRef(const CategoryRef &other) = default;

    bool addObservation(const ObservationPtr &observation)
    {
      m_events.emplace_back(observation);
      return true;
    }

    bool isCategory(const char *cat) { return m_category == cat; }

    pair<string, json> toJson()
    {
      pair<string, json> ret;
      if (!m_category.empty())
      {
        json items = json::array();
        for (auto &event : m_events)
          items.emplace_back(::mtconnect::toJson(event));

        ret = make_pair(m_category, items);
      }
      return ret;
    }

  protected:
    string m_category;
    vector<ObservationPtr> m_events;
  };

  class ComponentRef
  {
  public:
    ComponentRef(const ComponentPtr component) : m_component(component), m_categoryRef(nullptr) {}
    ComponentRef(const ComponentRef &other)
      : m_component(other.m_component), m_categories(other.m_categories), m_categoryRef(nullptr)
    {
    }

    bool isComponent(const ComponentPtr &component) { return m_component == component; }

    bool addObservation(const ObservationPtr &observation, const ComponentPtr &component,
                        const DataItemPtr dataItem)
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

    json toJson()
    {
      json ret;
      if (m_component != nullptr && !m_categories.empty())
      {
        json obj = json::object(
            {{"component", m_component->getName()}, {"componentId", m_component->getId()}});

        if (m_component->getComponentName())
          obj["name"] = *m_component->getComponentName();

        for (auto &cat : m_categories)
        {
          auto c = cat.toJson();
          if (!c.first.empty())
            obj[c.first] = c.second;
        }

        ret = json::object({{"ComponentStream", obj}});
      }
      return ret;
    }

  protected:
    const ComponentPtr m_component;
    vector<CategoryRef> m_categories;
    CategoryRef *m_categoryRef;
  };

  class DeviceRef
  {
  public:
    DeviceRef(const DevicePtr device) : m_device(device), m_componentRef(nullptr) {}
    DeviceRef(const DeviceRef &other) : m_device(other.m_device), m_components(other.m_components)
    {
    }

    bool isDevice(const DevicePtr device) { return device == m_device; }

    bool addObservation(const ObservationPtr &observation, const DevicePtr device,
                        const ComponentPtr component, const DataItemPtr dataItem)
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

    json toJson()
    {
      json ret;
      if (m_device != nullptr && !m_components.empty())
      {
        json obj =
            json::object({{"name", *m_device->getComponentName()}, {"uuid", *m_device->getUuid()}});
        json items = json::array();
        for (auto &comp : m_components)
          items.emplace_back(comp.toJson());
        obj["ComponentStreams"] = items;
        ret = json::object({{"DeviceStream", obj}});
      }
      return ret;
    }

  protected:
    const DevicePtr m_device;
    vector<ComponentRef> m_components;
    ComponentRef *m_componentRef;
  };

  std::string JsonPrinter::printSample(const unsigned int instanceId, const unsigned int bufferSize,
                                       const uint64_t nextSeq, const uint64_t firstSeq,
                                       const uint64_t lastSeq, ObservationList &observations) const
  {
    json streams = json::array();

    if (observations.size() > 0)
    {
      observations.sort(ObservationCompare);

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
        streams.emplace_back(ref.toJson());
    }

    json doc =
        json::object({{"MTConnectStreams",
                       {{"Header", streamHeader(m_version, hostname(), instanceId, bufferSize,
                                                nextSeq, firstSeq, lastSeq, m_schemaVersion)},
                        {"Streams", streams}}}});

    return print(doc, m_pretty);

    return print(doc, m_pretty);
  }

  std::string JsonPrinter::printAssets(const unsigned int instanceId, const unsigned int bufferSize,
                                       const unsigned int assetCount, const AssetList &assets) const
  {
    entity::JsonPrinter printer;

    json assetDoc = json::array();
    for (const auto &asset : assets)
      assetDoc.emplace_back(printer.print(asset));

    json doc =
        json::object({{"MTConnectAssets",
                       {{"Header", probeAssetHeader(m_version, hostname(), instanceId, 0,
                                                    bufferSize, assetCount, m_schemaVersion)},
                        {"Assets", assetDoc}}}});

    return print(doc, m_pretty);
  }
}  // namespace mtconnect
