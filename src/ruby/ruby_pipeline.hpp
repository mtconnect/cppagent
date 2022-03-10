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

#include "entity/entity.hpp"
#include "device_model/device.hpp"
#include "device_model/data_item/data_item.hpp"
#include "adapter/adapter_pipeline.hpp"

#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>
#include "ruby_transform.hpp"
#include "ruby_smart_ptr.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect;
  using namespace pipeline;
  using namespace std;
  using namespace Rice;
  
  struct RubyPipeline {
    void create(Rice::Module &module)
    {
      m_transform =  define_class_under<Transform>(module, "Transform");
      m_rubyTransform =  define_class_under<RubyTransform, Transform>(module, "RubyTransform");
      m_pipelineContext =  define_class_under<pipeline::PipelineContext>(module, "PipelineContext");
      m_pipeline = define_class_under<pipeline::Pipeline>(module, "Pipeline");
      m_adapterPipeline = define_class_under<adapter::AdapterPipeline, pipeline::Pipeline>(module, "AdapterPipeline");
    }
    
    void methods()
    {
      m_pipeline.define_method("splice_before", &pipeline::Pipeline::spliceBefore, Arg("name"), Arg("transform")).
        define_method("splice_after", &pipeline::Pipeline::spliceAfter, Arg("name"), Arg("transform")).
        define_method("first_after", &pipeline::Pipeline::firstAfter, Arg("name"), Arg("transform")).
        define_method("last_after", &pipeline::Pipeline::lastAfter, Arg("name"), Arg("transform")).
        define_method("run", &pipeline::Pipeline::run, Arg("entity")).
        define_method("context", [](pipeline::Pipeline *p) { return p->getContext(); }).
        define_method("remove", &pipeline::Pipeline::remove, Arg("transform")).
        define_method("replace", &pipeline::Pipeline::replace, Arg("old"), Arg("new"));

      //m_adapterPipeline.define_constructor(Constructor<adapter::AdapterPipeline>());
      
      m_transform.define_method("name", &RubyTransform::getName).
        define_method("guard=", [](RubyTransform* t, const Symbol &guard) { t->setGuard(guard); }, Arg("guard")).
        define_method("forward", &RubyTransform::next, Arg("entity"));
      
      m_transform.const_set("CONTINUE", CONTINUE);
      m_transform.const_set("RUN", RUN);
      m_transform.const_set("SKIP", SKIP);

      m_rubyTransform.define_constructor(smart_ptr::Constructor<RubyTransform, Object, const string, const Symbol>(),
                                         Arg("name"), Arg("guard")).
        define_method("method=", &RubyTransform::setMethod, Arg("method"));
    }
    
    Data_Type<Transform> m_transform;
    Data_Type<RubyTransform> m_rubyTransform;
    Data_Type<Pipeline> m_pipeline;
    Data_Type<adapter::AdapterPipeline> m_adapterPipeline;
    Data_Type<PipelineContext> m_pipelineContext;
  };
}

namespace Rice::detail
{
  template <>
  class From_Ruby<TransformPtr>
  {
  public:
    TransformPtr convert(VALUE value)
    {
      Wrapper* wrapper = detail::getWrapper(value, Data_Type<Transform>::rb_type());

      using Wrapper_T = WrapperSmartPointer<std::shared_ptr, Transform>;
      Wrapper_T* smartWrapper = static_cast<Wrapper_T*>(wrapper);
      std::shared_ptr<Transform> ptr = dynamic_pointer_cast<Transform>(smartWrapper->data());
      
      if (!ptr)
      {
        std::string message = "Invalid smart pointer wrapper";
          throw std::runtime_error(message.c_str());
      }
      return ptr;
    }
  };

}
