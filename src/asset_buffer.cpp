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

using namespace std;

namespace  mtconnect {
  
  inline void AssetBuffer::adjustCount(AssetPtr asset, int delta)
  {
    m_removedAssets += delta;
    m_typeRemoveCount[asset->getType()] += delta;
    if (auto device = asset->getDeviceUuid())
      m_deviceRemoveCount[*device] += delta;
  }

  
  AssetPtr AssetBuffer::updateAsset(const std::string &id,
                                    Index::iterator &it,
                                    AssetPtr asset)
  {
    AssetPtr old = it->second;

    if (asset->getType() != old->getType())
    {
      throw entity::PropertyError("Assed id: " + id +
                                  " cannot chage type from " +
                                  old->getType() +
                                  " to " + asset->getType());
    }
    
    auto lit = std::find_if(m_buffer.begin(), m_buffer.end(),
                           [&id](const AssetPtr &a) -> bool {
      return id == a->getAssetId();
    });
    if (lit == m_buffer.end())
    {
      throw entity::PropertyError("Asset key " + id + " not found");
    }
    
    // If the asset is not removed, then move it to the front
    if (!asset->isRemoved())
    {
      m_buffer.erase(lit);
    }
    else
    {
      // otherwise, update in place
      *lit = asset;
    }
    
    it->second = asset;
    m_typeIndex[old->getType()][id] = asset;
    auto device = asset->getDeviceUuid();
    auto od = old->getDeviceUuid();
    
    // Handle device change
    if ((od && !device) || (device && od && *od != *device))
    {
      m_deviceIndex[*od].erase(id);
    }
    
    // If the device is given, add the asset to the device index
    if (device)
    {
      m_deviceIndex[*device][id] = asset;
    }
    
    // Handle counts
    if (old->isRemoved())
    {
      adjustCount(old, -1);
    }
    if (asset->isRemoved())
    {
      adjustCount(asset, 1);
    }
    else
    {
      m_buffer.emplace_back(asset);
    }

    return old;
  }
  
  AssetPtr AssetBuffer::addAsset(AssetPtr asset)
  {
    AssetPtr old {};
    std::lock_guard<std::recursive_mutex> lock(m_bufferLock);

    if (!asset->getTimestamp())
    {
      asset->setProperty("timestamp", getCurrentTime(GMT_UV_SEC));
    }
    
    auto id = asset->getAssetId();
    auto it = m_primaryIndex.find(id);
    if (it != m_primaryIndex.end())
    {
      old = updateAsset(id, it, asset);
    }
    else
    {
      // Add to the end of the buffer
      m_buffer.emplace_back(asset);

      // Add to primary index
      m_primaryIndex[id] = asset;

      // Add secondary indexes
      m_typeIndex[asset->getType()][id] = asset;
      if (auto device = asset->getDeviceUuid())
      {
        m_deviceIndex[*device][id] = asset;
      }
      
      // Handle counts
      if (asset->isRemoved())
      {
        adjustCount(asset, 1);
      }
      
      // Check for overflow
      if (m_buffer.size() > m_maxAssets)
      {
        auto of = m_buffer.front();
        m_buffer.pop_front();
        
        // Clean up indexes
        m_primaryIndex.erase(of->getAssetId());
        m_typeIndex[of->getType()].erase(of->getAssetId());
        if (auto ofd = of->getDeviceUuid())
        {
          m_deviceIndex[*ofd].erase(of->getAssetId());
        }
        if (of->isRemoved())
        {
          adjustCount(of, -1);
        }
      }
    }
  
    return old;
  }

  
  AssetPtr AssetBuffer::removeAsset(const std::string &id, const string time)
  {
    AssetPtr asset {};
    std::lock_guard<std::recursive_mutex> lock(m_bufferLock);
    
    auto it = m_primaryIndex.find(id);
    if (it != m_primaryIndex.end() && !it->second->isRemoved())
    {
      asset = make_shared<Asset>(*(it->second));
      asset->setProperty("removed", true);
      string ts;
      if (!time.empty())
        ts = time;
      else
        ts = getCurrentTime(GMT_UV_SEC);
      asset->setProperty("timestamp", ts);
      updateAsset(id, it, asset);
    }
    
    return asset;
  }

}
