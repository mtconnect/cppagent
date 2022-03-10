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

#pragma once

#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>
#include <ruby/internal/intern/time.h>

using namespace std;
using namespace mtconnect::pipeline;

namespace mtconnect::ruby {
  static VALUE c_Entity;
  static VALUE c_Observation;
}

namespace Rice::detail {
  using namespace mtconnect::entity;
  using namespace mtconnect;
  
  template<>
  struct Type<QName>
  {
    static bool verify() { return true; }
  };

  template<>
  struct Type<QName&>
  {
    static bool verify() { return true; }
  };

  template<>
  struct To_Ruby<QName>
  {
    VALUE convert(const QName &q)
    {
      return To_Ruby<std::string>().convert(q.str());
    }
  };

  template<>
  struct To_Ruby<QName&>
  {
    VALUE convert(QName &q)
    {
      return To_Ruby<std::string>().convert(q.str());
    }
  };

  template<>
  struct From_Ruby<QName>
  {
    QName convert(VALUE q)
    {
      return From_Ruby<std::string>().convert(q);
    }
  };

  template<>
  struct From_Ruby<QName&>
  {
    QName convert(VALUE q)
    {
      return From_Ruby<std::string>().convert(q);
    }
  };

  template<>
  struct Type<Timestamp>
  {
    static bool verify() { return true; }
  };

  template<>
  struct Type<Timestamp&>
  {
    static bool verify() { return true; }
  };

  template<>
  struct To_Ruby<Timestamp>
  {
    VALUE convert(const Timestamp &ts)
    {
      using namespace std::chrono;
      
      auto secs = time_point_cast<seconds>(ts);
      auto ns = time_point_cast<nanoseconds>(ts) -
                 time_point_cast<nanoseconds>(secs);
      
      return rb_time_nano_new(secs.time_since_epoch().count(), ns.count());
    }
  };

  template<>
  struct To_Ruby<Timestamp&>
  {
    VALUE convert(const Timestamp &ts)
    {
      using namespace std::chrono;
      
      auto secs = time_point_cast<seconds>(ts);
      auto ns = time_point_cast<nanoseconds>(ts) -
                 time_point_cast<nanoseconds>(secs);
      
      return rb_time_nano_new(secs.time_since_epoch().count(), ns.count());
    }
  };
  
  template<>
  struct From_Ruby<Timestamp>
  {
    Timestamp convert(VALUE time)
    {
      using namespace std::chrono;
            
      auto rts = protect(rb_time_timespec, time);
      auto dur = duration_cast<nanoseconds>(seconds{rts.tv_sec}
          + nanoseconds{rts.tv_nsec});
      return  time_point<system_clock>{duration_cast<system_clock::duration>(dur)};
    }
  };

  template<>
  struct From_Ruby<Timestamp&>
  {
    Timestamp convert(VALUE time)
    {
      using namespace std::chrono;
            
      auto rts = protect(rb_time_timespec, time);
      auto dur = duration_cast<nanoseconds>(seconds{rts.tv_sec}
          + nanoseconds{rts.tv_nsec});
      return  time_point<system_clock>{duration_cast<system_clock::duration>(dur)};
    }
  };
}
