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
#include "topic_mapper.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
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

    EntityPtr operator()(entity::EntityPtr &&entity) override
    {
      auto source = entity->maybeGet<string>("source");
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
          {
            if (source)
              data->m_dataItem->setDataSource(*source);
            return next(std::move(obs));
          }
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
          next(std::move(entity));
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
