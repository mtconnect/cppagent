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

#include "mtconnect/agent.hpp"
#include "mtconnect/configuration/agent_config.hpp"
#include "mtconnect/entity/data_set.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/logging.hpp"
#include "mtconnect/pipeline/pipeline.hpp"
#include "mtconnect/utilities.hpp"

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

  RClass *RubyObservation::m_eventClass;
  RClass *RubyObservation::m_sampleClass;
  RClass *RubyObservation::m_conditionClass;

  std::recursive_mutex RubyVM::m_mutex;
  RubyVM *RubyVM::m_vm = nullptr;

  static mrb_value LoadModule(mrb_state *mrb, mrb_value &filename)
  {
    auto fname = RSTRING_CSTR(mrb, filename);
    int ai = mrb_gc_arena_save(mrb);

    auto fp = fopen(fname, "r");
    if (fp == NULL)
    {
      LOG(error) << "Cannot open file " << fname << " for read";

      auto mesg = mrb_str_new_cstr(mrb, "cannot load file");
      mrb_str_cat_lit(mrb, mesg, " -- ");
      mrb_str_cat_str(mrb, mesg, filename);
      auto exc = mrb_funcall(mrb, mrb_obj_value(mrb_class_get(mrb, "LoadError")), "new", 1, mesg);
      mrb_iv_set(mrb, exc, mrb_intern_lit(mrb, "path"), mrb_str_new_cstr(mrb, fname));

      mrb_exc_raise(mrb, exc);
      return mrb_false_value();
    }

    auto mrbc_ctx = mrbc_context_new(mrb);

    mrbc_filename(mrb, mrbc_ctx, fname);
    auto status = mrb_load_file_cxt(mrb, fp, mrbc_ctx);
    fclose(fp);

    mrb_gc_arena_restore(mrb, ai);
    mrbc_context_free(mrb, mrbc_ctx);

    if (mrb_nil_p(status))
    {
      LOG(error) << "Failed to load module: " << fname;
      return mrb_false_value();
    }
    else
    {
      LOG(debug) << "Loaded ruby modeule: " << fname;
      return mrb_true_value();
    }
  }

  Embedded::Embedded(mtconnect::configuration::AgentConfiguration *config,
                     const ConfigOptions &options)
    : m_agent(config->getAgent()), m_options(options)
  {
    using namespace std::filesystem;

    NAMED_SCOPE("Ruby::Embedded");

    // Load the ruby module in the configuration
    auto module = GetOption<string>(m_options, "Module");
    auto initialization = GetOption<string>(m_options, "Initialization");

    if (!module)
      module = GetOption<string>(m_options, "module");
    if (!initialization)
      initialization = GetOption<string>(m_options, "initialization");

    std::optional<std::filesystem::path> modulePath;
    if (module)
      modulePath = config->findDataFile(*module);

    if (!m_rubyVM)
    {
      m_rubyVM = make_unique<RubyVM>();

      lock_guard guard(*m_rubyVM);

      auto mrb = m_rubyVM->state();

      RubyAgent::initialize(mrb, m_rubyVM->mtconnect(), m_agent);
      RubyPipeline::initialize(mrb, m_rubyVM->mtconnect());
      RubyEntity::initialize(mrb, m_rubyVM->mtconnect());
      RubyObservation::initialize(mrb, m_rubyVM->mtconnect());
      RubyTransform::initialize(mrb, m_rubyVM->mtconnect());

      if (modulePath)
      {
        LOG(info) << "Finding module: " << *module;

        std::error_code ec;
        path file = canonical(*modulePath, ec);
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
            mrb_value file = mrb_str_new_cstr(mrb, modulePath->string().c_str());
            mrb_bool state = false;
            mrb_value res = mrb_protect(
                mrb, [](mrb_state *mrb, mrb_value filename) { return LoadModule(mrb, filename); },
                file, &state);
            mrb_gc_arena_restore(mrb, save);
            if (state)
            {
              LOG(fatal) << "Error loading file " << *modulePath << ": "
                         << mrb_str_to_cstr(mrb, mrb_inspect(mrb, res));
              exit(1);
            }
          }
          catch (std::exception ex)
          {
            LOG(fatal) << "Failed to load module: " << *modulePath << ": " << ex.what();
            exit(1);
          }
          catch (...)
          {
            LOG(fatal) << "Failed to load module: " << *modulePath;
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
