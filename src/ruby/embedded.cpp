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
#include <date/date.h>

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "device_model/device.hpp"
#include "entity.hpp"
#include "pipeline/guard.hpp"
#include "pipeline/transform.hpp"
#include "logging.hpp"

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

namespace mtconnect::ruby {
  using namespace mtconnect::pipeline;
  using namespace Rice;
  using namespace Rice::detail;
  using namespace std::literals;
  using namespace date::literals;
  using namespace observation;

  class RubyTransform : public pipeline::Transform
  {
  public:
    RubyTransform(const std::string &name,
                  Rice::Module &module,
                  const std::string &function,
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
  
  void Embedded::createModule()
  {
    m_module = make_unique<Module>(define_module("MTConnect"));
  }
  
  void Embedded::createPipeline()
  {
    define_class_under<pipeline::Transform>(*m_module, "Transform");
    define_class_under<pipeline::PipelineContext>(*m_module, "PipelineContext");

    define_class_under<pipeline::Pipeline>(*m_module, "Pipeline").
      define_method("splice_before", [](pipeline::Pipeline *p, const std::string name, pipeline::TransformPtr trans) {
          p->spliceBefore(name, trans);
        }, Arg("name"), Arg("transform")).
      define_method("splice_after", [](pipeline::Pipeline *p, const std::string name, pipeline::TransformPtr trans) {
          p->spliceAfter(name, trans);
        }, Arg("name"), Arg("transform")).
      define_method("first_after", [](pipeline::Pipeline *p, const std::string name, pipeline::TransformPtr trans) {
          p->firstAfter(name, trans);
        }, Arg("name"), Arg("transform")).
      define_method("last_after", [](pipeline::Pipeline *p, const std::string name, pipeline::TransformPtr trans) {
          p->lastAfter(name, trans);
        }, Arg("name"), Arg("transform")).
      define_method("run", [](pipeline::Pipeline *p, entity::EntityPtr entity) { p->run(entity); }, Arg("entity")).
      define_method("context", [](pipeline::Pipeline *p) { return p->getContext(); });
  }
  
  void Embedded::createComponent()
  {
    using namespace device_model;

    define_class_under<Component, Entity>(*m_module, "Component").
      define_method("component_name", [](Component *c) { return c->getComponentName(); }).
      define_method("uuid", [](Component *c) { return c->getUuid(); }).
      define_method("id", [](Component *c) { return c->getId(); }).
      define_method("children", [](Component *c) {
        Rice::Array ary;
        const auto &list = c->getChildren();
        if (list)
        {
          for (auto const &s : *list)
          {
            auto cp = dynamic_cast<Component*>(s.get());
            ary.push(cp);
          }
        }
        return ary;
      }).
    define_method("data_items", [](Component *c) {
        Rice::Array ary;
        const auto &list = c->getDataItems();
        if (list)
        {
          for (auto const &s : *list)
          {
            auto di = dynamic_cast<data_item::DataItem*>(s.get());
            ary.push(di);
          }
        }
        return ary;
      });

    define_class_under<device_model::Device, device_model::Component>(*m_module, "Device");
  }
  
  void Embedded::createDataItem()
  {
    using namespace device_model;

    define_class_under<data_item::DataItem, entity::Entity>(*m_module, "DataItem").
      define_method("name", [](data_item::DataItem *di) { return di->getName(); }).
      define_method("observation_name", [](data_item::DataItem *di) { return di->getObservationName().str(); }).
      define_method("id", [](data_item::DataItem *di) { return di->getId(); }).
      define_method("type", [](data_item::DataItem *di) { return di->getType(); }).
      define_method("sub_type", [](data_item::DataItem *di) { return di->getSubType(); });
  }
   
  Embedded::Embedded(Agent *agent, const ConfigOptions &options)
    : m_agent(agent), m_options(options)
  {
    NAMED_SCOPE("Ruby::Embedded::Embedded");

    using namespace device_model;
    
    createModule();
    
    m_ragent = make_unique<Class>(define_class_under<Agent>(*m_module, "Agent"));

    define_class_under<entity::Entity>(*m_module, "Entity").
      define_method("value", [](entity::EntityPtr entity) { return entity->getValue(); }).
      define_method("property", [](entity::EntityPtr entity, std::string name) {
        return entity->getProperty(name);
      }, Arg("name"));

    createPipeline();

    define_class_under<Source>(*m_module, "Source").
      define_method("name", [](Source *s) -> string {
        return s->getName();
      }).
      define_method("pipeline", [](Source *s) { return s->getPipeline(); });
    
    define_class_under<Sink>(*m_module, "Sink");
    
    m_module->define_singleton_method("agent", [this](Object self) {
      return m_agent;
    });
    
    define_class_under<observation::Observation, entity::Entity>(*m_module, "Observation").
      define_singleton_method("make", [](const DataItemPtr dataItem,
                                         const Properties &incompingProps,
                                         const Timestamp *timestamp) {
        ErrorList errors;
        Timestamp ts;
        if (timestamp == nullptr)
          ts = std::chrono::system_clock::now();
        else
          ts = *timestamp;
        auto obs = Observation::make(dataItem, incompingProps, ts, errors);
        return obs;
      }, Arg("dataItem"), Arg("properties"), Arg("timestamp") = nullptr);

    createDataItem();
    createComponent();
    
    m_ragent->define_method("sources", [](Agent *agent) {
        Rice::Array ary;
        for (auto &s : agent->getSources())
          ary.push(s.get());
        return ary;
      }).
      define_method("data_item_for_device", [](Agent *agent, const string device, const string name) {
        return agent->getDataItemForDevice(device, name);
      }, Arg("device"), Arg("name")).
      define_method("device", [](Agent *agent, const string device) {
        return agent->findDeviceByUUIDorName(device);
      }, Arg("name")).
      define_method("devices", [](Agent *agent) {
        Rice::Array ary;
        for (auto &d : agent->getDevices())
          ary.push(d.get());
        return ary;
      });

    auto module = GetOption<string>(m_options, "module");
    auto initialization = GetOption<string>(m_options, "initialization");
    
    int state;
    if (module)
    {
      LOG(info) << "Ruby – Loading file: " << *module;

      Rice::String file(*module);
      rb_load_protect(file, false, &state);
    }
    if (initialization)
    {
      rb_eval_string_protect(initialization->c_str(), &state);
    }
  }
  
  Embedded::~Embedded()
  {
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
