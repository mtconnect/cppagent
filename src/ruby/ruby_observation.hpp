//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#pragma once

#include "observation/observation.hpp"
#include "ruby_entity.hpp"
#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>

namespace mtconnect::ruby {
  using namespace mtconnect::observation;
  using namespace std;
  using namespace Rice;

  struct RubyObservation {
    void create(Rice::Module &module)
    {
      m_observation = define_class_under<Observation, entity::Entity>(module, "Observation");
      c_Observation = m_observation.value();
    }
    
    void methods()
    {           
      m_observation.define_singleton_function("make", [](const DataItemPtr dataItem,
                                                         Properties props,
                                                        const Timestamp *timestamp) {
          ErrorList errors;
          Timestamp ts;
          if (timestamp == nullptr)
            ts = std::chrono::system_clock::now();
          else
            ts = *timestamp;
          
          auto obs = Observation::make(dataItem, props, ts, errors);
          return obs;
        }, Return(), Arg("dataItem"), Arg("properties"), Arg("timestamp") = nullptr).
        define_method("data_item", &Observation::getDataItem).
        define_method("copy", [](Observation *o) { return o->copy(); }).
        define_method("timestamp", &Observation::getTimestamp);
    }
    
    Data_Type<Observation> m_observation;
  };
}

namespace Rice::detail {
  template <>
  class From_Ruby<observation::ObservationPtr>
  {
  public:
    observation::ObservationPtr convert(VALUE value)
    {
      Wrapper* wrapper = detail::getWrapper(value, Data_Type<observation::Observation>::rb_type());

      using Wrapper_T = WrapperSmartPointer<std::shared_ptr, observation::Observation>;
      Wrapper_T* smartWrapper = static_cast<Wrapper_T*>(wrapper);
      std::shared_ptr<observation::Observation> ptr = dynamic_pointer_cast<observation::Observation>(smartWrapper->data());
      
      if (!ptr)
      {
        std::string message = "Invalid smart pointer wrapper";
          throw std::runtime_error(message.c_str());
      }
      return ptr;
    }
  };
}
