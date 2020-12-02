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

#include "composition.hpp"
#include "coordinate_systems.hpp"
#include "device.hpp"
#include "relationships.hpp"
#include "sensor_configuration.hpp"
#include "specifications.hpp"
#include "solid_model.hpp"

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
  static inline std::string trim(std::string s)
  {
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

  static inline void addText(json &doc, const std::string &text)
  {
    add(doc, "text", trim(text));
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

  std::string JsonPrinter::printError(const unsigned int instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const std::string &errorCode,
                                      const std::string &errorText) const
  {
    json doc = json::object(
        {{"MTConnectError",
          {{"Header", header(m_version, hostname(), instanceId, bufferSize, m_schemaVersion)},
           {"Errors", {{"Error", {{"errorCode", errorCode}, {"text", trim(errorText)}}}}}}}});

    return print(doc, m_pretty);
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
                        {"subType", entry.m_subType}});

      entries[entry.m_key] = e;
    }

    return entries;
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
                          {"subType", entry.m_subType}});

        if (!entry.m_cells.empty())
        {
          e["CellDefinitions"] = toJson(entry.m_cells);
        }

        entries[entry.m_key] = e;
      }

      obj["EntryDefinitions"] = entries;
    }

    if (!definition.m_cells.empty())
    {
      obj["CellDefinitions"] = toJson(definition.m_cells);
    }

    return obj;
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

    json dataItem = json::object({{"DataItem", obj}});

    return dataItem;
  }

  static inline json toJson(Composition *composition)
  {
    json obj = json::object();

    addAttributes(obj, composition->getAttributes());
    if (composition->getDescription() != nullptr)
    {
      json desc = json::object();
      addAttributes(desc, composition->getDescription()->getAttributes());
      addText(desc, composition->getDescription()->getBody());
      obj["Description"] = desc;
    }

    json comp = json::object({{"Composition", obj}});

    return comp;
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
      visit(overloaded {
        [&parent](const Origin &o) {
          parent["Origin"] = json::array({ o.m_x, o.m_y, o.m_z });
        },
        [&parent](const Transformation &t) {
          json trans = json::object();
          if (t.m_translation)
          {
            trans["Translation"] = json::array({ t.m_translation->m_x,
              t.m_translation->m_y,
              t.m_translation->m_z });
;
          }
          if (t.m_rotation)
          {
            trans["Rotation"] = json::array({ t.m_rotation->m_roll,
              t.m_rotation->m_pitch,
              t.m_rotation->m_yaw });
          }
          parent["Transformation"] = trans;
        },
        [](const std::monostate &a){}
      }, geometry.m_location);
    }
    
    if (geometry.m_scale)
    {
      parent["Scale"] = json::array({ geometry.m_scale->m_scaleX,
        geometry.m_scale->m_scaleY,
        geometry.m_scale->m_scaleZ });
    }
  }
  
  inline json toJson(const SolidModel &model)
  {
    json obj = json::object();
    addAttributes(obj, model.m_attributes);
    if (model.m_geometry)
      toJson(obj, *model.m_geometry);
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
                           {"Maximum", spec->m_maximum},
                           {"Minimum", spec->m_minimum},
                           {"Nominal", spec->m_nominal}});
    json obj = json::object({{"Specification", fields}});
    return obj;
  }

  inline json toJson(const CoordinateSystem *system)
  {
    json fields = json::object();
    addAttributes(fields, {
                              {"id", system->m_id},
                              {"type", system->m_type},
                              {"name", system->m_name},
                              {"nativeName", system->m_nativeName},
                              {"parentIdRef", system->m_parentIdRef},
                          });

    if (!system->m_origin.empty())
    {
      json obj = json::object();
      obj["Origin"] = system->m_origin;
      fields["Transformation"] = obj;
    }
    if (!system->m_translation.empty() || !system->m_rotation.empty())
    {
      json obj = json::object();

      if (!system->m_translation.empty())
        obj["Translation"] = system->m_translation;
      if (!system->m_translation.empty())
        obj["Rotation"] = system->m_rotation;

      fields["Transformation"] = obj;
    }

    json obj = json::object({{"CoordinateSystem", fields}});
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
        json jsystem = toJson(system.get());
        systems.emplace_back(jsystem);
      }

      parent["CoordinateSystems"] = systems;
    }
    else if (auto obj = dynamic_cast<const ExtendedComponentConfiguration *>(config))
    {
      parent["ExtendedConfiguration"] = obj->getContent();
    }
    else if (auto obj = dynamic_cast<const SolidModel *>(config))
    {
      parent["SolidModel"] = toJson(*obj);
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
                                      const std::vector<Device *> &devices,
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

    if (observation->isUnavailable())
    {
      value = observation->getValue();
    }
    else if (observation->isTimeSeries())
    {
      ostringstream ostr;
      ostr.precision(6);
      const auto &v = observation->getTimeSeries();

      value = json::array();

      for (auto &e : v)
        value.emplace_back(e);
    }
    else if (observation->isDataSet())
    {
      value = json::object();

      const DataSet &set = observation->getDataSet();
      for (auto &e : set)
      {
        if (e.m_removed)
        {
          value[e.m_key] = json::object({{"removed", true}});
        }
        else
        {
          visit(overloaded{[&value, &e](const std::string &st) { value[e.m_key] = st; },
                           [&value, &e](const int64_t &i) { value[e.m_key] = i; },
                           [&value, &e](const double &d) { value[e.m_key] = d; },
                           [&value, &e](const DataSet &arg) {
                             auto row = json::object();
                             for (auto &c : arg)
                             {
                               visit(overloaded{
                                         [&row, &c](const std::string &st) { row[c.m_key] = st; },
                                         [&row, &c](const int64_t &i) { row[c.m_key] = i; },
                                         [&row, &c](const double &d) { row[c.m_key] = d; },
                                         [](auto &a) {
                                           g_logger << dlib::LERROR
                                                    << "Invalid  variant type for table cell";
                                         }},
                                     c.m_value);
                             }
                             value[e.m_key] = row;
                           }},
                e.m_value);
        }
      }
    }
    else if (dataItem->getCategory() == DataItem::SAMPLE)
    {
      if (!observation->getValue().empty())
      {
        if (dataItem->is3D())
        {
          value = json::array();
          stringstream s(observation->getValue());
          int i;
          for (i = 0; s && i < 3; i++)
          {
            double v;
            s >> v;
            value.emplace_back(v);
          }
          if (i < 3)
          {
            for (; i < 3; i++)
              value.emplace_back(0.0);
          }
        }
        else
        {
          char *ep;
          value = strtod(observation->getValue().c_str(), &ep);
        }
      }
      else
      {
        value = 0;
      }
    }
    else
    {
      value = observation->getValue();
    }

    json obj = json::object();
    for (const auto &attr : observation->getAttributes())
    {
      if (strcmp(attr.first, "sequence") == 0)
      {
        obj["sequence"] = observation->getSequence();
      }
      else if (strcmp(attr.first, "sampleCount") == 0 or strcmp(attr.first, "sampleRate") == 0 or
               strcmp(attr.first, "duration") == 0)
      {
        char *ep;
        obj[attr.first] = strtod(attr.second.c_str(), &ep);
      }
      else
      {
        obj[attr.first] = attr.second;
      }
    }
    obj["value"] = value;

    return json::object({{name, obj}});
  }

  class CategoryRef
  {
   public:
    CategoryRef(const char *cat) : m_category(cat)
    {
    }
    CategoryRef(const CategoryRef &other) = default;

    bool addObservation(const ObservationPtr &observation)
    {
      m_events.emplace_back(observation);
      return true;
    }

    bool isCategory(const char *cat)
    {
      return m_category == cat;
    }

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
    ComponentRef(const Component *component) : m_component(component), m_categoryRef(nullptr)
    {
    }
    ComponentRef(const ComponentRef &other)
        : m_component(other.m_component), m_categories(other.m_categories), m_categoryRef(nullptr)
    {
    }

    bool isComponent(const Component *component)
    {
      return m_component == component;
    }

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
    DeviceRef(const Device *device) : m_device(device), m_componentRef(nullptr)
    {
    }
    DeviceRef(const DeviceRef &other) : m_device(other.m_device), m_components(other.m_components)
    {
    }

    bool isDevice(const Device *device)
    {
      return device == m_device;
    }

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
                                       const uint64_t lastSeq,
                                       ObservationPtrArray &observations) const
  {
    json streams = json::array();

    if (observations.size() > 0)
    {
      dlib::qsort_array<ObservationPtrArray, ObservationComparer>(
          observations, 0ul, observations.size() - 1ul, ObservationCompare);

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

  inline static json split(const string &v, const char s = ',')
  {
    json array = json::array();
    stringstream ss(v);
    while (ss.good())
    {
      string ele;
      getline(ss, ele, s);
      if (!ele.empty())
        array.emplace_back(ele);
    }

    return array;
  }

  inline static void addIdentity(json &obj, Asset *asset)
  {
    auto &identity = asset->getIdentity();
    for (auto &key : identity)
    {
      obj[key.first] = key.second;
    }
  }

  inline static void addCuttingToolIdentity(json &obj, const AssetKeys &identity)
  {
    for (auto &key : identity)
    {
      if (key.first == "manufacturers")
      {
        obj["manufacturers"] = split(key.second);
      }
      else
        obj[key.first] = key.second;
    }
  }

  static set<string> IntegerKeys = {"negativeOverlap", "positiveOverlap",  "Location",
                                    "maximumCount",    "ReconditionCount", "significantDigits",
                                    "ToolLife"};

  static set<string> DoubleKeys = {
      "maximum", "minimum", "nominal", "ProcessSpindleSpeed", "ProcessFeedRate", "Measurement"};

  inline static void addValue(json &obj, const string &key, const string &value, bool attr)
  {
    string name = attr ? key : "value";
    if (IntegerKeys.count(key) > 0)
      obj[name] = atoi(value.c_str());
    else if (DoubleKeys.count(key) > 0)
    {
      char *ep;
      obj[name] = strtod(value.c_str(), &ep);
    }
    else
    {
      obj[name] = value;
    }
  }

  inline static json toJson(CuttingToolValuePtr &value, const std::string &kind = "")
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
      for (auto &s : item->m_lives)
      {
        lives.emplace_back(toJson(s));
      }
      obj["ItemLife"] = lives;
    }

    if (!item->m_measurements.empty())
    {
      json measurements = json::array();
      for (auto &m : item->m_measurements)
      {
        measurements.emplace_back(json::object({{m.first, toJson(m.second, "Measurement")}}));
      }

      obj["Measurements"] = measurements;
    }

    return json::object({{"CuttingItem", obj}});
  }

  inline static json toJson(CuttingTool *tool)
  {
    json obj = json::object();
    if (!tool->m_status.empty())
    {
      json status = json::array();
      for (auto &s : tool->m_status)
        status.emplace_back(s);
      obj["CutterStatus"] = status;
    }

    if (!tool->m_lives.empty())
    {
      json lives = json::array();
      for (auto &s : tool->m_lives)
      {
        lives.emplace_back(toJson(s));
      }
      obj["ToolLife"] = lives;
    }

    for (auto &prop : tool->m_values)
    {
      if (prop.first != "CuttingToolDefinition")
        obj[prop.first] = toJson(prop.second);
    }

    if (!tool->m_measurements.empty())
    {
      json measurements = json::array();
      for (auto &m : tool->m_measurements)
      {
        measurements.emplace_back(json::object({{m.first, toJson(m.second, "Measurement")}}));
      }

      obj["Measurements"] = measurements;
    }

    if (!tool->m_items.empty())
    {
      json items = json::array();
      for (auto &item : tool->m_items)
        items.emplace_back(toJson(item));

      obj["CuttingItems"] = items;
    }

    return obj;
  }

  inline static json toJson(AssetPtr asset)
  {
    json obj =
        json::object({{"assetId", asset->getAssetId()}, {"timestamp", asset->getTimestamp()}});

    if (!asset->getDeviceUuid().empty())
      obj["deviceUuid"] = asset->getDeviceUuid();

    if (asset->getType() == "CuttingTool" || asset->getType() == "CuttingToolArchetype")
    {
      auto *tool = dynamic_cast<CuttingTool *>(asset.getObject());
      addCuttingToolIdentity(obj, tool->getIdentity());
      obj["Description"] = tool->getDescription();

      if (tool->m_values.count("CuttingToolDefinition") > 0)
      {
        auto &def = tool->m_values["CuttingToolDefinition"];
        obj["CuttingToolDefinition"] =
            json::object({{"format", def->m_properties["format"]}, {"text", def->m_value}});
      }
      auto life = toJson(tool);
      if (!life.empty())
        obj["CuttingToolLifeCycle"] = toJson(tool);
    }
    else
    {
      addIdentity(obj, asset.getObject());
      obj["text"] = asset->getContent(nullptr);
    }

    json doc = json::object({{asset->getType(), obj}});

    return doc;
  }

  std::string JsonPrinter::printAssets(const unsigned int instanceId, const unsigned int bufferSize,
                                       const unsigned int assetCount,
                                       std::vector<AssetPtr> const &assets) const
  {
    json assetDoc = json::array();
    for (const auto asset : assets)
      assetDoc.emplace_back(toJson(asset));

    json doc =
        json::object({{"MTConnectAssets",
                       {{"Header", probeAssetHeader(m_version, hostname(), instanceId, 0,
                                                    bufferSize, assetCount, m_schemaVersion)},
                        {"Assets", assetDoc}}}});

    return print(doc, m_pretty);
  }

  std::string JsonPrinter::printCuttingTool(CuttingToolPtr const tool) const
  {
    AssetPtr asset(tool);
    return toJson(asset);
  }
}  // namespace mtconnect
