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

#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>

using namespace std;
using namespace mtconnect::pipeline;

namespace Rice::detail {
  using namespace mtconnect::entity;
  using namespace mtconnect;
  
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
      return To_Ruby<string>().convert(format(ts));
    }
  };
  
  template<>
  struct From_Ruby<Timestamp>
  {
    Timestamp convert(VALUE str)
    {
      Timestamp ts;
      istringstream in(From_Ruby<string>().convert(str));
      in >> std::setw(6) >> date::parse("%FT%T", ts);

      return ts;
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
  struct Type<Value>
  {
    static bool verify() { return true; }
  };
  
  static VALUE convertEntity(EntityPtr e)
  {
    using namespace Rice;
    using namespace Rice::detail;

    Rice::Data_Object<Entity> ent(e.get());
    return ent;
  }
  
  template<>
  struct To_Ruby<Value>
  {
    VALUE convert(const Value &value)
    {
      VALUE rv;
      
      rv = visit(overloaded {
        [](const std::monostate &) -> VALUE { return Qnil; },
        [](const std::nullptr_t &) -> VALUE { return Qnil; },

        // Not handled yet
        [](const EntityPtr &entity) -> VALUE { return convertEntity(entity); },
        [](const EntityList &list) -> VALUE {
          Rice::Array ary;
          for (const auto &ent : list)
          {
            ary.push(ent.get());
          }
          
          return ary;
        },
        [](const observation::DataSet &v) -> VALUE { return Qnil; },
                
        // Handled types
        [](const Vector &v) -> VALUE {
          Rice::Array array(v.begin(), v.end());
          return array;
        },
        [](const Timestamp &v) -> VALUE {
          Value out = v;
          ConvertValueToType(out, STRING);
          return To_Ruby<string>().convert(get<string>(out));
        },
        [](const string &arg) -> VALUE { return To_Ruby<string>().convert(arg); },
        [](const bool arg) -> VALUE { return To_Ruby<bool>().convert(arg); },
        [](const double arg) -> VALUE { return To_Ruby<double>().convert(arg); },
        [](const int64_t arg) -> VALUE { return To_Ruby<int64_t>().convert(arg); }
      }, value);
      
      return rv;
    }
  };
  
  template<>
  struct From_Ruby<Value>
  {
    Value convert(VALUE value)
    {
      Value res;
      
      switch (rb_type(value))
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
          
        default:
          res.emplace<std::monostate>();
          break;
      }
            
      return res;
    }
  };
}
