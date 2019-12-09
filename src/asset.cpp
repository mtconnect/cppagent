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

#include "asset.hpp"

#include <map>
#include <utility>

using namespace std;

namespace mtconnect
{
  Asset::Asset(std::string asssetId, std::string type, std::string content, const bool removed)
      : m_assetId(std::move(asssetId)),
        m_content(std::move(content)),
        m_type(std::move(type)),
        m_removed(removed)
  {
  }

  Asset::~Asset()
  {
  }

  void Asset::addIdentity(const std::string &key, const std::string &value)
  {
    if (key == "deviceUuid")
      m_deviceUuid = value;
    else if (key == "timestamp")
      m_timestamp = value;
    else if (key == "removed")
      m_removed = value == "true";
    else if (key == "assetId")
      m_assetId = value;
    else
      m_identity[key] = value;
  }
}  // namespace mtconnect
