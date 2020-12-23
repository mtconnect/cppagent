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
  using namespace entity;
  
  FactoryPtr Asset::getFactory()
  {
    static auto asset = make_shared<Factory>(Requirements({
      Requirement("assetId", true ),
      Requirement("deviceUuid", true ),
      Requirement("timestamp", true ),
      Requirement("removed", false ) }),
      [](const std::string &name, Properties &props) -> EntityPtr {
        return make_shared<Asset>(name, props);
      });
      
    return asset;
  }
  
  entity::FactoryPtr Asset::getRoot()
  {
    static auto root = make_shared<Factory>();    
    root->registerFactory(regex(".+"), ExtendedAsset::getFactory());
    root->registerMatchers();
    return root;
  }

  FactoryPtr ExtendedAsset::getFactory()
  {
    static auto asset = make_shared<Factory>(*Asset::getFactory());
    asset->addRequirements(Requirements({
      { "RAW", false }
    }));
    
    return asset;
  }

}  // namespace mtconnect
