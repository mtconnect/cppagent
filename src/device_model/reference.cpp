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

#include "reference.hpp"

#include <list>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "device.hpp"
#include "entity/entity.hpp"
#include "logging.hpp"
#include "utilities.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model {
    FactoryPtr Reference::getFactory()
    {
      static FactoryPtr references;
      if (!references)
      {
        auto reference =
            make_shared<Factory>(Requirements {{"idRef", true}, {"name", false}},
                                 [](const std::string &name, Properties &ps) -> EntityPtr {
                                   auto r = make_shared<Reference>(name, ps);
                                   if (name == "ComponentRef")
                                     r->m_type = COMPONENT;
                                   else if (name == "DataItemRef")
                                     r->m_type = DATA_ITEM;

                                   return r;
                                 });

        references = make_shared<Factory>(
            Requirements {Requirement("ComponentRef", ENTITY, reference, 0, Requirement::Infinite),
                          Requirement("DataItemRef", ENTITY, reference, 0, Requirement::Infinite)});

        references->registerMatchers();
        references->setMinListSize(1);
      }
      return references;
    }

    FactoryPtr Reference::getRoot()
    {
      static auto root = make_shared<Factory>(
          Requirements {Requirement("References", ENTITY_LIST, Reference::getFactory(), false)});

      return root;
    }

    void Reference::resolve(DevicePtr device)
    {
      NAMED_SCOPE("reference");
      if (m_type == COMPONENT)
      {
        auto comp = device->getComponentById(get<string>("idRef"));
        if (comp)
          m_component = comp;
        else
          LOG(warning) << "Refernce: Cannot find Component for idRef " << get<string>("idRef");
      }
      else if (m_type == DATA_ITEM)
      {
        auto di = device->getDeviceDataItem(get<string>("idRef"));
        if (di)
          m_dataItem = di;
        else
          LOG(warning) << "Refernce: Cannot find DataItem for idRef " << get<string>("idRef");
      }
      else
      {
        LOG(warning) << "Reference: Unknown Reference type for: " << getName();
      }
    }

  }  // namespace device_model
}  // namespace mtconnect
