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

#include "utilities.hpp"

#include <iostream>
#include <string>
#include <date/date.h>
#include <filesystem>

#include <boost/algorithm/string.hpp>

#include "agent.hpp"
#include "pipeline/pipeline.hpp"
#include "entity/entity.hpp"
#include "entity/data_set.hpp"
#include "embedded.hpp"
#include "logging.hpp"

#if defined(_WIN32) || defined(_WIN64)
#undef timezone
#undef WSAPPI
#define WSAAPI 
#endif

#include <mruby.h>
#include <mruby/data.h>
#include <mruby/array.h>
#include <mruby/compile.h>
#include <mruby/dump.h>
#include <mruby/variable.h>
#include <mruby/proc.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/string.h>
#include <mruby/presym.h>
#include <mruby/error.h>

using namespace std;

namespace mtconnect::ruby {
  using namespace mtconnect::pipeline;
  using namespace std::literals;
  using namespace date::literals;
  using namespace observation;

  static mrb_sym s_AgentIvar;

  struct RubyVM
  {
    RubyVM(Agent *agent)
    {
      m_agentPtr = agent;
      mrb = mrb_open();
      if (!mrb)
      {
        /* handle error */
        throw std::runtime_error("Cannot start mrb");
      }
      // mrb_load_string(mrb, str) to load from NULL terminated strings
      // mrb_load_nstring(mrb, str, len) for strings without null terminator or with known length
    }
    
    ~RubyVM()
    {
      if (mrb)
      {
        mrb_close(mrb);
      }
    }

    std::mutex m_mutex;
    
    Agent *m_agentPtr;
    RClass *m_module  = nullptr;
    RClass *m_agentClass = nullptr;
    mrb_value m_agent;
    mrb_state *mrb = nullptr;

    void createModule()
    {
      static mrb_data_type agentType { "Agent", nullptr };
      
      m_module = mrb_define_module(mrb, "MTConnect");
      m_agentClass = mrb_define_class_under(mrb, m_module, "Agent", mrb->object_class);
      MRB_SET_INSTANCE_TT(m_agentClass, MRB_TT_DATA);

      auto wrapper = mrb_data_object_alloc(mrb, m_agentClass, m_agentPtr, &agentType);
      m_agent = mrb_obj_value(wrapper);
      auto mod = mrb_obj_value(m_module);
      s_AgentIvar = mrb_intern_cstr(mrb, "@agent");
      mrb_iv_set(mrb, mod, s_AgentIvar, m_agent);

      mrb_define_class_method(mrb, m_module, "agent",
        [](mrb_state *mrb, mrb_value self) {
          return mrb_iv_get(mrb, self, s_AgentIvar);
        }, MRB_ARGS_NONE());
    }

    template<typename L>
    static inline void log(L level, mrb_state *mrb)
    {
      mrb_value msg;
      mrb_get_args(mrb, "S", &msg);
      BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),
                                   (::boost::log::keywords::severity = level)) << mrb_str_to_cstr(mrb, msg);
    }
    
    void defineLogger()
    {
      // Create a module with logging methods for each severity level
      auto logger = mrb_define_module_under(mrb, m_module, "Logger");
      mrb_define_class_method(mrb, logger, "debug", [](mrb_state *mrb, mrb_value self) {
        log(::boost::log::trivial::debug, mrb);
        return mrb_nil_value();
      }, MRB_ARGS_REQ(1));
      mrb_define_class_method(mrb, logger, "trace", [](mrb_state *mrb, mrb_value self) {
        log(::boost::log::trivial::trace, mrb);
        return mrb_nil_value();
      }, MRB_ARGS_REQ(1));
      mrb_define_class_method(mrb, logger, "info", [](mrb_state *mrb, mrb_value self) {
        log(::boost::log::trivial::info, mrb);
        return mrb_nil_value();
      }, MRB_ARGS_REQ(1));
      mrb_define_class_method(mrb, logger, "warning", [](mrb_state *mrb, mrb_value self) {
        log(::boost::log::trivial::warning, mrb);
        return mrb_nil_value();
      }, MRB_ARGS_REQ(1));
      mrb_define_class_method(mrb, logger, "error", [](mrb_state *mrb, mrb_value self) {
        log(::boost::log::trivial::error, mrb);
        return mrb_nil_value();
      }, MRB_ARGS_REQ(1));
      mrb_define_class_method(mrb, logger, "fatal", [](mrb_state *mrb, mrb_value self) {
        log(::boost::log::trivial::fatal, mrb);
        return mrb_nil_value();
      }, MRB_ARGS_REQ(1));
    }
  };
  
  // These are static wrapper classes that add types to the Ruby instance
  Embedded::Embedded(Agent *agent, const ConfigOptions &options)
    : m_agent(agent), m_options(options)
  {
    using namespace std::filesystem;
    
    NAMED_SCOPE("Ruby::Embedded");
    
    // Load the ruby module in the configuration
    auto module = GetOption<string>(m_options, "module");
    auto initialization = GetOption<string>(m_options, "initialization");
    
    if (!m_rubyVM.has_value())
    {
      auto vm = new RubyVM(agent);
      m_rubyVM = vm;
      
      vm->createModule();
      vm->defineLogger();
      
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
          FILE *fp = nullptr;
          try {
            FILE *fp = fopen(mod.c_str(), "r");
            mrb_load_file(vm->mrb, fp);
          } catch (std::exception ex) {
            LOG(error) << "Failed to load module: " << mod << ": " << ex.what();
          }
          catch (...)
          {
            LOG(error) << "Failed to load module: " << mod;
          }
          if (fp != nullptr)
          {
            fclose(fp);
          }
        }
      }
    }
  }
  
  Embedded::~Embedded()
  {
    
  }
}
