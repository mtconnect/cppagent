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

#include <mruby-time/include/mruby/time.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/value.h>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect;
  using namespace device_model;
  using namespace data_item;
  using namespace entity;
  using namespace std;

  inline string stringFromRuby(mrb_state *mrb, mrb_value value)
  {
    if (mrb_string_p(value))
      return string(mrb_str_to_cstr(mrb, value));
    else if (mrb_type(value) == MRB_TT_SYMBOL)
    {
      mrb_sym sym = mrb_symbol(value);
      return string(mrb_sym_name(mrb, sym));
    }
    else
    {
      mrb_value s = mrb_any_to_s(mrb, value);
      return string(mrb_str_to_cstr(mrb, s));
    }
  }

  inline mrb_value toRuby(mrb_state *mrb, const std::string &str)
  {
    return mrb_str_new_cstr(mrb, str.c_str());
  }

  struct mrb_time
  {
    time_t sec;
    time_t usec;
    enum mrb_timezone timezone;
    struct tm datetime;
  };

  inline Timestamp timestampFromRuby(mrb_state *mrb, mrb_value value)
  {
    using namespace std::chrono;
    if (mrb_string_p(value))
    {
      auto text = std::string(mrb_str_to_cstr(mrb, value));
      return parseTimestamp(text);
    }
    else if (mrb_integer_p(value))
    {
      auto v = mrb_fixnum(value);
      auto dur = duration_cast<microseconds>(seconds {v});
      return time_point<system_clock> {duration_cast<system_clock::duration>(dur)};
    }
    auto dp = DATA_TYPE(value);
    if (strncmp(dp->struct_name, "Time", 4) == 0)
    {
      auto tm = static_cast<mrb_time *>(DATA_PTR(value));
      auto dur = duration_cast<microseconds>(seconds {tm->sec} + microseconds {tm->usec});
      return time_point<system_clock> {duration_cast<system_clock::duration>(dur)};
    }
    else
    {
      LOG(warning) << "Don't know how to convert to timestamp: " << dp->struct_name;
      return std::chrono::system_clock::now();
    }
  }

  inline mrb_value toRuby(mrb_state *mrb, const Timestamp &ts)
  {
    using namespace std::chrono;

    auto secs = time_point_cast<seconds>(ts);
    auto us = time_point_cast<microseconds>(ts) - time_point_cast<microseconds>(secs);
    return mrb_time_at(mrb, secs.time_since_epoch().count(), us.count(), MRB_TIMEZONE_UTC);
  }
}  // namespace mtconnect::ruby
