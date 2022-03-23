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
#include "entity/entity.hpp"

#include "ruby_smart_ptr.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect;
  using namespace pipeline;
  using namespace entity;
  using namespace std;
  using namespace Rice;
  
  struct RubyPipeline {
    static void initialize(mrb_state *mrb, RClass *module)
    {
      auto pipelineClass = mrb_define_class_under(mrb, module, "Pipeline", mrb->object_class);
      MRB_SET_INSTANCE_TT(pipelineClass, MRB_TT_DATA);

      auto contextClass = mrb_define_class_under(mrb, module, "PipelineContext", mrb->object_class);
      MRB_SET_INSTANCE_TT(contextClass, MRB_TT_DATA);
            
      mrb_define_method(mrb, pipelineClass, "spice_before", [](mrb_state *mrb, mrb_value self) {
        const char *name;
        TransformPtr *transform;
        
        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        mrb_get_args(mrb, "zd", &name, &transform, MRubySharedPtr<Transform>::type());
        pipeline->spliceBefore(name, *transform);
        
        return self;
      }, MRB_ARGS_REQ(2));

      mrb_define_method(mrb, pipelineClass, "splice_after", [](mrb_state *mrb, mrb_value self) {
        const char *name;
        TransformPtr *transform;

        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        mrb_get_args(mrb, "zd", &name, &transform, MRubySharedPtr<Transform>::type());
        pipeline->spliceAfter(name, *transform);
        
        return self;
      }, MRB_ARGS_REQ(2));

      mrb_define_method(mrb, pipelineClass, "first_after", [](mrb_state *mrb, mrb_value self) {
        const char *name;
        TransformPtr *transform;
        
        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        mrb_get_args(mrb, "zd", &name, &transform, MRubySharedPtr<Transform>::type());
        pipeline->firstAfter(name, *transform);
        
        return self;
      }, MRB_ARGS_REQ(2));

      mrb_define_method(mrb, pipelineClass, "last_after", [](mrb_state *mrb, mrb_value self) {
        const char *name;
        TransformPtr *transform;
        
        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        mrb_get_args(mrb, "zd", &name, &transform, MRubySharedPtr<Transform>::type());
        
        pipeline->lastAfter(name, *transform);
        
        return self;
      }, MRB_ARGS_REQ(2));

      mrb_define_method(mrb, pipelineClass, "remove", [](mrb_state *mrb, mrb_value self) {
        const char *name;

        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        mrb_get_args(mrb, "z", &name);
        pipeline->remove(name);
        
        return self;
      }, MRB_ARGS_REQ(1));
      
      mrb_define_method(mrb, pipelineClass, "replace", [](mrb_state *mrb, mrb_value self) {
        const char *name;
        TransformPtr *trans;
        
        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        mrb_get_args(mrb, "zd", &name,
                     &trans, MRubySharedPtr<Transform>::type());
        
        pipeline->replace(name, *trans);
        
        return self;
      }, MRB_ARGS_REQ(2));


      mrb_define_method(mrb, pipelineClass, "run", [](mrb_state *mrb, mrb_value self) {
        EntityPtr *entity;
        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        mrb_get_args(mrb, "d", &entity, MRubySharedPtr<Entity>::type());
        
        EntityPtr ptr = *entity;
        ptr = pipeline->run(ptr);
        
        return MRubySharedPtr<Entity>::wrap(mrb, "Entity", ptr);
      }, MRB_ARGS_REQ(1));
      
      mrb_define_method(mrb, pipelineClass, "context", [](mrb_state *mrb, mrb_value self) {
        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        return MRubySharedPtr<PipelineContext>::wrap(mrb, "PipelineContext", pipeline->getContext());
      }, MRB_ARGS_NONE());
      
      auto adapterPipelineClass = mrb_define_class_under(mrb, module, "AdapterPipeline", pipelineClass);
      MRB_SET_INSTANCE_TT(adapterPipelineClass, MRB_TT_DATA);

      auto loopbackPipelineClass = mrb_define_class_under(mrb, module, "LoopbackPipeline", pipelineClass);
      MRB_SET_INSTANCE_TT(loopbackPipelineClass, MRB_TT_DATA);
      
      mrb_define_method(mrb, pipelineClass, "context", [](mrb_state *mrb, mrb_value self) {
        auto pipeline = MRubyPtr<Pipeline>::unwrap(self);
        return MRubySharedPtr<PipelineContext>::wrap(mrb, "PipelineContext", pipeline->getContext());
      }, MRB_ARGS_NONE());


    }    
  };
}
