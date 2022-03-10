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
  struct Type<EntityList>
  {
    static bool verify() { return true; }
  };
  
  template<>
  struct To_Ruby<EntityList>
  {
    VALUE convert(const EntityList &list)
    {
      Rice::Array ary;
      for (const auto &ent : list)
      {
        ary.push(ent.get());
      }
      
      return ary;
    }
  };

  template<>
  struct From_Ruby<EntityList>
  {
    EntityList convert(VALUE list)
    {
      EntityList ary;
      Rice::Array vary(list);
      for (const auto &ent : vary)
      {
        if (Rice::protect(rb_obj_is_kind_of, ent.value(), mtconnect::ruby::c_Entity))
        {
          Data_Object<Entity> entity(ent.value());
          ary.emplace_back(entity->getptr());
        }
      }
      
      return ary;
    }
  };
  
  template<>
  struct Type<observation::DataSetValue>
  {
    static bool verify() { return true; }
  };
  
  template<>
  struct To_Ruby<observation::DataSetValue>
  {
    VALUE convert(const observation::DataSetValue &value)
    {
      VALUE rv;
      
      rv = visit(overloaded {
        [](const std::monostate &v) -> VALUE { return Qnil; },
        [](const std::string &v) -> VALUE { return To_Ruby<string>().convert(v); },
        [](const observation::DataSet &v) -> VALUE {
          return Qnil;
        },
        [](const int64_t v) -> VALUE { return To_Ruby<int64_t>().convert(v); },
        [](const double v) -> VALUE { return To_Ruby<double>().convert(v); },

      }, value);
      
      return rv;
    }
  };
  
  template<>
  struct From_Ruby<observation::DataSetValue>
  {
    observation::DataSetValue convert(VALUE value)
    {
      observation::DataSetValue res;
      
      switch (TYPE(value))
      {
        case RUBY_T_STRING:
          res.emplace<string>(From_Ruby<string>().convert(value));
          break;
          
        case RUBY_T_BIGNUM:
        case RUBY_T_FIXNUM:
          res.emplace<int64_t>(From_Ruby<int64_t>().convert(value));
          break;

        case RUBY_T_FLOAT:
          res.emplace<double>(From_Ruby<double>().convert(value));
          break;
          
        case RUBY_T_HASH:
          break;
          
        default:
          break;
      }
      return res;
    }
  };
  
  template<>
  struct Type<Entity>
  {
    static bool verify() { return true; }
  };
  
  template<>
  struct To_Ruby<Entity>
  {
    VALUE convert(const EntityPtr &e)
    {
      Data_Object<Entity> entity(e.get());
      return entity.value();
    }
  };
  
  template<>
  struct From_Ruby<Entity>
  {
    EntityPtr convert(VALUE e)
    {
      Data_Object<Entity> entity(e);
      return entity->getptr();
    }
  };

  template<>
  struct Type<Value>
  {
    static bool verify() { return true; }
  };

  template<>
  struct Type<Value&>
  {
    static bool verify() { return true; }
  };
  
  inline VALUE ConvertValueToRuby(const Value &value)
  {
    VALUE rv;
    
    rv = visit(overloaded {
      [](const std::monostate &) -> VALUE { return Qnil; },
      [](const std::nullptr_t &) -> VALUE { return Qnil; },

      // Not handled yet
      [](const EntityPtr &entity) -> VALUE {
        return To_Ruby<Entity>().convert(entity);
      },
      [](const EntityList &list) -> VALUE {
        Rice::Array ary;
        for (const auto &ent : list)
        {
          ary.push(ent.get());
        }
        
        return ary;
      },
      [](const observation::DataSet &v) -> VALUE {
        return Qnil;
      },
              
      // Handled types
      [](const Vector &v) -> VALUE {
        Rice::Array array(v.begin(), v.end());
        return array;
      },
      [](const Timestamp &v) -> VALUE {
        return To_Ruby<Timestamp>().convert(v);
      },
      [](const string &arg) -> VALUE { return To_Ruby<string>().convert(arg); },
      [](const bool arg) -> VALUE { return To_Ruby<bool>().convert(arg); },
      [](const double arg) -> VALUE { return To_Ruby<double>().convert(arg); },
      [](const int64_t arg) -> VALUE { return To_Ruby<int64_t>().convert(arg); }
    }, value);
    
    return rv;
  }

  template<>
  struct To_Ruby<Value>
  {
    VALUE convert(Value &value)
    {
      return ConvertValueToRuby(value);
    }

    VALUE convert(const Value &value)
    {
      return ConvertValueToRuby(value);
    }
  };

  template<>
  struct To_Ruby<Value&>
  {
    VALUE convert(Value &value)
    {
      return ConvertValueToRuby(value);
    }

    VALUE convert(const Value &value)
    {
      return ConvertValueToRuby(value);
    }
  };

  inline Value ConvertRubyToValue(VALUE value)
  {
    using namespace mtconnect::ruby;
    using namespace mtconnect::observation;

    Value res;
    
    switch (TYPE(value))
    {
      case RUBY_T_NIL:
      case RUBY_T_UNDEF:
        res.emplace<std::nullptr_t>();
        break;
        
      case RUBY_T_STRING:
        res.emplace<string>(From_Ruby<string>().convert(value));
        break;
        
      case RUBY_T_BIGNUM:
      case RUBY_T_FIXNUM:
        res.emplace<int64_t>(From_Ruby<int64_t>().convert(value));
        break;

      case RUBY_T_FLOAT:
        res.emplace<double>(From_Ruby<double>().convert(value));
        break;
        
      case RUBY_T_TRUE:
        res.emplace<bool>(true);
        break;

      case RUBY_T_FALSE:
        res.emplace<bool>(false);
        break;
        
      case RUBY_T_HASH:
        break;
        
      case RUBY_T_ARRAY:
      {
        res.emplace<Vector>();
        Vector &out = get<Vector>(res);
        const Rice::Array ary(value);
        for (const auto &v : ary) {
          auto t = rb_type(v);
          if (t == RUBY_T_FIXNUM || t == RUBY_T_BIGNUM || t == RUBY_T_FLOAT)
          {
            out.emplace_back(From_Ruby<double>().convert(v));
          }
        }
        break;
      }
        
      case RUBY_T_OBJECT:
        if (protect(rb_obj_is_kind_of, value, rb_cTime))
        {
          res = From_Ruby<Timestamp>().convert(value);
        }
        else if (protect(rb_obj_is_kind_of, value, c_Entity))
        {
          res = From_Ruby<Entity>().convert(value);
        }
        {
          res.emplace<std::monostate>();
        }
        break;
        
      default:
        res.emplace<std::monostate>();
        break;
    }
          
    return res;
  }

  template<>
  struct From_Ruby<Value>
  {
    Value convert(VALUE value)
    {
      return ConvertRubyToValue(value);
    }
  };

  template<>
  struct From_Ruby<Value&>
  {
    Value convert(VALUE value)
    {
      return ConvertRubyToValue(value);
    }
  };
}
