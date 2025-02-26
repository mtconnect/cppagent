// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#pragma once

#include <boost/algorithm/string.hpp>

#include <chrono>
#include <regex>
#include <unordered_map>

#include "mtconnect/config.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  /// @brief A message from a pub/sub messaging protocol
  class AGENT_LIB_API PipelineMessage : public Entity
  {
  public:
    using Entity::Entity;
    ~PipelineMessage() = default;

    DataItemPtr m_dataItem;                        ///< Mapped data item
    std::weak_ptr<device_model::Device> m_device;  ///< Mapped device
  };
  using PipelineMessagePtr = std::shared_ptr<PipelineMessage>;

  /// @brief A JsonMessage un-parsed
  class AGENT_LIB_API JsonMessage : public PipelineMessage
  {
  public:
    using PipelineMessage::PipelineMessage;
  };

  /// @brief An unparsed data message
  class AGENT_LIB_API DataMessage : public PipelineMessage
  {
  public:
    using PipelineMessage::PipelineMessage;
  };

  /// @brief A transform to map the topic to a data item
  class AGENT_LIB_API TopicMapper : public Transform
  {
  public:
    TopicMapper(const TopicMapper &) = default;
    TopicMapper(PipelineContextPtr context, const std::optional<std::string> &device = std::nullopt)
      : Transform("TopicMapper"), m_context(context), m_defaultDeviceName(device)
    {
      m_guard = EntityNameGuard("Message", RUN);
      if (m_defaultDeviceName)
        m_defaultDevice = m_context->m_contract->findDevice(*m_defaultDeviceName);
    }

    /// @brief Try to find a matching data item for the given topic
    ///
    /// Map using the following methods:
    /// 1. Try `<device>/<data item name or id>`
    /// 2. Try the default device and the data item name or id
    /// 3. Scan the path for any matching device and data item
    ///
    /// If found, remember the mapping of the topic to the data item
    /// @param topic the topic
    /// @return
    auto resolve(const std::string &topic)
    {
      using namespace std;
      namespace algo = boost::algorithm;
      using namespace boost;

      DataItemPtr dataItem;
      DevicePtr device;

      string name, deviceName;

      std::vector<string> path;
      boost::split(path, topic, algo::is_any_of("/"));
      if (path.size() > 1)
      {
        deviceName = path[0];
        name = path[1];
        dataItem = m_context->m_contract->findDataItem(deviceName, name);
      }

      if (!dataItem)
      {
        deviceName = m_defaultDeviceName.value_or("");
        name = topic;
        dataItem = m_context->m_contract->findDataItem(deviceName, name);
      }

      if (!dataItem && path.size() > 1)
      {
        name = path.back();
        dataItem = m_context->m_contract->findDataItem(deviceName, name);
      }

      if (!dataItem)
      {
        for (auto &tok : path)
        {
          device = m_context->m_contract->findDevice(tok);
          if (device)
            break;
        }

        if (device)
        {
          for (auto &tok : path)
          {
            dataItem = device->getDeviceDataItem(tok);
            if (dataItem)
              break;
          }
        }
      }

      // Note even if null so we don't have to try again
      m_resolved[topic] = dataItem;
      m_devices[topic] = device;

      return std::make_tuple(device, dataItem);
    }

    EntityPtr operator()(entity::EntityPtr &&entity) override
    {
      PipelineMessagePtr result;
      auto &body = entity->getValue<std::string>();
      auto c = body[0];

      DataItemPtr dataItem;
      DevicePtr device;

      if (c == '{' || c == '[')
      {
        entity::Properties props {entity->getProperties()};
        device = m_defaultDevice;
        result = std::make_shared<JsonMessage>("JsonMessage", props);
      }
      else
      {
        entity::Properties props {entity->getProperties()};
        if (auto topic = entity->maybeGet<std::string>("topic"))
        {
          if (auto it = m_devices.find(*topic); it != m_devices.end())
          {
            device = it->second.lock();
          }
          if (auto it = m_resolved.find(*topic); it != m_resolved.end())
          {
            dataItem = it->second.lock();
          }
          if (!dataItem && !device)
          {
            std::tie(device, dataItem) = resolve(*topic);
          }
        }

        result = std::make_shared<DataMessage>("DataMessage", props);
      }
      result->m_dataItem = dataItem;
      result->m_device = device;

      return next(result);
    }

  protected:
    PipelineContextPtr m_context;
    std::optional<std::string> m_defaultDeviceName;
    DevicePtr m_defaultDevice;
    std::unordered_map<std::string, std::weak_ptr<device_model::data_item::DataItem>> m_resolved;
    std::unordered_map<std::string, std::weak_ptr<device_model::Device>> m_devices;
  };
}  // namespace mtconnect::pipeline
