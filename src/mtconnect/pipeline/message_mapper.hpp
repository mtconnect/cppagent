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

#pragma once

#include <boost/algorithm/string.hpp>

#include <chrono>
#include <regex>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "mtconnect/config.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "topic_mapper.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  /// @brief JsonMapper does nothing, it is a placeholder for a json interpreter
  class AGENT_LIB_API JsonMapper : public Transform
  {
  public:
    JsonMapper(const JsonMapper &) = default;
    JsonMapper(PipelineContextPtr context) : Transform("JsonMapper"), m_context(context)
    {
      m_guard = TypeGuard<JsonMessage>(RUN);
    }

    const EntityPtr operator()(const EntityPtr entity) override
    {
      auto json = std::dynamic_pointer_cast<JsonMessage>(entity);

      return nullptr;
    }

  protected:
    PipelineContextPtr m_context;
  };

  /// @brief Attempt to find a data item associated with a topic from a pub/sub message
  ///        system
  class AGENT_LIB_API DataMapper : public Transform
  {
  public:
    DataMapper(const DataMapper &) = default;
    DataMapper(PipelineContextPtr context, source::adapter::Handler *handler)
      : Transform("DataMapper"), m_context(context), m_handler(handler)
    {
      m_guard = TypeGuard<DataMessage>(RUN);
    }

    const EntityPtr operator()(const EntityPtr entity) override
    {
      auto data = std::dynamic_pointer_cast<DataMessage>(entity);
      if (data->m_dataItem)
      {
        entity::Properties props {{"VALUE", data->getValue()}};
        entity::ErrorList errors;
        try
        {
          auto obs = observation::Observation::make(data->m_dataItem, props,
                                                    std::chrono::system_clock::now(), errors);
          if (errors.empty())
            return next(obs);
        }
        catch (entity::EntityError &e)
        {
          LOG(error) << "Could not create observation: " << e.what();
        }
        for (auto &e : errors)
        {
          LOG(warning) << "Error while parsing message data: " << e->what();
        }

        return nullptr;
      }
      else
      {
        if (std::holds_alternative<std::string>(data->getValue()))
        {
          // Try processing as shdr data
          auto entity = make_shared<Entity>(
              "Data", Properties {{"VALUE", data->getValue()}, {"source", string("")}});
          next(entity);
        }
        else
        {
          std::string msg;
          if (auto topic = data->maybeGet<std::string>("topic"); topic)
            msg = *topic;
          else
            msg = "unknown topic";
          LOG(error) << "Cannot find data item for topic: " << msg
                     << " and data: " << data->getValue<std::string>();
        }
        return nullptr;
      }
    }

  protected:
    PipelineContextPtr m_context;
    source::adapter::Handler *m_handler;
  };
}  // namespace mtconnect::pipeline
