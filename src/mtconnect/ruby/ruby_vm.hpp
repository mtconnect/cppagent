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

#include "mtconnect/config.hpp"

namespace mtconnect::ruby {
  class AGENT_LIB_API RubyVM
  {
  public:
    RubyVM()
    {
      m_mrb = mrb_open();
      if (!m_mrb)
      {
        /* handle error */
        throw std::runtime_error("Cannot start mrb");
      }

      createModule();
      defineLogger();

      m_vm = this;
    }

    ~RubyVM()
    {
      m_vm = nullptr;
      std::lock_guard guard(m_mutex);
      if (m_mrb)
      {
        mrb_close(m_mrb);
      }
    }

    auto state() { return m_mrb; }
    auto mtconnect() { return m_module; }

    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    [[nodiscard]] bool try_lock() { return m_mutex.try_lock(); }

    static auto &rubyVM() { return *m_vm; }
    static bool hasVM() { return m_vm != nullptr; }

  protected:
    void createModule() { m_module = mrb_define_module(m_mrb, "MTConnect"); }

    template <typename L>
    static inline void log(L level, mrb_state *mrb)
    {
      mrb_value msg;
      mrb_get_args(mrb, "S", &msg);
      BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),
                                   (::boost::log::keywords::severity = level))
          << mrb_str_to_cstr(mrb, msg);
    }

    void defineLogger()
    {
      // Create a module with logging methods for each severity level
      auto logger = mrb_define_module_under(m_mrb, m_module, "Logger");
      mrb_define_class_method(
          m_mrb, logger, "debug",
          [](mrb_state *mrb, mrb_value self) {
            log(::boost::log::trivial::debug, mrb);
            return mrb_nil_value();
          },
          MRB_ARGS_REQ(1));
      mrb_define_class_method(
          m_mrb, logger, "trace",
          [](mrb_state *mrb, mrb_value self) {
            log(::boost::log::trivial::trace, mrb);
            return mrb_nil_value();
          },
          MRB_ARGS_REQ(1));
      mrb_define_class_method(
          m_mrb, logger, "info",
          [](mrb_state *mrb, mrb_value self) {
            log(::boost::log::trivial::info, mrb);
            return mrb_nil_value();
          },
          MRB_ARGS_REQ(1));
      mrb_define_class_method(
          m_mrb, logger, "warning",
          [](mrb_state *mrb, mrb_value self) {
            log(::boost::log::trivial::warning, mrb);
            return mrb_nil_value();
          },
          MRB_ARGS_REQ(1));
      mrb_define_class_method(
          m_mrb, logger, "error",
          [](mrb_state *mrb, mrb_value self) {
            log(::boost::log::trivial::error, mrb);
            return mrb_nil_value();
          },
          MRB_ARGS_REQ(1));
      mrb_define_class_method(
          m_mrb, logger, "fatal",
          [](mrb_state *mrb, mrb_value self) {
            log(::boost::log::trivial::fatal, mrb);
            return mrb_nil_value();
          },
          MRB_ARGS_REQ(1));
    }

  protected:
    Agent *m_agent;
    RClass *m_module = nullptr;
    mrb_state *m_mrb = nullptr;
    static std::recursive_mutex m_mutex;
    static RubyVM *m_vm;
  };
}  // namespace mtconnect::ruby
