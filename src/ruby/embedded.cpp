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

#include "embedded.hpp"

#include <iostream>
#include <string>

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "device_model/device.hpp"
#include "entity.hpp"
#include "pipeline/guard.hpp"
#include "pipeline/transform.hpp"

#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>

using namespace std;
using namespace mtconnect::pipeline;

namespace Rice::detail {
  using namespace mtconnect::entity;
  using namespace mtconnect;
  
  template<>
  struct Type<Value>
  {
    static bool verify() { return true; }
  };
  
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
        [](const EntityPtr &) -> VALUE { return Qnil; },
        [](const EntityList &) -> VALUE { return Qnil; },
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

namespace mtconnect::ruby {
  using namespace mtconnect::pipeline;

  class RubyTransform : public pipeline::Transform
  {
  public:
    RubyTransform(const std::string &name,
                  Rice::Module &module,
                  const Rice::Identifier function,
                  pipeline::PipelineContextPtr context)
    : Transform(name), m_contract(context->m_contract.get()),
      m_module(module), m_function(function)

    {
      m_guard = TypeGuard<observation::Observation>(RUN);
    }
    
    using calldata = pair<RubyTransform*, observation::ObservationPtr>;
    
    static void *gvlCall(void *data)
    {
      using namespace Rice;
      using namespace observation;
      
      calldata *obs = static_cast<calldata*>(data);
      obs->first->m_module.call(obs->first->m_function, obs->second);
      
      return nullptr;
    }
    
    const entity::EntityPtr operator()(const entity::EntityPtr entity) override
    {
      
      using namespace observation;
      
      ObservationPtr obs = dynamic_pointer_cast<Observation>(entity)->copy();
      calldata data(this, obs);

      rb_thread_call_with_gvl(&gvlCall, &data);
      
      return obs;
    }
    
  protected:
    std::string m_expression;
    PipelineContract *m_contract;
    Rice::Module &m_module;
    Rice::Identifier m_function;
  };
 
  Embedded::Embedded(Agent *agent, const ConfigOptions &options)
  {
    using namespace Rice;
    using namespace Rice::detail;
    using namespace std::literals;
    using namespace date::literals;
    using namespace observation;

    int argc = 0;
    char* argv = nullptr;
    char** pArgv = &argv;

    ruby_sysinit(&argc, &pArgv);
    ruby_init();
    ruby_init_loadpath();
        
    static Module mtc = define_module("MTConnect");
    
    static Class entity = define_class_under<entity::Entity>(mtc, "Entity").
      define_method("value", [](entity::Entity &entity) { return entity.getValue(); }).
      define_method("property", [](entity::Entity &entity, std::string name) {
        return entity.getProperty(name);
      }, Arg("name"));
    
    static Class observation = define_class_under<observation::Observation, entity::Entity>(mtc, "Observation");

    static Class dataItem = define_class_under<device_model::data_item::DataItem, entity::Entity>(mtc, "DataItem");

    mtc.instance_eval("def transform(obs); p obs.value; end");
    mtc.instance_eval("p $:");
    mtc.instance_eval("p constants");
    rb_eval_string("p Object.constants.sort");

    auto dev = agent->getDeviceByName("Mazak");
    auto di = dev->getDeviceDataItem("Ypos");
    Timestamp time = date::sys_days(2021_y / jan / 19_d) + 10h + 1min;

    ErrorList errors;
    auto obs = Observation::make(di, {{"VALUE", 1.23}}, time, errors);

    mtc.call(Identifier("transform"), obs);
    
    for (auto &adp : agent->getSources()) {
      auto pipeline = adp->getPipeline();
      auto trans = make_shared<RubyTransform>("test", mtc, Identifier("transform"),
                                             pipeline->getContext());
      pipeline->spliceAfter("DuplicateFilter", trans);
    }
  }
  
  void *runWithoutGil(void *context)
  {
    boost::asio::io_context *ctx = static_cast<boost::asio::io_context*>(context);
    ctx->run();
    return nullptr;
  }
  
  VALUE boostThread(void *context)
  {
    rb_thread_call_without_gvl(&runWithoutGil, context,
                               NULL, NULL);
    return Qnil;
  }
  
  void Embedded::start(boost::asio::io_context &context, int count)
  {
    std::list<VALUE> threads;
    for (int i = 0; i < count; i++)
    {
      VALUE thread = rb_thread_create(&boostThread, &context);
      threads.push_back(thread);
      rb_thread_run(thread);
    }
    
    for (auto &v : threads)
    {
      rb_funcall(v, rb_intern("join"), 0);
    }
  }

}
