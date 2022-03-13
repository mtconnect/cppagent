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

#include <boost/algorithm/string.hpp>

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

namespace mtconnect::ruby {
  using namespace mtconnect::pipeline;
  using namespace Rice;
  using namespace Rice::detail;
  using namespace std::literals;
  using namespace date::literals;
  using namespace observation;

  struct RubyVM
  {
    RubyVM() {}
    ~RubyVM();
    void createModule();
    
    optional<RubyAgent> m_RubyAgent;
    optional<RubyEntity> m_RubyEntity;
    optional<RubyPipeline> m_RubyPipeline;
    optional<RubyTransformClass> m_RubyTransformClass;
    optional<RubyObservation> m_RubyObservation;
    optional<Rice::Module> m_Module;
    
    Agent *m_agent = nullptr;
  };
  
  // These are static wrapper classes that add types to the Ruby instance
  static optional<RubyVM> s_RubyVM;
  
  void defineLogger(Rice::Module &module)
  {
    // Create a module with logging methods for each severity level
    define_module_under(module, "Logger").
      define_singleton_function("debug", [](const string message) {
          LOG(debug) << message;
        }, Arg("message")).
      define_singleton_function("trace", [](const string message) {
          LOG(trace) << message;
        }, Arg("message")).
      define_singleton_function("info", [](const string message) {
          LOG(info) << message;
        }, Arg("message")).
      define_singleton_function("warning", [](const string message) {
          LOG(warning) << message;
        }, Arg("message")).
      define_singleton_function("error", [](const string message) {
          LOG(error) << message;
        }, Arg("message")).
      define_singleton_function("fatal", [](const string message) {
          LOG(fatal) << message;
        }, Arg("message"));
  }
  
  void RubyVM::createModule()
  {
    m_Module.emplace(define_module("MTConnect"));
    m_RubyAgent.emplace();
    m_RubyEntity.emplace();
    m_RubyPipeline.emplace();
    m_RubyTransformClass.emplace();
    m_RubyObservation.emplace();
    
    m_RubyAgent->create(*m_Module);
    m_RubyEntity->create(*m_Module);
    m_RubyPipeline->create(*m_Module);
    m_RubyTransformClass->create(*m_Module);
    m_RubyObservation->create(*m_Module);

    m_RubyAgent->methods();
    m_RubyEntity->methods();
    m_RubyPipeline->methods();
    m_RubyTransformClass->methods();
    m_RubyObservation->methods();
    
    m_Module->define_singleton_method("agent", [this](Object self) {
      return m_agent;
    });
    
    defineLogger(*s_RubyVM->m_Module);
  }

    
  Embedded::Embedded(Agent *agent, const ConfigOptions &options)
    : m_agent(agent), m_options(options)
  {
    using namespace std::filesystem;
    
    NAMED_SCOPE("Ruby::Embedded");
    
    // Load the ruby module in the configuration
    auto module = GetOption<string>(m_options, "module");
    auto initialization = GetOption<string>(m_options, "initialization");
    
    if (!s_RubyVM)
    {
      s_RubyVM.emplace();
      s_RubyVM->m_agent = agent;
            
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
            
      using namespace device_model;
      
      // Create the module and all the ruby wrapper classes
      s_RubyVM->createModule();
      
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
    else
    {
      s_RubyVM->m_agent = agent;
      int state;

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
          rb_load_file(file.string().c_str());
        }
      }

      if (initialization)
      {
        rb_eval_string_protect(initialization->c_str(), &state);
      }
    }
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
    m_context = &context;
    
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
  
  void Embedded::stop()
  {
    if (m_context != nullptr)
    {
      m_context->stop();
      m_context = nullptr;
    }
  }
  
  RubyVM::~RubyVM()
  {
    m_RubyAgent.reset();
    m_RubyEntity.reset();
    m_RubyPipeline.reset();
    m_RubyTransformClass.reset();
    m_RubyObservation.reset();
    m_Module.reset();

    ruby_finalize();
  }
  
}
