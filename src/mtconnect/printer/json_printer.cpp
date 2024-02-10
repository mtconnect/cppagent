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
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/range/algorithm/sort.hpp>

#include <cstdlib>
#include <set>
#include <sstream>

#include "mtconnect/device_model/composition.hpp"
#include "mtconnect/device_model/configuration/configuration.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/device_model/reference.hpp"
#include "mtconnect/entity/json_printer.hpp"
#include "mtconnect/logging.hpp"
#include "mtconnect/printer/json_printer_helper.hpp"
#include "mtconnect/version.h"

using namespace std;

namespace mtconnect::printer {
  using namespace observation;
  using namespace device_model;
  using namespace rapidjson;

  JsonPrinter::JsonPrinter(uint32_t jsonVersion, bool pretty)
    : Printer(pretty), m_jsonVersion(jsonVersion)
  {
    NAMED_SCOPE("JsonPrinter::JsonPrinter");
    char appVersion[32] = {0};
    std::sprintf(appVersion, "%d.%d.%d.%d", AGENT_VERSION_MAJOR, AGENT_VERSION_MINOR,
                 AGENT_VERSION_PATCH, AGENT_VERSION_BUILD);
    m_version = appVersion;
  }

  template <typename T>
  inline void header(AutoJsonObject<T> &obj, const string &version, const string &hostname,
                     const uint64_t instanceId, const unsigned int bufferSize,
                     const string &schemaVersion, const string modelChangeTime)
  {
    obj.AddPairs("version", version, "creationTime", getCurrentTime(GMT), "testIndicator", false,
                 "instanceId", instanceId, "sender", hostname, "schemaVersion", schemaVersion);

    if (schemaVersion >= "1.7")
      obj.AddPairs("deviceModelChangeTime", modelChangeTime);
    if (bufferSize > 0)
      obj.AddPairs("bufferSize", bufferSize);
  }

  template <typename T>
  inline void probeAssetHeader(AutoJsonObject<T> &obj, const string &version,
                               const string &hostname, const uint64_t instanceId,
                               const unsigned int bufferSize, const unsigned int assetBufferSize,
                               const unsigned int assetCount, const string &schemaVersion,
                               const string modelChangeTime)
  {
    header(obj, version, hostname, instanceId, bufferSize, schemaVersion, modelChangeTime);
    obj.AddPairs("assetBufferSize", assetBufferSize, "assetCount", assetCount);
  }

  template <typename T>
  inline void streamHeader(AutoJsonObject<T> &obj, const string &version, const string &hostname,
                           const uint64_t instanceId, const unsigned int bufferSize,
                           const uint64_t nextSequence, const uint64_t firstSequence,
                           const uint64_t lastSequence, const string &schemaVersion,
                           const string modelChangeTime)
  {
    header(obj, version, hostname, instanceId, bufferSize, schemaVersion, modelChangeTime);
    obj.AddPairs("nextSequence", nextSequence, "lastSequence", lastSequence, "firstSequence",
                 firstSequence);
  }

  template <typename T1, class T2>
  inline void toJson(T1 &writer, const string &collection, T2 &list)
  {
    if (!list.empty())
    {
      AutoJsonArray<T1> ary(writer, collection);
      for (auto &item : list)
      {
        // items.emplace_back(toJson(item));
      }
    }
  }

  std::string JsonPrinter::printErrors(const uint64_t instanceId, const unsigned int bufferSize,
                                       const uint64_t nextSeq, const ProtoErrorList &list,
                                       bool pretty) const
  {
    defaultSchemaVersion();

    StringBuffer output;
    RenderJson(output, m_pretty || pretty, [&](auto &writer) {
      AutoJsonObject obj(writer);
      {
        AutoJsonObject obj(writer, "MTConnectError");
        obj.AddPairs("jsonVersion", m_jsonVersion);

        {
          AutoJsonObject obj(writer, "Header");
          header(obj, m_version, m_senderName, instanceId, bufferSize, *m_schemaVersion,
                 m_modelChangeTime);
        }
        {
          if (m_jsonVersion > 1)
          {
            AutoJsonObject obj(writer, "Errors");
            {
              AutoJsonArray ary(writer, "Error");
              for (auto &e : list)
              {
                AutoJsonObject obj(writer);
                string s(e.second);
                obj.AddPairs("errorCode", e.first, "value", trim(s));
              }
            }
          }
          else
          {
            AutoJsonArray obj(writer, "Errors");
            {
              for (auto &e : list)
              {
                AutoJsonObject obj(writer);
                {
                  AutoJsonObject obj(writer, "Error");
                  string s(e.second);
                  obj.AddPairs("errorCode", e.first, "value", trim(s));
                }
              }
            }
          }
        }
      }
    });

    return string(output.GetString(), output.GetLength());
  }

  std::string JsonPrinter::printProbe(const uint64_t instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const unsigned int assetBufferSize,
                                      const unsigned int assetCount,
                                      const std::list<DevicePtr> &devices,
                                      const std::map<std::string, size_t> *count,
                                      bool includeHidden, bool pretty) const
  {
    defaultSchemaVersion();

    StringBuffer output;
    RenderJson(output, m_pretty || pretty, [&](auto &writer) {
      entity::JsonPrinter printer(writer, m_jsonVersion, includeHidden);

      AutoJsonObject top(writer);
      AutoJsonObject obj(writer, "MTConnectDevices");
      obj.AddPairs("jsonVersion", m_jsonVersion, "schemaVersion", *m_schemaVersion);
      {
        AutoJsonObject obj(writer, "Header");
        probeAssetHeader(obj, m_version, m_senderName, instanceId, bufferSize, assetBufferSize,
                         assetCount, *m_schemaVersion, m_modelChangeTime);
      }
      {
        obj.Key("Devices");
        printer.printEntityList(devices);
      }
    });

    return string(output.GetString(), output.GetLength());
  }

  std::string JsonPrinter::printAssets(const uint64_t instanceId, const unsigned int bufferSize,
                                       const unsigned int assetCount, const asset::AssetList &asset,
                                       bool pretty) const
  {
    defaultSchemaVersion();

    StringBuffer output;
    RenderJson(output, m_pretty || pretty, [&](auto &writer) {
      entity::JsonPrinter printer(writer, m_jsonVersion);

      AutoJsonObject top(writer);
      AutoJsonObject obj(writer, "MTConnectAssets");
      obj.AddPairs("jsonVersion", m_jsonVersion, "schemaVersion", *m_schemaVersion);
      {
        AutoJsonObject obj(writer, "Header");
        probeAssetHeader(obj, m_version, m_senderName, instanceId, 0, bufferSize, assetCount,
                         *m_schemaVersion, m_modelChangeTime);
      }
      {
        obj.Key("Assets");
        printer.printEntityList(asset);
      }
    });
    return string(output.GetString(), output.GetLength());
  }

  using namespace boost;
  using namespace multi_index;
  using namespace device_model::data_item;

  /// @brief A structure used by the multimap to create a composite key for a list of observations.
  ///
  /// Caches the data item, component, category, and device associated with the observation
  struct ObservationRef
  {
    ObservationRef(const ObservationPtr &obs) : m_observation(obs)
    {
      m_dataItem = obs->getDataItem();
      m_category = m_dataItem->getCategory();
      m_component = m_dataItem->getComponent();
      m_device = m_component->getDevice();
    }

    std::string_view getDeviceId() const { return m_device->getId(); }
    std::string_view getComponentId() const { return m_component->getId(); }
    auto getCategory() const { return m_category; }
    auto getSequence() const { return m_observation->getSequence(); }
    std::string_view getType() const { return m_observation->getName().str(); }

    ObservationPtr m_observation;
    ComponentPtr m_component;
    DataItemPtr m_dataItem;
    DevicePtr m_device;
    DataItem::Category m_category;
  };

  using ObservationMap = multi_index_container<
      ObservationRef,
      indexed_by<ordered_non_unique<composite_key<
          ObservationRef,
          const_mem_fun<ObservationRef, std::string_view, &ObservationRef::getDeviceId>,
          const_mem_fun<ObservationRef, std::string_view, &ObservationRef::getComponentId>,
          const_mem_fun<ObservationRef, DataItem::Category, &ObservationRef::getCategory>,
          const_mem_fun<ObservationRef, std::string_view, &ObservationRef::getType>,
          const_mem_fun<ObservationRef, SequenceNumber_t, &ObservationRef::getSequence>>>>>;

  template <typename T>
  void printSampleVersion1(T &writer, uint32_t jsonVersion, ObservationMap &observations)
  {
    using WriterType = decltype(writer);
    using StackType = JsonStack<WriterType>;

    StackType stack(writer);
    entity::JsonPrinter printer(writer, jsonVersion);

    AutoJsonArray streams(writer, "Streams");

    std::string_view deviceId;
    std::string_view componentId;
    int32_t category = -1;

    for (auto &ref : observations)
    {
      if (ref.getDeviceId() != deviceId)
      {
        stack.clear();
        componentId = "";
        category = -1;

        stack.addObject();
        stack.addObject("DeviceStream");

        deviceId = ref.getDeviceId();
        auto device = ref.m_device;
        stack.AddPairs("name", *(device->getComponentName()), "uuid", *(device->getUuid()));
        stack.addArray("ComponentStreams");
      }

      if (ref.getComponentId() != componentId)
      {
        stack.clear(3);
        category = -1;

        stack.addObject();
        stack.addObject("ComponentStream");

        componentId = ref.getComponentId();
        auto component = ref.m_component;
        stack.AddPairs("component", component->getName(), "componentId", component->getId());
        if (component->getComponentName())
          stack.AddPairs("name", *(component->getComponentName()));
      }

      if (ref.getCategory() != category)
      {
        stack.clear(5);

        category = ref.getCategory();
        stack.addArray(ref.m_dataItem->getCategoryText());
      }

      printer.print(ref.m_observation);
    }

    stack.clear();
  }

  template <typename T>
  void printSampleVersion2(T &writer, uint32_t jsonVersion, ObservationMap &observations)
  {
    using WriterType = decltype(writer);
    using StackType = JsonStack<WriterType>;

    AutoJsonObject streams(writer, "Streams");
    AutoJsonArray devStream(writer, "DeviceStream");
    StackType stack(writer);
    entity::JsonPrinter printer(writer, jsonVersion);

    std::string_view deviceId;
    std::string_view componentId;
    int32_t category = -1;
    std::string_view obsType;

    for (auto &ref : observations)
    {
      if (ref.getDeviceId() != deviceId)
      {
        stack.clear();
        componentId = "";
        category = -1;
        obsType = "";

        stack.addObject();

        deviceId = ref.getDeviceId();

        auto device = ref.m_device;
        stack.AddPairs("name", *(device->getComponentName()), "uuid", *(device->getUuid()));

        stack.addArray("ComponentStream");
      }

      if (ref.getComponentId() != componentId)
      {
        stack.clear(2);
        category = -1;
        obsType = "";

        stack.addObject();

        componentId = ref.getComponentId();
        auto component = ref.m_component;
        stack.AddPairs("component", component->getName(), "componentId", component->getId());
        if (component->getComponentName())
          stack.AddPairs("name", *(component->getComponentName()));
      }

      if (ref.getCategory() != category)
      {
        stack.clear(3);
        obsType = "";

        category = ref.getCategory();
        stack.addObject(ref.m_dataItem->getCategoryText());
      }

      if (ref.m_observation->getName() != obsType)
      {
        stack.clear(4);
        obsType = ref.m_observation->getName();
        stack.addArray(obsType);
      }

      printer.printEntity(ref.m_observation);
    }

    stack.clear();
  }

  std::string JsonPrinter::printSample(const uint64_t instanceId, const unsigned int bufferSize,
                                       const uint64_t nextSeq, const uint64_t firstSeq,
                                       const uint64_t lastSeq, ObservationList &observations,
                                       bool pretty) const
  {
    defaultSchemaVersion();

    StringBuffer output;
    RenderJson(output, m_pretty || pretty, [&](auto &writer) {
      AutoJsonObject top(writer);
      AutoJsonObject obj(writer, "MTConnectStreams");
      obj.AddPairs("jsonVersion", m_jsonVersion, "schemaVersion", *m_schemaVersion);
      {
        AutoJsonObject obj(writer, "Header");
        streamHeader(obj, m_version, m_senderName, instanceId, bufferSize, nextSeq, firstSeq, lastSeq,
                     *m_schemaVersion, m_modelChangeTime);
      }

      {
        if (!observations.empty())
        {
          // Order the observations by Device, Component, Category, Observation Type, and Sequence
          ObservationMap obs;
          for (const auto &o : observations)
          {
            if (!o->isOrphan())
              obs.emplace(o);
          }

          if (m_jsonVersion == 1)
            printSampleVersion1(writer, m_jsonVersion, obs);
          else if (m_jsonVersion == 2)
            printSampleVersion2(writer, m_jsonVersion, obs);
        }
        else
        {
          AutoJsonObject streams(writer, "Streams");
        }
      }
    });

    return string(output.GetString(), output.GetLength());
  }
}  // namespace mtconnect::printer
