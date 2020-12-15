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
      Requirement("value", true) }));
    
    static auto fileProperties = make_shared<Factory>(Requirements({
        Requirement("FileProperty", Requirement::ENTITY, fileProperty) }));
    
    static auto fileComment = make_shared<Factory>(Requirements({
      Requirement("timestamp", true ),
      Requirement("value", true) }));
    
    static auto fileComments = make_shared<Factory>(Requirements({
      Requirement("FileComment", Requirement::ENTITY, fileComment) }));

    static auto fileArchetype = make_shared<Factory>(Requirements{
      Requirement("assetId", true ),
      Requirement("deviceUuid", true ),
      Requirement("timestamp", true ),
      Requirement("removed", false ),
      Requirement("name", true ),
      Requirement("mediaTyep", true),
      Requirement("applicationCategory", true),
      Requirement("applicationType", true),
      Requirement("FileComments", Requirement::ENTITY_LIST, fileComments, false),
      Requirement("FileProperties", Requirement::ENTITY_LIST, fileProperties, false)
    });

    return fileArchetype;
  }
  
  FactoryPtr FileAsset::getFactory()
  {
    static auto file = make_shared<Factory>(*FileArchetypeAsset::getFactory());
    
    static auto destination = make_shared<Factory>(Requirements({
      Requirement("value", true )}));
    
    static auto destinations = make_shared<Factory>(Requirements({
      Requirement("Destination", Requirement::ENTITY, destination) }));

    static auto fileLocation = make_shared<Factory>(Requirements({
      Requirement("href", true )}));

    static auto value = make_shared<Factory>(Requirements({
      Requirement("value", true )}));

    file->addRequirements(Requirements({
      Requirement("size", Requirement::INTEGER),
      Requirement("verisionId", Requirement::STRING),
      Requirement("state", Requirement::STRING),
      Requirement("FileLocation", Requirement::ENTITY, fileLocation),
      Requirement("Signature", Requirement::ENTITY, value, false),
      Requirement("PublicKey", Requirement::ENTITY, value, false),
      Requirement("CreationTime", Requirement::ENTITY, value),
      Requirement("ModificationTime", Requirement::ENTITY, value, false),
      Requirement("Destinations", Requirement::ENTITY_LIST, destinations)
    }));
    
    return file;
  }

  
}
