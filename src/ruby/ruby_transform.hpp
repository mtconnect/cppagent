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

#include "pipeline/guard.hpp"
#include "pipeline/transform.hpp"
#include "pipeline/topic_mapper.hpp"

#include "ruby_entity.hpp"
#include "ruby_observation.hpp"
#include "ruby_pipeline.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect::pipeline;
  using namespace Rice;
  using namespace Rice::detail;
  using namespace std::literals;
  using namespace date::literals;
  using namespace entity;
  using namespace observation;

  class RubyTransform : public pipeline::Transform
  {
  public:
    RubyTransform(Object self, const std::string &name, const Symbol guard)
    : Transform(name), m_self(self), m_method("transform")
    {
      if (guard.value() != Qnil)
        setGuard(guard);
      else
        m_guard = TypeGuard<Entity>(RUN);
    }
    
    void setMethod(const Symbol &sym)
    {
      m_method = sym;
    }
    
    void setGuard(const Symbol &guard)
    {
      auto gv = guard.str();
      if (gv == "Observation")
        m_guard = TypeGuard<Observation>(RUN) || TypeGuard<Entity>(SKIP);
      else if (gv == "Sample")
        m_guard = TypeGuard<Sample>(RUN) || TypeGuard<Entity>(SKIP);
      else if (gv == "Event")
        m_guard = TypeGuard<Event>(RUN) || TypeGuard<Entity>(SKIP);
      else if (gv == "Message")
        m_guard = TypeGuard<PipelineMessage>(RUN) || TypeGuard<Entity>(SKIP);
      else
        m_guard = TypeGuard<Entity>(RUN);
    }
    
    using calldata = pair<RubyTransform*, EntityPtr>;
    
    static void *gvlCall(void *data)
    {
      using namespace Rice;
      using namespace observation;
      
      calldata *cd = static_cast<calldata*>(data);
      RubyTransform &_trans = *(cd->first);
      Object res;
      if (ObservationPtr obs = dynamic_pointer_cast<Observation>(cd->second))
        res = _trans.m_self.call(_trans.m_method, obs);
      else
        res = _trans.m_self.call(_trans.m_method, cd->second);
      cd->second = detail::From_Ruby<EntityPtr>().convert(res.value());
      
      return nullptr;
    }
    
    const entity::EntityPtr operator()(const entity::EntityPtr entity) override
    {
      
      using namespace observation;
      
      calldata data(this, entity);
      rb_thread_call_with_gvl(&gvlCall, &data);
      
      return data.second;
    }
    
    auto &object() { return m_self; }
    void setObject(Object &obj) { m_self = obj; }
    
  protected:
    PipelineContract *m_contract;
    Object           m_self;
    Rice::Symbol m_method;
  };
  
  struct RubyTransformClass {
    void create(Rice::Module &module)
    {
      m_rubyTransform =  define_class_under<RubyTransform, Transform>(module, "RubyTransform");
    }
    
    void methods()
    {
      m_rubyTransform.define_constructor(smart_ptr::Constructor<RubyTransform, Object, const string, const Symbol>(),
                                         Arg("name"), Arg("guard")).
        define_method("method=", &RubyTransform::setMethod, Arg("method"));
    }
    
    Data_Type<RubyTransform> m_rubyTransform;
  };
}

