//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "mtconnect/config.hpp"
#include "mtconnect/logging.hpp"

// This header assumes <mruby.h> is already included by the translation unit
// (as with the other ruby_*.hpp files); it pulls in the specific mruby headers
// it uses so it does not depend on include order within the TU.
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/debug.h>
#include <mruby/error.h>
#include <mruby/irep.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>

namespace mtconnect::ruby {
  /// @brief A small, cross-platform `require`/`require_relative`/`$LOAD_PATH`
  ///        implementation for the embedded mruby VM.
  ///
  /// Replaces the `mattn/mruby-require` gem, whose build-time git fetch and
  /// runtime shared-library (`@bundled` `.so`/`.dll`) machinery failed to link
  /// into the Windows build (the gem-init that defines `require` and `$:` was
  /// dropped from the generated `gem_init.c`). The agent only ever loads ruby
  /// **source** files from a search path -- never native gems -- so this reuses
  /// the same `mrb_load_file_cxt` loader `LoadModule` uses and behaves
  /// identically on macOS, Linux, and Windows.
  struct RubyRequire
  {
    /// @brief Define the Kernel methods and load-path globals on the VM.
    static void initialize(mrb_state *mrb)
    {
      // LoadError < ScriptError (ScriptError is provided by mruby core).
      RClass *scriptError = mrb_class_get(mrb, "ScriptError");
      RClass *loadError = mrb_define_class(mrb, "LoadError", scriptError);
      (void)loadError;

      auto krn = mrb->kernel_module;
      mrb_define_method(mrb, krn, "require", require_method, MRB_ARGS_REQ(1));
      mrb_define_method(mrb, krn, "require_relative", require_relative_method, MRB_ARGS_REQ(1));
      mrb_define_method(mrb, krn, "load", load_method, MRB_ARGS_REQ(1));

      // $LOAD_PATH ($:) and $LOADED_FEATURES ($") -- create if not already set.
      if (!mrb_array_p(mrb_gv_get(mrb, loadPathSym(mrb))))
        mrb_gv_set(mrb, loadPathSym(mrb), mrb_ary_new(mrb));
      if (!mrb_array_p(mrb_gv_get(mrb, loadedSym(mrb))))
        mrb_gv_set(mrb, loadedSym(mrb), mrb_ary_new(mrb));
    }

    /// @brief Append a directory to `$LOAD_PATH`. Used by the agent to seed the
    ///        module's own directory so a module can `require` its siblings.
    static void addLoadPath(mrb_state *mrb, const std::string &dir)
    {
      mrb_value lp = mrb_gv_get(mrb, loadPathSym(mrb));
      if (!mrb_array_p(lp))
      {
        lp = mrb_ary_new(mrb);
        mrb_gv_set(mrb, loadPathSym(mrb), lp);
      }
      mrb_ary_push(mrb, lp, mrb_str_new_cstr(mrb, dir.c_str()));
    }

  protected:
    static mrb_sym loadPathSym(mrb_state *mrb) { return mrb_intern_lit(mrb, "$:"); }
    static mrb_sym loadedSym(mrb_state *mrb) { return mrb_intern_lit(mrb, "$\""); }

    /// @brief Candidate leaf names for a required feature: try `<name>.rb`
    ///        (adding the extension when missing) and then `<name>` verbatim.
    static std::vector<std::string> candidates(const std::string &base)
    {
      std::vector<std::string> names;
      if (base.size() >= 3 && base.compare(base.size() - 3, 3, ".rb") == 0)
        names.push_back(base);
      else
      {
        names.push_back(base + ".rb");
        names.push_back(base);
      }
      return names;
    }

    static bool alreadyLoaded(mrb_state *mrb, const std::string &canon)
    {
      mrb_value loaded = mrb_gv_get(mrb, loadedSym(mrb));
      if (!mrb_array_p(loaded))
        return false;
      mrb_int n = RARRAY_LEN(loaded);
      for (mrb_int i = 0; i < n; i++)
      {
        mrb_value e = mrb_ary_ref(mrb, loaded, i);
        if (mrb_string_p(e) && canon == RSTRING_CSTR(mrb, e))
          return true;
      }
      return false;
    }

    static void markLoaded(mrb_state *mrb, const std::string &canon)
    {
      mrb_value loaded = mrb_gv_get(mrb, loadedSym(mrb));
      if (!mrb_array_p(loaded))
      {
        loaded = mrb_ary_new(mrb);
        mrb_gv_set(mrb, loadedSym(mrb), loaded);
      }
      mrb_ary_push(mrb, loaded, mrb_str_new_cstr(mrb, canon.c_str()));
    }

    /// @brief Resolve a candidate path to a regular file and return its
    ///        canonical absolute form, if it exists.
    static std::optional<std::string> resolve(const std::filesystem::path &p)
    {
      std::error_code ec;
      if (!std::filesystem::is_regular_file(p, ec))
        return std::nullopt;
      auto canon = std::filesystem::canonical(p, ec);
      return ec ? p.string() : canon.string();
    }

    /// @brief Load and execute a ruby source file. Leaves any raised exception
    ///        in `mrb->exc` so it propagates to the caller.
    static void executeFile(mrb_state *mrb, const std::string &path)
    {
      FILE *fp = fopen(path.c_str(), "r");
      if (fp == nullptr)
      {
        mrb_raisef(mrb, mrb_class_get(mrb, "LoadError"), "cannot open file -- %s", path.c_str());
        return;
      }

      int ai = mrb_gc_arena_save(mrb);
      auto ctx = mrbc_context_new(mrb);
      mrbc_filename(mrb, ctx, path.c_str());
      mrb_load_file_cxt(mrb, fp, ctx);
      mrbc_context_free(mrb, ctx);
      fclose(fp);
      mrb_gc_arena_restore(mrb, ai);
    }

    /// @brief The source file of the ruby frame that called us, for
    ///        `require_relative`. Returns nullopt for C-function callers or when
    ///        no debug filename is available.
    static std::optional<std::string> callerFile(mrb_state *mrb)
    {
      if (mrb->c == nullptr || mrb->c->ci <= mrb->c->cibase)
        return std::nullopt;

      mrb_callinfo *caller = mrb->c->ci - 1;
      const struct RProc *proc = caller->proc;
      if (proc == nullptr || MRB_PROC_CFUNC_P(proc))
        return std::nullopt;

      const mrb_irep *irep = proc->body.irep;
      if (irep == nullptr)
        return std::nullopt;

      uint32_t pc = 0;
      if (caller->pc != nullptr && irep->iseq != nullptr)
        pc = static_cast<uint32_t>(caller->pc - irep->iseq);

      const char *fn = mrb_debug_get_filename(mrb, irep, pc);
      if (fn == nullptr)
        return std::nullopt;
      return std::string(fn);
    }

    /// @brief `require` -- searches `$LOAD_PATH` (unless an explicit relative or
    ///        absolute path is given), loads once, tracks `$"`.
    static mrb_value require_method(mrb_state *mrb, mrb_value self)
    {
      namespace fs = std::filesystem;
      const char *name = nullptr;
      mrb_get_args(mrb, "z", &name);
      std::string req(name);

      auto loadResolved = [&](const std::optional<std::string> &canon) -> int {
        if (!canon)
          return -1;  // not a file
        if (alreadyLoaded(mrb, *canon))
          return 0;  // already loaded
        markLoaded(mrb, *canon);  // mark before executing to break require cycles
        executeFile(mrb, *canon);
        return 1;  // loaded
      };

      const bool explicitPath = fs::path(req).is_absolute() || startsWith(req, "./") ||
                                startsWith(req, "../") || startsWith(req, ".\\") ||
                                startsWith(req, "..\\");

      if (explicitPath)
      {
        for (auto &c : candidates(req))
        {
          int r = loadResolved(resolve(fs::path(c)));
          if (r >= 0)
            return r == 1 ? mrb_true_value() : mrb_false_value();
        }
      }
      else
      {
        mrb_value lp = mrb_gv_get(mrb, loadPathSym(mrb));
        if (mrb_array_p(lp))
        {
          mrb_int n = RARRAY_LEN(lp);
          for (mrb_int i = 0; i < n; i++)
          {
            mrb_value d = mrb_ary_ref(mrb, lp, i);
            if (!mrb_string_p(d))
              continue;
            fs::path dir(RSTRING_CSTR(mrb, d));
            for (auto &c : candidates(req))
            {
              int r = loadResolved(resolve(dir / c));
              if (r >= 0)
                return r == 1 ? mrb_true_value() : mrb_false_value();
            }
          }
        }
      }

      mrb_raisef(mrb, mrb_class_get(mrb, "LoadError"), "cannot load such file -- %s", name);
      return mrb_false_value();
    }

    /// @brief `require_relative` -- resolves relative to the calling file.
    static mrb_value require_relative_method(mrb_state *mrb, mrb_value self)
    {
      namespace fs = std::filesystem;
      const char *name = nullptr;
      mrb_get_args(mrb, "z", &name);

      auto base = callerFile(mrb);
      fs::path baseDir;
      if (base && *base != "-")
        baseDir = fs::path(*base).parent_path();

      fs::path target = baseDir.empty() ? fs::path(name) : baseDir / name;
      for (auto &c : candidates(target.string()))
      {
        auto canon = resolve(fs::path(c));
        if (!canon)
          continue;
        if (alreadyLoaded(mrb, *canon))
          return mrb_false_value();
        markLoaded(mrb, *canon);
        executeFile(mrb, *canon);
        return mrb_true_value();
      }

      mrb_raisef(mrb, mrb_class_get(mrb, "LoadError"), "cannot load such file -- %s", name);
      return mrb_false_value();
    }

    /// @brief `load` -- always executes, no `$"` tracking; searches `$LOAD_PATH`
    ///        when the argument is not an explicit path.
    static mrb_value load_method(mrb_state *mrb, mrb_value self)
    {
      namespace fs = std::filesystem;
      const char *name = nullptr;
      mrb_get_args(mrb, "z", &name);
      std::string arg(name);

      const bool explicitPath = fs::path(arg).is_absolute() || startsWith(arg, "./") ||
                                startsWith(arg, "../") || startsWith(arg, ".\\") ||
                                startsWith(arg, "..\\");

      if (explicitPath)
      {
        if (auto canon = resolve(fs::path(arg)))
        {
          executeFile(mrb, *canon);
          return mrb_true_value();
        }
      }
      else
      {
        // Try the current directory first, then the load path.
        if (auto canon = resolve(fs::path(arg)))
        {
          executeFile(mrb, *canon);
          return mrb_true_value();
        }
        mrb_value lp = mrb_gv_get(mrb, loadPathSym(mrb));
        if (mrb_array_p(lp))
        {
          mrb_int n = RARRAY_LEN(lp);
          for (mrb_int i = 0; i < n; i++)
          {
            mrb_value d = mrb_ary_ref(mrb, lp, i);
            if (!mrb_string_p(d))
              continue;
            if (auto canon = resolve(fs::path(RSTRING_CSTR(mrb, d)) / arg))
            {
              executeFile(mrb, *canon);
              return mrb_true_value();
            }
          }
        }
      }

      mrb_raisef(mrb, mrb_class_get(mrb, "LoadError"), "cannot load such file -- %s", name);
      return mrb_false_value();
    }

    static bool startsWith(const std::string &s, const char *prefix)
    {
      return s.rfind(prefix, 0) == 0;
    }
  };
}  // namespace mtconnect::ruby
