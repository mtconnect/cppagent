//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <date/date.h>
#include <filesystem>
#include <iostream>
#include <string>

#include "agent.hpp"
#include "entity/data_set.hpp"
#include "entity/entity.hpp"
#include "logging.hpp"
#include "pipeline/pipeline.hpp"
#include "utilities.hpp"

#ifdef _WINDOWS
#undef timezone
#endif

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/data.h>
#include <mruby/dump.h>
#include <mruby/error.h>
#include <mruby/numeric.h>
#include <mruby/presym.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/throw.h>
#include <mruby/variable.h>

#include "ruby_agent.hpp"
#include "ruby_entity.hpp"
#include "ruby_observation.hpp"
#include "ruby_pipeline.hpp"
#include "ruby_transform.hpp"
#include "ruby_vm.hpp"

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

      lock_guard guard(*m_rubyVM);

      auto mrb = m_rubyVM->state();

      RubyAgent::initialize(mrb, m_rubyVM->mtconnect(), agent);
      RubyPipeline::initialize(mrb, m_rubyVM->mtconnect());
      RubyEntity::initialize(mrb, m_rubyVM->mtconnect());
      RubyObservation::initialize(mrb, m_rubyVM->mtconnect());
      RubyTransform::initialize(mrb, m_rubyVM->mtconnect());

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
          try
          {
            int save = mrb_gc_arena_save(mrb);
            mrb_value file = mrb_str_new_cstr(mrb, mod.string().c_str());
            mrb_bool state;
            mrb_value res = mrb_protect(
                mrb,
                [](mrb_state *mrb, mrb_value data) {
                  FILE *fp = fopen(mrb_str_to_cstr(mrb, data), "r");
                  return mrb_load_file(mrb, fp);
                },
                file, &state);
            mrb_gc_arena_restore(mrb, save);
            if (state)
            {
              LOG(fatal) << "Error loading file " << mod << ": "
                         << mrb_str_to_cstr(mrb, mrb_inspect(mrb, res));
              exit(1);
            }
          }
          catch (std::exception ex)
          {
            LOG(fatal) << "Failed to load module: " << mod << ": " << ex.what();
            exit(1);
          }
          catch (...)
          {
            LOG(fatal) << "Failed to load module: " << mod;
            exit(1);
          }
          if (fp != nullptr)
          {
            fclose(fp);
          }
        }
      }
    }
  }

  Embedded::~Embedded() { m_rubyVM.reset(); }
}  // namespace mtconnect::ruby
