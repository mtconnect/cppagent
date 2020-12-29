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

#include "file_asset.hpp"

namespace mtconnect
{
  using namespace entity;
  using namespace std;
  
  FactoryPtr FileArchetypeAsset::getFactory()
  {
    static auto root = make_shared<Factory>();
    
    static auto fileProperty = make_shared<Factory>(Requirements({
      Requirement("name", true ),
      Requirement("VALUE", true) }));
    
    static auto fileProperties = make_shared<Factory>(Requirements({
        Requirement("FileProperty", ENTITY, fileProperty) }));
    
    static auto fileComment = make_shared<Factory>(Requirements({
      Requirement("timestamp", true ),
      Requirement("VALUE", true) }));
    
    static auto fileComments = make_shared<Factory>(Requirements({
      Requirement("FileComment", ENTITY, fileComment) }));

    static auto fileArchetype = make_shared<Factory>(*Asset::getFactory());
    fileArchetype->addRequirements(Requirements{
      Requirement("mediaTyep", true),
      Requirement("applicationCategory", true),
      Requirement("applicationType", true),
      Requirement("FileComments", ENTITY_LIST, fileComments, false),
      Requirement("FileProperties", ENTITY_LIST, fileProperties, false)
    });
    
    static bool first { true };
    if (first)
    {
      auto root = Asset::getRoot();
      root->registerFactory("FileArchetype", fileArchetype);
      first = false;
    }

    return fileArchetype;
  }
  
  RegisterAsset<FileArchetypeAsset>* const  FileArchetypeAsset::m_registerAsset =
   new RegisterAsset<FileArchetypeAsset>("FileArchetype");

  
  FactoryPtr FileAsset::getFactory()
  {
    static auto file = make_shared<Factory>(*FileArchetypeAsset::getFactory());
    
    static auto destination = make_shared<Factory>(Requirements({
      Requirement("VALUE", true )}));
    
    static auto destinations = make_shared<Factory>(Requirements({
      Requirement("Destination", ENTITY, destination) }));

    static auto fileLocation = make_shared<Factory>(Requirements({
      Requirement("href", true )}));

    file->addRequirements(Requirements({
      Requirement("size", INTEGER),
      Requirement("verisionId", STRING),
      Requirement("state", STRING),
      Requirement("FileLocation", ENTITY, fileLocation),
      Requirement("Signature", false),
      Requirement("PublicKey", false),
      Requirement("CreationTime", false),
      Requirement("ModificationTime", false),
      Requirement("Destinations", ENTITY_LIST, destinations)
    }));
    
    static bool first { true };
    if (first)
    {
      auto root = Asset::getRoot();
      root->registerFactory("File", file);
      first = false;
    }
    
    return file;
  }
  
  RegisterAsset<FileAsset>* const  FileAsset::m_registerAsset =
   new RegisterAsset<FileAsset>("File");

}
