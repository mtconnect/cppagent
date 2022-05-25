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

#include "asset/file_asset.hpp"

namespace mtconnect {
  namespace asset {
    using namespace entity;
    using namespace std;

    FactoryPtr FileArchetypeAsset::getFactory()
    {
      static auto root = make_shared<Factory>();

      static auto fileProperty = make_shared<Factory>(
          Requirements({Requirement("name", true), Requirement("VALUE", true)}));

      static auto fileProperties = make_shared<Factory>(Requirements(
          {Requirement("FileProperty", ENTITY, fileProperty, 1, Requirement::Infinite)}));

      static auto fileComment = make_shared<Factory>(
          Requirements({Requirement("timestamp", true), Requirement("VALUE", true)}));

      static auto fileComments = make_shared<Factory>(Requirements(
          {Requirement("FileComment", ENTITY, fileComment, 1, Requirement::Infinite)}));

      static auto fileArchetype = make_shared<Factory>(*Asset::getFactory());
      fileArchetype->addRequirements(Requirements {
          Requirement("name", true), Requirement("mediaType", true),
          Requirement("applicationCategory", true), Requirement("applicationType", true),
          Requirement("FileComments", ENTITY_LIST, fileComments, false),
          Requirement("FileProperties", ENTITY_LIST, fileProperties, false)});
      fileArchetype->setOrder({"FileProperties", "FileComments"});

      static bool first {true};
      if (first)
      {
        auto root = Asset::getRoot();
        root->registerFactory("FileArchetype", fileArchetype);
        first = false;
      }

      return fileArchetype;
    }

    void FileArchetypeAsset::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("FileArchetype", getFactory());
        once = false;
      }
    }

    FactoryPtr FileAsset::getFactory()
    {
      static auto file = make_shared<Factory>(*FileArchetypeAsset::getFactory());

      static auto destination = make_shared<Factory>(Requirements({Requirement("VALUE", true)}));

      static auto destinations = make_shared<Factory>(Requirements(
          {Requirement("Destination", ENTITY, destination, 1, Requirement::Infinite)}));

      static auto fileLocation = make_shared<Factory>(Requirements({Requirement("href", true)}));

      file->addRequirements(
          Requirements({Requirement("size", INTEGER), Requirement("versionId", STRING),
                        Requirement("state", {"EXPERIMENTAL", "PRODUCTION", "REVISION"}),
                        Requirement("FileLocation", ENTITY, fileLocation),
                        Requirement("Signature", false), Requirement("PublicKey", false),
                        Requirement("CreationTime", false), Requirement("ModificationTime", false),
                        Requirement("Destinations", ENTITY_LIST, destinations)}));
      file->setOrder({"FileProperties", "FileComments", "FileLocation", "Signature", "PublicKey",
                      "Destinations", "CreationTime", "ModificationTime"});

      static bool first {true};
      if (first)
      {
        auto root = Asset::getRoot();
        root->registerFactory("File", file);
        first = false;
      }

      return file;
    }

    void FileAsset::registerAsset()
    {
      static bool once {true};
      if (once)
      {
        Asset::registerAssetType("File", getFactory());
        once = false;
      }
    }
  }  // namespace asset
}  // namespace mtconnect
