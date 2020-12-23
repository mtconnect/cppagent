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

#include "asset_buffer.hpp"

namespace  mtconnect {
  AssetPtr AssetBuffer::addAsset(AssetPtr asset)
  {
    AssetPtr old {};

    if (!asset->getTimestamp())
    {
      auto time = getCurrentTime(GMT_UV_SEC);
      asset->addProperty({ "timestamp", time });
    }
          
    // Lock the asset addition to protect from multithreaded collisions. Releaes
    // before we add the event so we don't cause a race condition.
    {
      std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
      
      auto id = asset->getAssetId();
      
      old = m_primaryIndex[id];
      if (old)
      {
        auto it = std::find_if(m_buffer.begin(), m_buffer.end(),
                               [&id](const AssetPtr &a) -> bool {
          return id == a->getAssetId();
        });
        if (it == m_buffer.end())
          throw entity::PropertyError("Asset key " + id + " not found");
      }

      // Add to primary index
      m_primaryIndex[id] = asset;

      // Add secondary indexes
      auto &types = m_typeIndex[asset->getType()];
      types[id] = asset;

      // Handle secondary index and cleanup
      auto oldDevice = old->getDeviceUuid();
      auto device = asset->getDeviceUuid();
      if (device)
      {
        if (oldDevice && *oldDevice != *device)
        {
          auto &devices = m_deviceIndex[*oldDevice];
          devices.erase(id);
        }
        
        auto &devices = m_deviceIndex[*device];
        devices[id] = asset;
      }
      else if (oldDevice)
      {
        asset->addProperty({"deviceUuid", *oldDevice });
        auto &devices = m_deviceIndex[*oldDevice];
        devices[id] = asset;
      }
                    
      // Check for overflow
      if (m_buffer.size() >= m_maxAssets)
      {
        auto old = m_buffer.front();
        m_buffer.pop_front();
        
        m_primaryIndex.erase(old->getAssetId());
        m_typeIndex[old->getType()].erase(old->getAssetId());
        auto device = old->getDeviceUuid();
        if (device)
          m_deviceIndex[*device].erase(old->getAssetId());
      }
      
      if (!asset->isRemoved())
      {
        m_buffer.emplace_back(asset);
      }
    }
    
    return old;
  }

  
  AssetPtr AssetBuffer::removeAsset(const std::string &id)
  {
    AssetPtr old {};
    
    std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
    
    old = m_primaryIndex[id];
    if (old)
    {
      old->addProperty({ "removed", "true" });
    }

    return old;
  }

}
