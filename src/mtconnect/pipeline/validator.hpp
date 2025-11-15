// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/validation/observations.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  using namespace entity;

  /// @brief Validate obsrvations based on Controlled Vocabularies
  ///
  /// - Does not validate data sets and tables
  /// - Validates all events, not samples or conditions
  class AGENT_LIB_API Validator : public Transform
  {
  public:
    Validator(const Validator &) = default;
    Validator(PipelineContextPtr context)
      : Transform("Validator"), m_contract(context->m_contract.get())
    {
      m_guard = TypeGuard<observation::Observation>(RUN) || TypeGuard<entity::Entity>(SKIP);
    }

    /// @brief validate the Event
    /// @param entity The Event entity
    /// @returns modified entity with quality and deprecated properties
    EntityPtr operator()(entity::EntityPtr &&entity) override
    {
      using namespace observation;
      using namespace mtconnect::validation::observations;
      auto obs = std::dynamic_pointer_cast<Observation>(entity);
      auto &value = obs->getValue();

      bool valid = true;
      auto di = obs->getDataItem();
      if (!obs->isUnavailable() && !di->isDataSet())
      {
        if (auto evt = std::dynamic_pointer_cast<observation::Event>(obs))
        {
          auto vocab = ControlledVocabularies.find(evt->getName());
          if (vocab != ControlledVocabularies.end())
          {
            auto sv = std::get_if<std::string>(&value);
            auto &lits = vocab->second;
            if (lits.size() != 0 && sv != nullptr)
            {
              auto lit = lits.find(*sv);
              if (lit != lits.end())
              {
                // Check if it has not been introduced yet
                if (lit->second.first > 0 && m_contract->getSchemaVersion() < lit->second.first)
                  valid = false;

                // Check if deprecated
                if (lit->second.second > 0 && m_contract->getSchemaVersion() >= lit->second.second)
                {
                  evt->setProperty("deprecated", true);
                }
              }
              else
              {
                valid = false;
              }
            }
            else if (lits.size() != 0)
            {
              valid = false;
            }
          }
          else
          {
            evt->setProperty("quality", std::string("UNVERIFIABLE"));
          }
        }
        else if (auto spl = std::dynamic_pointer_cast<observation::Sample>(obs))
        {
          if (!(spl->hasProperty("quality") || std::holds_alternative<double>(value) ||
                std::holds_alternative<int64_t>(value)))
            valid = false;
        }
      }

      if (!valid)
      {
        obs->setProperty("quality", std::string("INVALID"));
        // Log once
        auto &id = di->getId();
        if (m_logOnce.count(id) < 1)
        {
          LOG(warning) << "DataItem '" << id << "': Invalid value for '" << obs->getName() << "': '"
                       << value << '\'';
          m_logOnce.insert(id);
        }
        else
        {
          LOG(trace) << "DataItem '" << id << "': Invalid value for '" << obs->getName();
        }
      }
      else if (!obs->hasProperty("quality"))
      {
        obs->setProperty("quality", std::string("VALID"));
      }

      return next(std::move(obs));
    }

  protected:
    // Logging Context
    std::set<std::string> m_logOnce;
    PipelineContract *m_contract;
    std::unordered_map<std::string, WeakDataItemPtr> m_dataItemMap;
  };
}  // namespace mtconnect::pipeline
