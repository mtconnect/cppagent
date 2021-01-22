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

#include "asset_mapper.hpp"

#include "shdr_parser.hpp"

using namespace std;

namespace mtconnect
{
  namespace adapter
  {
    static dlib::logger g_logger("AssetMapper");

    void MapTokensToAsset(ShdrObservation &obs, TokenList::const_iterator &token,
                          const TokenList::const_iterator &end, Context &)
    {
      obs.m_observed.emplace<DataItemObservation>();
      if (*token == "@ASSET@")
      {
        auto &observation = get<AssetObservation>(obs.m_observed);
        obs.m_properties.emplace("assetId", *++token);
        obs.m_properties.emplace("type", *++token);
        observation.m_body = *token;
      }
      else if (*token == "REMOVE_ALL_ASSETS@")
      {
        obs.m_observed = AssetCommand::REMOVE_ALL;
        obs.m_properties.emplace("type", *++token);
      }
      else if (*token == "REMOVE_ASSETS@")
      {
        obs.m_observed = AssetCommand::REMOVE_ASSET;
        obs.m_properties.emplace("assetId", *++token);
      }
      else
      {
        g_logger << dlib::LWARN << "Unsupported Asset Command: " << *token;
      }
      token++;
    }
  }  // namespace adapter
}  // namespace mtconnect
