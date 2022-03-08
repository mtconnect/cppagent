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

#include "embedded.hpp"

#include <iostream>
#include <string>
#include <date/date.h>
#include <filesystem>

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "device_model/device.hpp"
#include "entity.hpp"
#include "pipeline/guard.hpp"
#include "pipeline/transform.hpp"
#include "logging.hpp"

#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>

#include "ruby_type.hpp"
#include "ruby_agent.hpp"
#include "ruby_entity.hpp"
#include "ruby_pipeline.hpp"
#include "ruby_observation.hpp"
#include "ruby_transform.hpp"

extern "C" void Init_ext(void);

namespace mtconnect::ruby {
  using namespace mtconnect::pipeline;
  using namespace Rice;
  using namespace Rice::detail;
  using namespace std::literals;
  using namespace date::literals;
  using namespace observation;

  // These are static wrapper classes that add types to the Ruby instance
  static optional<RubyAgent> s_RubyAgent;
  static optional<RubyEntity> s_RubyEntity;
  static optional<RubyPipeline> s_RubyPipeline;
  static optional<RubyObservation> s_RubyObservation;
  
  void Embedded::createModule()
  {
    m_module = make_unique<Module>(define_module("MTConnect"));
    s_RubyAgent.emplace();
    s_RubyEntity.emplace();
    s_RubyPipeline.emplace();
    s_RubyObservation.emplace();
  }
    
  Embedded::Embedded(Agent *agent, const ConfigOptions &options)
    : m_agent(agent), m_options(options)
  {
    using namespace std::filesystem;
    
    NAMED_SCOPE("Ruby::Embedded::Embedded");
    
    // Load the ruby module in the configuration
    auto module = GetOption<string>(m_options, "module");
    auto initialization = GetOption<string>(m_options, "initialization");
    
    char **pargv = new char*[3];
    int argc = 0;
    pargv[argc++] = strdup("agent");

    if (module)
    {
      LOG(info) << "Finding module: " << *module;
      path mod(*module);
      std::error_code ec;
      path file = canonical(mod, ec);
      if (ec)
      {
        LOG(error) << "Cannot open file: " << ec.message();
      }
      else
      {
        LOG(info) << "Found module: " << file;
        pargv[argc++] = strdup(file.string().c_str());
      }
    }
    
    if (argc == 1)
    {
      pargv[argc++]  = strdup("-e");
      pargv[argc++]  = strdup("puts 'starting ruby'");
    }

    RUBY_INIT_STACK;
    
    ruby_sysinit(&argc, (char***) &pargv);
    ruby_init();

    Init_ext();

    using namespace device_model;
    
    // Create the module and all the ruby wrapper classes
    createModule();
    s_RubyAgent->create(*m_module);
    s_RubyEntity->create(*m_module);
    s_RubyPipeline->create(*m_module);
    s_RubyObservation->create(*m_module);
    
    // Add the methods to the wrapper classes
    s_RubyAgent->methods();
    s_RubyEntity->methods();
    s_RubyPipeline->methods();
    s_RubyObservation->methods();
    
    // Singleton methods to retrieve the agent from the MTConnect module
    m_module->define_singleton_method("agent", [this](Object self) {
      return m_agent;
    });
    
    // Create a module with logging methods for each severity level
    define_module_under(*m_module, "Logger").
      define_singleton_method("debug", [](Object self, const string &message) {
        LOG(debug) << message;
      }, Arg("message")).
      define_singleton_method("trace", [](Object self, const string &message) {
        LOG(trace) << message;
      }, Arg("message")).
      define_singleton_method("info", [](Object self, const string &message) {
        LOG(info) << message;
      }, Arg("message")).
      define_singleton_method("warning", [](Object self, const string &message) {
        LOG(warning) << message;
      }, Arg("message")).
      define_singleton_method("error", [](Object self, const string &message) {
        LOG(error) << message;
      }, Arg("message")).
      define_singleton_method("fatal", [](Object self, const string &message) {
        LOG(fatal) << message;
      }, Arg("message"));

    int state;
    ruby_exec_node(ruby_options(argc, pargv));

    if (initialization)
    {
      rb_eval_string_protect(initialization->c_str(), &state);
    }
    
    for (int i = 0; i < argc; i++)
      free(pargv[i]);
    delete[] pargv;
  }
  
  Embedded::~Embedded()
  {
  }

  
  void *runWithoutGil(void *context)
  {
    boost::asio::io_context *ctx = static_cast<boost::asio::io_context*>(context);
    ctx->run();
    return nullptr;
  }
  
  VALUE boostThread(void *context)
  {
    rb_thread_call_without_gvl(&runWithoutGil, context,
                               NULL, NULL);
    return Qnil;
  }
  
  void Embedded::start(boost::asio::io_context &context, int count)
  {
    std::list<VALUE> threads;
    for (int i = 0; i < count; i++)
    {
      VALUE thread = rb_thread_create(&boostThread, &context);
      threads.push_back(thread);
      rb_thread_run(thread);
    }
    
    for (auto &v : threads)
    {
      rb_funcall(v, rb_intern("join"), 0);
    }
  }
}
