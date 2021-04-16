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

#include "device_model/composition.hpp"
#include "device_model/coordinate_systems.hpp"
#include "device_model/device.hpp"
#include "device_model/relationships.hpp"
#include "device_model/sensor_configuration.hpp"
#include "device_model/solid_model.hpp"
#include "device_model/specifications.hpp"
#include "entity/json_printer.hpp"
#include "version.h"

#include <dlib/logger.h>
#include <dlib/sockets.h>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <set>
#include <sstream>

using namespace std;
using json = nlohmann::json;

namespace mtconnect
{
  using namespace observation;

  static dlib::logger g_logger("json.printer");

  JsonPrinter::JsonPrinter(const string version, bool pretty)
    : Printer(pretty), m_schemaVersion(version)
  {
    char appVersion[32] = {0};
    std::sprintf(appVersion, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR,
                 AGENT_VERSION_PATCH, AGENT_VERSION_BUILD);
    m_version = appVersion;
  }

  static inline std::string ltrim(std::string s)
  {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
    return s;
  }

  // trim from end (in place)
  static inline std::string rtrim(std::string s)
  {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(),
            s.end());
    return s;
  }

  // trim from both ends (in place)
  static inline std::string trim(std::string s) { return rtrim(ltrim(s)); }

  const string &JsonPrinter::hostname() const
  {
    if (m_hostname.empty())
    {
      string name;
      if (dlib::get_local_hostname(name))
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

  static inline void addAttributes(json &doc, const map<string, string> &attrs)
  {
    for (const auto &attr : attrs)
    {
      if (!attr.second.empty())
        doc[attr.first] = attr.second;
    }
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

  static inline void addText(json &doc, const std::string &text) { add(doc, "text", trim(text)); }

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

  static inline json toJson(const set<CellDefinition> &definitions)
  {
    json entries = json::object();

    for (const auto &entry : definitions)
    {
      json e = json::object();
      addAttributes(e, {{"Description", entry.m_description},
                        {"units", entry.m_units},
                        {"type", entry.m_type},
                        {"keyType", entry.m_keyType},
                        {"subType", entry.m_subType}});

      if (!entry.m_key.empty())
        entries[entry.m_key] = e;
      else
        entries["."] = e;
    }

    return entries;
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

  static inline json toJson(const DataItemDefinition &definition)
  {
    json obj = json::object();
    if (!definition.m_description.empty())
      obj["Description"] = definition.m_description;

    if (!definition.m_entries.empty())
    {
      json entries = json::object();
      for (const auto &entry : definition.m_entries)
      {
        json e = json::object();
        addAttributes(e, {{"Description", entry.m_description},
                          {"units", entry.m_units},
                          {"type", entry.m_type},
                          {"keyType", entry.m_keyType},
                          {"subType", entry.m_subType}});

        if (!entry.m_cells.empty())
        {
          e["CellDefinitions"] = toJson(entry.m_cells);
        }

        if (!entry.m_key.empty())
          entries[entry.m_key] = e;
        else
          entries["."] = e;
      }

      obj["EntryDefinitions"] = entries;
    }

    if (!definition.m_cells.empty())
    {
      obj["CellDefinitions"] = toJson(definition.m_cells);
    }

    return obj;
  }

  static inline json toJson(const list<DataItem::Relationship> &relations)
  {
    json array = json::array();
    for (const auto &rel : relations)
    {
      json obj = json::object();
      add(obj, "name", rel.m_name);
      obj["type"] = rel.m_type;
      obj["idRef"] = rel.m_idRef;

      array.emplace_back(json::object({{rel.m_relation, obj}}));
    }

    return array;
  }

  static inline json toJson(DataItem *item)
  {
    json obj = json::object();
    addAttributes(obj, item->getAttributes());

    // Data Item Source
    json source = json::object();
    addText(source, item->getSource());
    add(source, "dataItemId", item->getSourceDataItemId());
    add(source, "componentId", item->getSourceComponentId());
    add(source, "compositionId", item->getSourceCompositionId());

    if (source.begin() != source.end())
      obj["Source"] = source;

    // Data Item Constraints
    if (item->hasConstraints())
    {
      json constraints = json::object();
      addDouble(constraints, "Maximum", item->getMaximum());
      addDouble(constraints, "Minimum", item->getMinimum());

      if (!item->getConstrainedValues().empty())
      {
        json values(item->getConstrainedValues());
        constraints["value"] = values;
      }

      obj["Constraints"] = constraints;
    }

    if (item->hasMinimumDelta() or item->hasMinimumPeriod())
    {
      json filters = json::array();
      if (item->hasMinimumDelta())
        filters.emplace_back(json::object(
            {{"Filter", {{"value", item->getFilterValue()}, {"type", "MINIMUM_DELTA"}}}}));
      if (item->hasMinimumPeriod())
        filters.emplace_back(
            json::object({{"Filter", {{"value", item->getFilterPeriod()}, {"type", "PERIOD"}}}}));
      obj["Filters"] = filters;
    }

    if (item->hasInitialValue())
    {
      char *ep;
      obj["InitialValue"] = strtod(item->getInitialValue().c_str(), &ep);
    }

    if (item->hasDefinition())
    {
      obj["Definition"] = toJson(item->getDefinition());
    }

    if (item->getRelationships().size() > 0)
    {
      obj["Relationships"] = toJson(item->getRelationships());
    }

    json dataItem = json::object({{"DataItem", obj}});

    return dataItem;
  }

  static inline json jsonReference(const Component::Reference &reference)
  {
    json ref = json::object({{"idRef", reference.m_id}});
    add(ref, "name", reference.m_name);
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

  inline void toJson(json &parent, const Geometry &geometry)
  {
    if (geometry.m_location.index() != 0)
    {
      visit(overloaded{[&parent](const Origin &o) {
                         parent["Origin"] = json::array({o.m_x, o.m_y, o.m_z});
                       },
                       [&parent](const Transformation &t) {
                         json trans = json::object();
                         if (t.m_translation)
                         {
                           trans["Translation"] = json::array(
                               {t.m_translation->m_x, t.m_translation->m_y, t.m_translation->m_z});
                           ;
                         }
                         if (t.m_rotation)
                         {
                           trans["Rotation"] = json::array(
                               {t.m_rotation->m_roll, t.m_rotation->m_pitch, t.m_rotation->m_yaw});
                         }
                         parent["Transformation"] = trans;
                       },
                       [](const std::monostate &a) {}},
            geometry.m_location);
    }

    if (geometry.m_scale)
    {
      parent["Scale"] = json::array(
          {geometry.m_scale->m_scaleX, geometry.m_scale->m_scaleY, geometry.m_scale->m_scaleZ});
    }
    if (geometry.m_axis)
    {
      parent["Axis"] =
          json::array({geometry.m_axis->m_x, geometry.m_axis->m_y, geometry.m_axis->m_z});
    }
  }

  inline json toJson(const GeometricConfiguration &model)
  {
    json obj = json::object();
    addAttributes(obj, model.m_attributes);
    if (model.m_geometry)
      toJson(obj, *model.m_geometry);
    if (!model.m_description.empty())
      obj["Description"] = model.m_description;
    return obj;
  }

  inline void toJson(json &parent, const SensorConfiguration::Calibration &cal)
  {
    addAttributes(parent, {{"CalibrationDate", cal.m_date},
                           {"NextCalibrationDate", cal.m_nextDate},
                           {"CalibrationInitials", cal.m_initials}});
  }

  inline json toJson(const Relationship *rel)
  {
    json obj = json::object();
    json fields = json::object();
    addAttributes(fields, {{"id", rel->m_id},
                           {"name", rel->m_name},
                           {"type", rel->m_type},
                           {"criticality", rel->m_criticality}});

    if (auto r = dynamic_cast<const ComponentRelationship *>(rel))
    {
      fields["idRef"] = r->m_idRef;
      obj["ComponentRelationship"] = fields;
    }
    if (auto r = dynamic_cast<const DeviceRelationship *>(rel))
    {
      fields["href"] = r->m_href;
      fields["role"] = r->m_role;
      fields["deviceUuidRef"] = r->m_deviceUuidRef;
      obj["DeviceRelationship"] = fields;
    }

    return obj;
  }

  inline json toJson(const Specification *spec)
  {
    json fields = json::object();
    addAttributes(fields, {{"type", spec->m_type},
                           {"subType", spec->m_subType},
                           {"units", spec->m_units},
                           {"name", spec->m_name},
                           {"coordinateSystemIdRef", spec->m_coordinateSystemIdRef},
                           {"dataItemIdRef", spec->m_dataItemIdRef},
                           {"compositionIdRef", spec->m_compositionIdRef},
                           {"originator", spec->m_originator},
                           {"id", spec->m_id}});

    if (spec->hasGroups())
    {
      const auto &groups = spec->getGroups();
      for (const auto &group : groups)
      {
        json limits = json::object();
        for (const auto &limit : group.second)
          limits[limit.first] = limit.second;
        fields[group.first] = limits;
      }
    }
    else
    {
      const auto group = spec->getLimits();
      if (group)
      {
        for (const auto &limit : *group)
          fields[limit.first] = limit.second;
      }
    }

    json obj = json::object({{spec->getClass(), fields}});
    return obj;
  }

  inline void toJson(json &parent, const ComponentConfiguration *config)
  {
    if (auto obj = dynamic_cast<const SensorConfiguration *>(config))
    {
      json sensor = json::object();
      if (!obj->getFirmwareVersion().empty())
        sensor["FirmwareVersion"] = obj->getFirmwareVersion();
      auto &cal = obj->getCalibration();
      toJson(sensor, cal);

      if (!obj->getChannels().empty())
      {
        json channels = json::array();

        for (auto &channel : obj->getChannels())
        {
          json chan = json::object();
          addAttributes(chan, channel.getAttributes());

          if (!channel.getDescription().empty())
            chan["Description"] = channel.getDescription();

          auto &cal = channel.getCalibration();
          toJson(chan, cal);

          json chanObj = json::object({{"Channel", chan}});
          channels.emplace_back(chanObj);
        }

        sensor["Channels"] = channels;
      }
      parent["SensorConfiguration"] = sensor;
      ;
    }
    else if (auto obj = dynamic_cast<const Relationships *>(config))
    {
      json relationships = json::array();

      for (const auto &rel : obj->getRelationships())
      {
        json jrel = toJson(rel.get());
        relationships.emplace_back(jrel);
      }

      parent["Relationships"] = relationships;
    }
    else if (auto obj = dynamic_cast<const Specifications *>(config))
    {
      json specifications = json::array();

      for (const auto &spec : obj->getSpecifications())
      {
        json jspec = toJson(spec.get());
        specifications.emplace_back(jspec);
      }

      parent["Specifications"] = specifications;
    }
    else if (auto obj = dynamic_cast<const CoordinateSystems *>(config))
    {
      json systems = json::array();

      for (const auto &system : obj->getCoordinateSystems())
      {
        json jsystem = json::object({{"CoordinateSystem", toJson(*system.get())}});
        systems.emplace_back(jsystem);
      }

      parent["CoordinateSystems"] = systems;
    }
    else if (auto obj = dynamic_cast<const ExtendedComponentConfiguration *>(config))
    {
      parent["ExtendedConfiguration"] = obj->getContent();
    }
    else if (auto obj = dynamic_cast<const GeometricConfiguration *>(config))
    {
      parent[obj->klass()] = toJson(*obj);
    }
  }

  static inline json toJson(const unique_ptr<Composition> &composition)
  {
    json obj = json::object();

    addAttributes(obj, composition->m_attributes);
    if (composition->getDescription())
    {
      json desc = json::object();
      addAttributes(desc, composition->getDescription()->m_attributes);
      addText(desc, composition->getDescription()->m_body);
      obj["Description"] = desc;
    }
    if (!composition->getConfiguration().empty())
    {
      json cfg = json::object();

      for (const auto &conf : composition->getConfiguration())
      {
        toJson(cfg, conf.get());
      }
      obj["Configuration"] = cfg;
    }

    json comp = json::object({{"Composition", obj}});

    return comp;
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

    if (!component->getConfiguration().empty())
    {
      json obj = json::object();

      for (const auto &conf : component->getConfiguration())
      {
        toJson(obj, conf.get());
      }
      comp["Configuration"] = obj;
    }
    toJson(comp, "DataItems", component->getDataItems());
    toJson(comp, "Components", component->getChildren());
    toJson(comp, "Compositions", component->getCompositions());
    toJson(comp, "References", component->getReferences());

    json doc = json::object({{component->getClass(), comp}});

    return doc;
  }

  std::string JsonPrinter::printProbe(const unsigned int instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const unsigned int assetBufferSize,
                                      const unsigned int assetCount,
                                      const std::list<Device *> &devices,
                                      const std::map<std::string, int> *count) const
  {
    json devicesDoc = json::array();
    for (const auto device : devices)
      devicesDoc.emplace_back(toJson(device));

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
    ComponentRef(const Component *component) : m_component(component), m_categoryRef(nullptr) {}
    ComponentRef(const ComponentRef &other)
      : m_component(other.m_component), m_categories(other.m_categories), m_categoryRef(nullptr)
    {
    }

    bool isComponent(const Component *component) { return m_component == component; }

    bool addObservation(const ObservationPtr &observation, const Component *component,
                        const DataItem *dataItem)
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
            {{"component", m_component->getClass()}, {"componentId", m_component->getId()}});

        if (!m_component->getName().empty())
          obj["name"] = m_component->getName();

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
    const Component *m_component;
    vector<CategoryRef> m_categories;
    CategoryRef *m_categoryRef;
  };

  class DeviceRef
  {
  public:
    DeviceRef(const Device *device) : m_device(device), m_componentRef(nullptr) {}
    DeviceRef(const DeviceRef &other) : m_device(other.m_device), m_components(other.m_components)
    {
    }

    bool isDevice(const Device *device) { return device == m_device; }

    bool addObservation(const ObservationPtr &observation, const Device *device,
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

    json toJson()
    {
      json ret;
      if (m_device != nullptr && !m_components.empty())
      {
        json obj = json::object({{"name", m_device->getName()}, {"uuid", m_device->getUuid()}});
        json items = json::array();
        for (auto &comp : m_components)
          items.emplace_back(comp.toJson());
        obj["ComponentStreams"] = items;
        ret = json::object({{"DeviceStream", obj}});
      }
      return ret;
    }

  protected:
    const Device *m_device;
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
