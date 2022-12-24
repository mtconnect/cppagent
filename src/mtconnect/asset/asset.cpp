//
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

#include "asset/asset.hpp"

#include <map>
#include <utility>

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace asset {
    FactoryPtr Asset::getFactory()
    {
      static auto asset = make_shared<Factory>(
          Requirements({Requirement("assetId", false), Requirement("deviceUuid", false),
                        Requirement("timestamp", TIMESTAMP, false),
                        Requirement("removed", BOOL, false)}),
          [](const std::string &name, Properties &props) -> EntityPtr {
            return make_shared<Asset>(name, props);
          });

      return asset;
    }

    FactoryPtr ExtendedAsset::getFactory()
    {
      static auto asset = make_shared<Factory>(*Asset::getFactory());
      asset->addRequirements(Requirements({{"RAW", false}}));

      return asset;
    }

    void Asset::registerAssetType(const std::string &type, FactoryPtr factory)
    {
      auto root = getRoot();
      root->registerFactory(type, factory);
    }

    entity::FactoryPtr Asset::getRoot()
    {
      static auto root = make_shared<Factory>();
      static bool first {true};

      if (first)
      {
        root->registerFactory(regex(".+"), ExtendedAsset::getFactory());
        root->registerMatchers();
        first = false;
      }

      return root;
    }
  }  // namespace asset
}  // namespace mtconnect
