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

#include "ruby_vm.hpp"
#include "ruby_agent.hpp"
#include "ruby_pipeline.hpp"


using namespace std;

namespace mtconnect::ruby {
  using namespace mtconnect::pipeline;
  using namespace std::literals;
  using namespace date::literals;
  using namespace observation;
  
  std::recursive_mutex RubyVM::m_mutex;
  RubyVM *RubyVM::m_vm = nullptr;

  
  // These are static wrapper classes that add types to the Ruby instance
  Embedded::Embedded(Agent *agent, const ConfigOptions &options)
    : m_agent(agent), m_options(options)
  {
    using namespace std::filesystem;
    
    NAMED_SCOPE("Ruby::Embedded");
    
    // Load the ruby module in the configuration
    auto module = GetOption<string>(m_options, "module");
    auto initialization = GetOption<string>(m_options, "initialization");
    
    if (!m_rubyVM)
    {
      m_rubyVM = make_unique<RubyVM>();
      
      auto mrb = m_rubyVM->state();
      
      RubyAgent::initialize(mrb, m_rubyVM->mtconnect(), agent);
      RubyPipeline::initialize(mrb, m_rubyVM->mtconnect());
      
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
            mrb_load_file(mrb, fp);
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
    m_rubyVM.reset();
  }
}
