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

#include "asset/qif_document.hpp"

using namespace std;

namespace mtconnect::asset {
  using namespace entity;
  FactoryPtr QIFDocumentWrapper::getFactory()
  {
    static FactoryPtr factory;
    if (!factory)
    {
      static auto ext = make_shared<Factory>();
      ext->registerFactory(regex(".+"), ext);
      ext->setAny(true);

      static auto doc = make_shared<Factory>();
      doc->registerFactory(regex(".+"), ext);
      doc->setAny(true);

      factory = make_shared<Factory>(*Asset::getFactory());
      factory->addRequirements(Requirements {
          {"qifDocumentType",
           {"MEASUREMENT_RESOURCE", "PLAN", "PRODUCT", "RESULTS", "RULES", "STATISTICS"},
           true},
          {"QIFDocument", ENTITY, doc, true}});
    }

    return factory;
  }

  void QIFDocumentWrapper::registerAsset()
  {
    static bool once {true};
    if (once)
    {
      Asset::registerAssetType("QIFDocumentWrapper", getFactory());
      once = false;
    }
  }
}  // namespace mtconnect::asset
