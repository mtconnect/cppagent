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

#pragma once

#include <chrono>
#include <regex>

#include <boost/algorithm/string.hpp>

#include "entity/entity.hpp"
#include "observation/observation.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "transform.hpp"
#include "device_model/device.hpp"

namespace mtconnect
{
  class Device;
  
  namespace pipeline
  {
    class Message : public Entity
    {
    public:
      using Entity::Entity;
      ~Message() = default;
      
      DataItemPtr m_dataItem;
      DevicePtr m_device;
    };
    using MessagePtr = std::shared_ptr<Message>;
    
    class JsonMessage : public Message
    {
    public:
      using Message::Message;
    };
    class DataMessage : public Message
    {
    public:
      using Message::Message;
    };
    

    class TopicMapper : public Transform
    {
    public:
      TopicMapper(const TopicMapper &) = default;
      TopicMapper(PipelineContextPtr context,
                  const std::optional<std::string> &device = std::nullopt)
      : Transform("TopicMapper"), m_context(context), m_defaultDevice(device)
      {
        m_guard = TypeGuard<Message>(RUN);
      }
      
      const EntityPtr operator()(const EntityPtr entity) override
      {
        using namespace std;
        namespace algo = boost::algorithm;
        using namespace boost;
        
        auto &body = entity->getValue<std::string>();
        DataItemPtr dataItem;
        DevicePtr dev;
        entity::Properties props{entity->getProperties()};
        if (auto topic = entity->maybeGet<std::string>("topic"))
        {
          string name, device;

          std::vector<string> path;
          boost::split(path, *topic, algo::is_any_of("/"));
          if (path.size() > 1)
          {
            device = path[0];
            name = path[1];
            dataItem = m_context->m_contract->findDataItem(device, name);
          }
          
          if (!dataItem)
          {
            device = m_defaultDevice.value_or("");
            name = *topic;
            dataItem = m_context->m_contract->findDataItem(device, name);
          }
          
          if (!dataItem && path.size() > 1)
          {
            name = path.back();
            dataItem = m_context->m_contract->findDataItem(device, name);
          }
          
          if (!dataItem)
          {
            for (auto &tok : path)
            {
              dev = m_context->m_contract->findDevice(tok);
              if (dev)
                break;
            }
            
            if (dev)
            {
              for (auto &tok : path)
              {
                dataItem = dev->getDeviceDataItem(tok);
                if (dataItem)
                  break;
              }
            }
          }
        }
        
        MessagePtr result;
        // Check for JSON Message
        if (body[0] == '{')
        {
          result = std::make_shared<JsonMessage>("JsonMessage", props);
        }
        else
        {
          result = std::make_shared<DataMessage>("DataMessage", props);
        }
        result->m_dataItem = dataItem;
        result->m_device = dev;

        
        return next(result);
      }
      
    protected:
      PipelineContextPtr m_context;
      std::optional<std::string> m_defaultDevice;

    };    
  }  // namespace pipeline
}  // namespace mtconnect
