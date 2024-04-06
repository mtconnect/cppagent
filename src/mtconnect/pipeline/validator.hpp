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

#include <chrono>
#include <regex>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/validation/observations.hpp"
#include "transform.hpp"

namespace mtconnect::pipeline {
  using namespace entity;
  
  /// @brief Map a token list to data items or asset types
  class AGENT_LIB_API Validator : public Transform
  {
  public:
    Validator(const Validator &) = default;
    Validator(PipelineContextPtr context)
      : Transform("Validator"),
        m_contract(context->m_contract.get())
    {
      m_guard = TypeGuard<observation::Event>(RUN) || TypeGuard<observation::Observation>(SKIP);
    }
    
    EntityPtr operator()(entity::EntityPtr &&entity) override
    {
      using namespace observation;
      using namespace mtconnect::validation::observations;
      auto evt = std::dynamic_pointer_cast<Event>(entity);
      
      auto di = evt->getDataItem();
      auto &type = di->getType();
      auto &value = evt->getValue<std::string>();
      
      if (value == "UNAVAILABLE")
      {
        evt->setProperty("quality", std::string("VALID"));
      }
      else
      {
        // Optimize
        auto vocab = ControlledVocabularies.find(type);
        if (vocab != ControlledVocabularies.end())
        {
          auto &lits = vocab->second;
          if (lits.size() != 0)
          {
            auto lit = lits.find(value);
            if (lit != lits.end())
            {
              evt->setProperty("quality", std::string("VALID"));
              // Check for deprecated
            }
            else
            {
              evt->setProperty("quality", std::string("INVALID"));
              // Log once
              LOG(warning) << "Invalid value for '" << type << "' " <<  evt->getValue<std::string>();
            }
          }
          else
          {
            evt->setProperty("quality", std::string("VALID"));
          }
        }
        else
        {
          evt->setProperty("quality", std::string("UNVERIFIABLE"));
        }
      }
      
      return next(std::move(evt));
    }

  protected:
    // Logging Context
    std::set<std::string> m_logOnce;
    PipelineContract *m_contract;
    std::unordered_map<std::string, WeakDataItemPtr> m_dataItemMap;
  };
}  // namespace mtconnect::pipeline
