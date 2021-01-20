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

#pragma once

#include "adapter.hpp"
#include "entity/entity.hpp"

#include <regex>
#include <chrono>

namespace mtconnect
{
  namespace adapter
  {
    using TokenList = std::list<std::string>;

    struct ShdrObservation
    {
      ShdrObservation() = default;
      ShdrObservation(const ShdrObservation &) = default;
      
      Timestamp m_timestamp;
      std::optional<double> m_duration;
      const Device     *m_device { nullptr };
      entity::Properties m_properties;
    };
    
    struct ShdrDataItemObservation : ShdrObservation
    {
      ShdrDataItemObservation() = default;
      ShdrDataItemObservation(const ShdrDataItemObservation &o) = default;

      bool m_unavailable {false};
      const DataItem *m_dataItem {nullptr};
    };
    
    struct ShdrAssetObservation : ShdrObservation
    {
      ShdrAssetObservation() = default;
      ShdrAssetObservation(const ShdrAssetObservation &o) = default;
      std::string m_body;
    };
    
    struct ShdrAssetCommand : ShdrObservation
    {
      ShdrAssetCommand() = default;
      ShdrAssetCommand(const ShdrAssetCommand &o) = default;
      enum Command
      {
        REMOVE_ALL,
        REMOVE_ASSET
      };
      
      Command m_command { REMOVE_ALL };
    };
        
    class ShdrParser
    {
    public:
      void processData(const std::string &data, Context &context);
      
    protected:
    };
    

  }
}
