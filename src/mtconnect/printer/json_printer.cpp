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

#include "json_printer.hpp"

#include <boost/asio/ip/host_name.hpp>

#include <cstdlib>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "device_model/composition.hpp"
#include "device_model/configuration/configuration.hpp"
#include "device_model/device.hpp"
#include "device_model/reference.hpp"
#include "entity/json_printer.hpp"
#include "logging.hpp"
#include "version.h"

using namespace std;
using json = nlohmann::json;

namespace mtconnect::printer {
  using namespace observation;
  using namespace device_model;

  JsonPrinter::JsonPrinter(uint32_t jsonVersion, bool pretty)
    : Printer(pretty), m_jsonVersion(jsonVersion)
  {
    NAMED_SCOPE("JsonPrinter::JsonPrinter");
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

  inline json header(const string &version, const string &hostname, const uint64_t instanceId,
                     const unsigned int bufferSize, const string &schemaVersion,
                     const string modelChangeTime)
  {
    json doc = json::object({{"version", version},
                             {"creationTime", getCurrentTime(GMT)},
                             {"testIndicator", false},
                             {"instanceId", instanceId},
                             {"sender", hostname},
                             {"schemaVersion", schemaVersion}});

    if (schemaVersion >= "1.7")
    {
      doc["deviceModelChangeTime"] = modelChangeTime;
    }

    if (bufferSize > 0)
      doc["bufferSize"] = bufferSize;
    return doc;
  }

  inline json probeAssetHeader(const string &version, const string &hostname,
                               const uint64_t instanceId, const unsigned int bufferSize,
                               const unsigned int assetBufferSize, const unsigned int assetCount,
                               const string &schemaVersion, const string modelChangeTime)
  {
    json doc = header(version, hostname, instanceId, bufferSize, schemaVersion, modelChangeTime);
    doc["assetBufferSize"] = assetBufferSize;
    doc["assetCount"] = assetCount;

    return doc;
  }

  inline json streamHeader(const string &version, const string &hostname, const uint64_t instanceId,
                           const unsigned int bufferSize, const uint64_t nextSequence,
                           const uint64_t firstSequence, const uint64_t lastSequence,
                           const string &schemaVersion, const string modelChangeTime)
  {
    json doc = header(version, hostname, instanceId, bufferSize, schemaVersion, modelChangeTime);
    doc["nextSequence"] = nextSequence;
    doc["lastSequence"] = lastSequence;
    doc["firstSequence"] = firstSequence;
    return doc;
  }

  std::string JsonPrinter::printErrors(const uint64_t instanceId, const unsigned int bufferSize,
                                       const uint64_t nextSeq, const ProtoErrorList &list) const
  {
    defaultSchemaVersion();

    json errors = json::array();
    for (auto &e : list)
    {
      string s(e.second);
      errors.push_back({{"Error", {{"errorCode", e.first}, {"value", trim(s)}}

      }});
    }

    json doc = json::object({{"MTConnectError",
                              {{"jsonVersion", m_jsonVersion},
                               {"Header", header(m_version, hostname(), instanceId, bufferSize,
                                                 *m_schemaVersion, m_modelChangeTime)},
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

  std::string JsonPrinter::printProbe(const uint64_t instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const unsigned int assetBufferSize,
                                      const unsigned int assetCount,
                                      const std::list<DevicePtr> &devices,
                                      const std::map<std::string, size_t> *count) const
  {
    defaultSchemaVersion();

    entity::JsonPrinter printer(m_jsonVersion);

    json devicesDoc;

    if (m_jsonVersion == 1)
    {
      devicesDoc = json::array();
      for (const auto &device : devices)
        devicesDoc.emplace_back(printer.print(device));
    }
    else if (m_jsonVersion == 2)
    {
      entity::EntityList list;
      copy(devices.begin(), devices.end(), back_inserter(list));
      entity::EntityPtr entity = make_shared<entity::Entity>("LIST");
      entity->setProperty("LIST", list);
      devicesDoc = printer.printEntity(entity);
    }
    else
    {
      throw runtime_error("invalid json printer version");
    }

    json doc = json::object({{"MTConnectDevices",
                              {{"jsonVersion", m_jsonVersion},
                               {"Header", probeAssetHeader(m_version, hostname(), instanceId,
                                                           bufferSize, assetBufferSize, assetCount,
                                                           *m_schemaVersion, m_modelChangeTime)},
                               {"Devices", devicesDoc}}}});

    return print(doc, m_pretty);
  }

  class CategoryRef
  {
  public:
    CategoryRef(const char *cat, uint32_t version) : m_category(cat), m_version(version) {}
    CategoryRef(const CategoryRef &other) = default;

    bool addObservation(const ObservationPtr &observation)
    {
      m_events.emplace_back(observation);
      return true;
    }

    bool isCategory(const char *cat) { return m_category == cat; }

    pair<string_view, json> toJson()
    {
      entity::JsonPrinter printer(m_version);
      pair<string_view, json> ret(m_category, json::object());
      if (!m_category.empty())
      {
        if (m_version == 1)
        {
          ret.second = json::array();
          for (auto &event : m_events)
            ret.second.emplace_back(printer.print(event));
        }
        else if (m_version == 2)
        {
          entity::EntityList list;
          copy(m_events.begin(), m_events.end(), back_inserter(list));
          entity::EntityPtr entity = make_shared<entity::Entity>("LIST");
          entity->setProperty("LIST", list);
          ret.second = printer.printEntity(entity);
        }
      }
      return ret;
    }

  protected:
    string_view m_category;
    list<ObservationPtr> m_events;
    uint32_t m_version;
  };

  class ComponentRef
  {
  public:
    ComponentRef(const ComponentPtr component, uint32_t version)
      : m_component(component), m_version(version)
    {}
    ComponentRef(const ComponentRef &other)
      : m_component(other.m_component), m_categories(other.m_categories), m_version(other.m_version)
    {}

    bool isComponent(const ComponentPtr &component) { return m_component == component; }

    bool addObservation(const ObservationPtr &observation, const ComponentPtr &component,
                        const DataItemPtr dataItem)
    {
      if (m_component == component)
      {
        auto cat = dataItem->getCategoryText();
        if (!m_categoryRef || !(*m_categoryRef)->isCategory(cat))
        {
          m_categories.emplace_back(cat, m_version);
          m_categoryRef.emplace(&m_categories.back());
        }

        if (m_categoryRef)
          return (*m_categoryRef)->addObservation(observation);
      }
      return false;
    }

    json toJson()
    {
      json ret;
      if (m_component && !m_categories.empty())
      {
        json obj = json::object(
            {{"component", m_component->getName()}, {"componentId", m_component->getId()}});

        if (m_component->getComponentName())
          obj["name"] = *m_component->getComponentName();

        for (auto &cat : m_categories)
        {
          auto c = cat.toJson();
          if (!c.first.empty())
            obj[string(c.first)] = c.second;
        }

        if (m_version == 1)
          ret = json::object({{"ComponentStream", obj}});
        else if (m_version == 2)
          ret = obj;
      }
      return ret;
    }

  protected:
    const ComponentPtr m_component;
    vector<CategoryRef> m_categories;
    optional<CategoryRef *> m_categoryRef;
    uint32_t m_version;
  };

  class DeviceRef
  {
  public:
    DeviceRef(const DevicePtr device, uint32_t version) : m_device(device), m_version(version) {}
    DeviceRef(const DeviceRef &other)
      : m_device(other.m_device), m_components(other.m_components), m_version(other.m_version)
    {}

    bool isDevice(const DevicePtr device) { return device == m_device; }

    bool addObservation(const ObservationPtr &observation, const DevicePtr device,
                        const ComponentPtr component, const DataItemPtr dataItem)
    {
      if (m_device == device)
      {
        if (!m_componentRef || !(*m_componentRef)->isComponent(component))
        {
          m_components.emplace_back(component, m_version);
          m_componentRef.emplace(&m_components.back());
        }

        if (m_componentRef)
          return (*m_componentRef)->addObservation(observation, component, dataItem);
      }
      return false;
    }

    json toJson()
    {
      json ret;
      if (m_device && !m_components.empty())
      {
        json obj =
            json::object({{"name", *m_device->getComponentName()}, {"uuid", *m_device->getUuid()}});
        json items = json::array();
        for (auto &comp : m_components)
          items.emplace_back(comp.toJson());

        if (m_version == 1)
        {
          obj["ComponentStreams"] = items;
          ret = json::object({{"DeviceStream", obj}});
        }
        else if (m_version == 2)
        {
          if (m_components.size() == 1)
          {
            ret["ComponentStream"] = items.front();
          }
          else
          {
            ret["ComponentStream"] = items;
          }
        }
        else
        {
          throw runtime_error("Invalid json printer version");
        }
      }
      return ret;
    }

  protected:
    const DevicePtr m_device;
    vector<ComponentRef> m_components;
    optional<ComponentRef *> m_componentRef;
    uint32_t m_version;
  };

  std::string JsonPrinter::printSample(const uint64_t instanceId, const unsigned int bufferSize,
                                       const uint64_t nextSeq, const uint64_t firstSeq,
                                       const uint64_t lastSeq, ObservationList &observations) const
  {
    defaultSchemaVersion();

    json streams;

    if (observations.size() > 0)
    {
      observations.sort(ObservationCompare);

      vector<DeviceRef> devices;
      DeviceRef *deviceRef = nullptr;

      for (auto &observation : observations)
      {
        if (!observation->isOrphan())
        {
          const auto dataItem = observation->getDataItem();
          const auto component = dataItem->getComponent();
          const auto device = component->getDevice();

          if (!deviceRef || !deviceRef->isDevice(device))
          {
            devices.emplace_back(device, m_jsonVersion);
            deviceRef = &devices.back();
          }

          deviceRef->addObservation(observation, device, component, dataItem);
        }
      }

      streams = json::array();
      for (auto &ref : devices)
        streams.emplace_back(ref.toJson());

      if (m_jsonVersion == 2)
      {
        if (devices.size() == 1)
        {
          streams = json::object({{"DeviceStream", streams.front()}});
        }
        else
        {
          streams = json::object({{"DeviceStream", streams}});
        }
      }
    }

    json doc = json::object(
        {{"MTConnectStreams",
          {{"jsonVersion", m_jsonVersion},
           {"Header", streamHeader(m_version, hostname(), instanceId, bufferSize, nextSeq, firstSeq,
                                   lastSeq, *m_schemaVersion, m_modelChangeTime)},
           {"Streams", streams}}}});

    return print(doc, m_pretty);
  }

  std::string JsonPrinter::printAssets(const uint64_t instanceId, const unsigned int bufferSize,
                                       const unsigned int assetCount,
                                       const asset::AssetList &asset) const
  {
    defaultSchemaVersion();

    entity::JsonPrinter printer(m_jsonVersion);
    json assetDoc;
    if (m_jsonVersion == 1)
    {
      assetDoc = json::array();
      for (const auto &asset : asset)
        assetDoc.emplace_back(printer.print(asset));
    }
    else if (m_jsonVersion == 2)
    {
      entity::EntityList list;
      copy(asset.begin(), asset.end(), back_inserter(list));
      entity::EntityPtr entity = make_shared<entity::Entity>("LIST");
      entity->setProperty("LIST", list);
      assetDoc = printer.printEntity(entity);
    }
    else
    {
      throw runtime_error("invalid json printer version");
    }

    json doc = json::object(
        {{"MTConnectAssets",
          {{"jsonVersion", m_jsonVersion},
           {"Header", probeAssetHeader(m_version, hostname(), instanceId, 0, bufferSize, assetCount,
                                       *m_schemaVersion, m_modelChangeTime)},
           {"Assets", assetDoc}}}});

    return print(doc, m_pretty);
  }
}  // namespace mtconnect::printer
