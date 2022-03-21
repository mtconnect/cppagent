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

#include "agent.hpp"
#include "source.hpp"
#include "sink.hpp"

#include "ruby_smart_ptr.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect::device_model;
  using namespace std;
  using namespace Rice;

  struct RubyAgent {
    static void initialize(mrb_state *mrb, RClass *module, Agent *agent)
    {
      static mrb_data_type agentType { "Agent", nullptr };
      static mrb_data_type pipelineType { "Pipeline", nullptr };

      auto agentClass = mrb_define_class_under(mrb, module, "Agent", mrb->object_class);
      MRB_SET_INSTANCE_TT(agentClass, MRB_TT_DATA);

      auto wrapper = mrb_data_object_alloc(mrb, agentClass, agent, &agentType);
      auto agentValue = mrb_obj_value(wrapper);
      auto mod = mrb_obj_value(module);
      auto ivar = mrb_intern_cstr(mrb, "@agent");
      mrb_iv_set(mrb, mod, ivar, agentValue);

      mrb_define_class_method(mrb, module, "agent",
        [](mrb_state *mrb, mrb_value self) {
          auto ivar = mrb_intern_cstr(mrb, "@agent");
          return mrb_iv_get(mrb, self, ivar);
        }, MRB_ARGS_NONE());

      auto sinkClass = mrb_define_class_under(mrb, module, "Sink", mrb->object_class);
      MRB_SET_INSTANCE_TT(sinkClass, MRB_TT_DATA);

      auto sourceClass = mrb_define_class_under(mrb, module, "Source", mrb->object_class);
      MRB_SET_INSTANCE_TT(sourceClass, MRB_TT_DATA);
      
      mrb_define_method(mrb, sourceClass, "name", [](mrb_state *mrb, mrb_value self){
        auto source = static_cast<SourcePtr*>(DATA_PTR(self));
        return mrb_str_new_cstr(mrb, (*source)->getName().c_str());
      }, MRB_ARGS_NONE());
      mrb_define_method(mrb, sourceClass, "pipeline", [](mrb_state *mrb, mrb_value self){
        auto source = static_cast<SourcePtr*>(DATA_PTR(self));
        auto mod = mrb_module_get(mrb, "MTConnect");
        auto klass = mrb_class_get_under(mrb, mod, "Pipeline");
        auto wrapper = mrb_data_object_alloc(mrb, klass, (*source)->getPipeline(), &pipelineType);
        return mrb_obj_value(wrapper);
      }, MRB_ARGS_NONE());

      mrb_define_method(mrb, agentClass, "sources", [](mrb_state *mrb, mrb_value self) {
        auto agent = static_cast<Agent*>(DATA_PTR(self));
        auto sources = mrb_ary_new(mrb);
        
        for (auto &source : agent->getSources())
        {
          MRubySharedPtr<Source> ptr(mrb, "Source", source);
          mrb_ary_push(mrb, sources, ptr.m_obj);
        }
        
        return sources;
      }, MRB_ARGS_NONE());

      mrb_define_method(mrb, agentClass, "sinks", [](mrb_state *mrb, mrb_value self) {
        auto agent = static_cast<Agent*>(DATA_PTR(self));
        auto sinks = mrb_ary_new(mrb);
        
        for (auto &sink : agent->getSinks())
        {
          MRubySharedPtr<Sink> ptr(mrb, "Sink", sink);
          mrb_ary_push(mrb, sinks, ptr.m_obj);
        }
        
        return sinks;
      }, MRB_ARGS_NONE());
      
      mrb_define_method(mrb, agentClass, "devices", [](mrb_state *mrb, mrb_value self) {
        auto agent = static_cast<Agent*>(DATA_PTR(self));
        auto devices = mrb_ary_new(mrb);
        
        for (auto &device : agent->getDevices())
        {
          MRubySharedPtr<Device> ptr(mrb, "Device", device);
          mrb_ary_push(mrb, devices, ptr.m_obj);
        }
        
        return devices;
      }, MRB_ARGS_NONE());
      
      mrb_define_method(mrb, agentClass, "data_item_for_device", [](mrb_state *mrb, mrb_value self) {
        auto agent = static_cast<Agent*>(DATA_PTR(self));
        const char *name, *device;
        mrb_get_args(mrb, "zz", &device, &name);

        auto di = agent->getDataItemForDevice(device, name);
        if (di)
        {
          MRubySharedPtr<device_model::data_item::DataItem> ptr(mrb, "DataItem", di);
          return ptr.m_obj;
        }
        else
        {
          return mrb_nil_value();
        }
      }, MRB_ARGS_REQ(2));
      
      mrb_define_method(mrb, agentClass, "device", [](mrb_state *mrb, mrb_value self) {
        auto agent = static_cast<Agent*>(DATA_PTR(self));
        const char *name;
        mrb_get_args(mrb, "z", &name);
        auto device = agent->findDeviceByUUIDorName(name);
        if (device)
        {
          MRubySharedPtr<Device> ptr(mrb, "Device", device);
          return ptr.m_obj;
        }
        else
        {
          return mrb_nil_value();
        }
      }, MRB_ARGS_REQ(1));
    }
  };
}
