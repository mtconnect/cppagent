//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/agent.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/sink/sink.hpp"
#include "mtconnect/source/source.hpp"
#include "ruby_smart_ptr.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect::device_model;
  using namespace std;

  /// @brief Wrapper around the MTConnect Agent class
  struct RubyAgent
  {
    /// @brief initialize the Ruby Agent class associating the `agent`
    /// @param[in] mrb The ruby state
    /// @param[in] module Module to instantiate agent class
    /// @param[in] agent The agent
    static void initialize(mrb_state *mrb, RClass *module, Agent *agent)
    {
      auto agentClass = mrb_define_class_under(mrb, module, "Agent", mrb->object_class);
      MRB_SET_INSTANCE_TT(agentClass, MRB_TT_DATA);

      mrb_value agentValue = MRubyPtr<Agent>::wrap(mrb, agentClass, agent);
      auto ivar = mrb_intern_cstr(mrb, "@agent");
      auto mod = mrb_obj_value(module);
      mrb_iv_set(mrb, mod, ivar, agentValue);

      mrb_define_class_method(
          mrb, module, "agent",
          [](mrb_state *mrb, mrb_value self) {
            auto ivar = mrb_intern_cstr(mrb, "@agent");
            return mrb_iv_get(mrb, self, ivar);
          },
          MRB_ARGS_NONE());

      auto sinkClass = mrb_define_class_under(mrb, module, "Sink", mrb->object_class);
      MRB_SET_INSTANCE_TT(sinkClass, MRB_TT_DATA);

      auto sourceClass = mrb_define_class_under(mrb, module, "Source", mrb->object_class);
      MRB_SET_INSTANCE_TT(sourceClass, MRB_TT_DATA);

      mrb_define_method(
          mrb, sourceClass, "name",
          [](mrb_state *mrb, mrb_value self) {
            auto source = MRubySharedPtr<source::Source>::unwrap(mrb, self);
            return mrb_str_new_cstr(mrb, source->getName().c_str());
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, sourceClass, "pipeline",
          [](mrb_state *mrb, mrb_value self) {
            auto source = MRubySharedPtr<source::Source>::unwrap(mrb, self);
            return MRubyPtr<pipeline::Pipeline>::wrap(mrb, "Pipeline", source->getPipeline());
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, agentClass, "sources",
          [](mrb_state *mrb, mrb_value self) {
            auto agent = MRubyPtr<Agent>::unwrap(mrb, self);
            auto sources = mrb_ary_new(mrb);

            for (auto &source : agent->getSources())
            {
              auto obj = MRubySharedPtr<source::Source>::wrap(mrb, "Source", source);
              mrb_ary_push(mrb, sources, obj);
            }

            return sources;
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, agentClass, "sinks",
          [](mrb_state *mrb, mrb_value self) {
            auto agent = MRubyPtr<Agent>::unwrap(mrb, self);
            auto sinks = mrb_ary_new(mrb);

            for (auto &sink : agent->getSinks())
            {
              auto obj = MRubySharedPtr<sink::Sink>::wrap(mrb, "Sink", sink);
              mrb_ary_push(mrb, sinks, obj);
            }

            return sinks;
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, agentClass, "devices",
          [](mrb_state *mrb, mrb_value self) {
            auto agent = MRubyPtr<Agent>::unwrap(mrb, self);
            auto devices = mrb_ary_new(mrb);

            auto mod = mrb_module_get(mrb, "MTConnect");
            auto klass = mrb_class_get_under(mrb, mod, "Device");

            for (auto &device : agent->getDevices())
            {
              auto obj = MRubySharedPtr<entity::Entity>::wrap(mrb, klass, device);
              mrb_ary_push(mrb, devices, obj);
            }

            return devices;
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, agentClass, "default_device",
          [](mrb_state *mrb, mrb_value self) {
            auto agent = MRubyPtr<Agent>::unwrap(mrb, self);
            auto dev = agent->getDefaultDevice();

            auto mod = mrb_module_get(mrb, "MTConnect");
            auto klass = mrb_class_get_under(mrb, mod, "Device");

            if (dev)
              return MRubySharedPtr<entity::Entity>::wrap(mrb, klass, dev);
            else
              return mrb_nil_value();
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, agentClass, "data_item_for_device",
          [](mrb_state *mrb, mrb_value self) {
            auto agent = MRubyPtr<Agent>::unwrap(mrb, self);
            const char *name, *device;
            mrb_get_args(mrb, "zz", &device, &name);

            auto mod = mrb_module_get(mrb, "MTConnect");
            auto klass = mrb_class_get_under(mrb, mod, "DataItem");

            auto di = agent->getDataItemForDevice(device, name);
            if (di)
            {
              return MRubySharedPtr<entity::Entity>::wrap(mrb, klass, di);
            }
            else
            {
              return mrb_nil_value();
            }
          },
          MRB_ARGS_REQ(2));

      mrb_define_method(
          mrb, agentClass, "device",
          [](mrb_state *mrb, mrb_value self) {
            auto agent = MRubyPtr<Agent>::unwrap(mrb, self);
            const char *name;
            mrb_get_args(mrb, "z", &name);
            auto device = agent->findDeviceByUUIDorName(name);
            if (device)
            {
              return MRubySharedPtr<entity::Entity>::wrap(mrb, "Device", device);
            }
            else
            {
              return mrb_nil_value();
            }
          },
          MRB_ARGS_REQ(1));
    }
  };

  /// @struct RubyAgent
  /// @remark Ruby Agent Wrapper
  /// @code
  /// class Agent -> mtconnect::Agent
  ///   def sources -> mtconnect::Agent::getSources()
  ///   def sinks -> mtconnect::Agent::getSinks()
  ///   def devices -> mtconnect::Agent::getDevices()
  ///   def default_device -> mtconnect::Agent::getDefaultDevice()
  ///   def data_item_for_device(device, name) -> mtconnect::Agent::getDataItemForDevice(device,
  ///   name) def device(name) -> mtconnect::Agent::findDeviceByUUIDorName(name)
  /// end
  /// @endcode
  ///
  /// @remark Ruby Source Wrapper
  /// @code
  /// class Source -> mtconnect::source::Source
  ///   def name -> mtconnect::source::Source::getName()
  ///   def pipeline -> mtconnect::source::Source::getPipeline()
  /// end
  /// @endcode
  ///
  /// @remark Ruby Sink Wrapper
  /// @code
  /// class Sink -> mtconnect::sink::Sink
  /// end
  /// @endcode

}  // namespace mtconnect::ruby
