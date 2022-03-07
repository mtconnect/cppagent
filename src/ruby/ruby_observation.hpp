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
      m_observation = make_unique<Class>(define_class_under<Observation, entity::Entity>(module, "Observation"));
      c_Observation = m_observation->value();
      m_dataSet = make_unique<Class>(define_class_under<DataSet>(module, "DataSet"));
      m_dataSetEntry = make_unique<Class>(define_class_under<DataSet>(module, "DataSetEntry"));
    }
    
    void methods()
    {           
      m_observation->define_singleton_method("make", [](const DataItemPtr dataItem,
                                                        const Properties &incompingProps,
                                                        const Timestamp *timestamp) {
          ErrorList errors;
          Timestamp ts;
          if (timestamp == nullptr)
            ts = std::chrono::system_clock::now();
          else
            ts = *timestamp;
          auto obs = Observation::make(dataItem, incompingProps, ts, errors);
          return obs;
        }, Return().takeOwnership(), Arg("dataItem"), Arg("properties"), Arg("timestamp") = nullptr);
      
      Data_Type<DataSetEntry> *dsep = dynamic_cast<Data_Type<DataSetEntry>*>(m_dataSetEntry.get());
      dsep->define_attr("key", &DataSetEntry::m_key);
      dsep->define_attr("value", &DataSetEntry::m_value);
      dsep->define_attr("removed", &DataSetEntry::m_removed);
      
      
    }
    
    std::unique_ptr<Rice::Class> m_observation;
    std::unique_ptr<Rice::Class> m_dataSetEntry;
    std::unique_ptr<Rice::Class> m_dataSet;
  };
}
