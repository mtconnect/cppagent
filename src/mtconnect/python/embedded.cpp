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

#include <boost/python.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/str.hpp>
#include <boost/python/tuple.hpp>

#include <iostream>
#include <string>

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "device_model/device.hpp"
#include "entity.hpp"
#include "pipeline/guard.hpp"
#include "pipeline/transform.hpp"

using namespace std;

namespace mtconnect {
  namespace python {
    using namespace boost::python;
    namespace py = boost::python;
    using namespace entity;

#define None object(detail::borrowed_reference(Py_None))

    struct Context
    {
      object m_source;
      object m_device;
      object m_dataItem;
      object m_entity;
      object m_transform;
      object m_pipeline;
    };
    using ContextPtr = Context *;

    struct Wrapper
    {
      virtual ~Wrapper() {}
      ContextPtr m_context;
    };

    static object wrap(EntityPtr entity, ContextPtr context);

    struct EntityWrapper : Wrapper
    {
      EntityWrapper() {}

      string name() { return m_entity->getName(); }

      object property(string name)
      {
        auto prop = m_entity->getProperty(name);
        object res = None;
        visit(overloaded {[&res](const std::string &s) {
                            if (!s.empty())
                              res = object(s);
                          },
                          [&res](const int64_t v) { res = object(v); },
                          [&res](const bool v) { res = object(v); },
                          [&res](const double v) { res = object(v); },
                          [&res](const Timestamp &v) { res = object(v); },
                          [&res](const Vector &v) {
                            py::list l;
                            for (auto &i : v)
                              l.append(i);
                            res = l;
                          },
                          [&res, this](EntityPtr ent) { res = wrap(ent, m_context); },
                          [&res, this](EntityList &entities) {
                            py::list l;
                            for (auto &i : entities)
                              l.append(wrap(i, m_context));
                            res = l;
                          },

                          [](const auto &) {}},
              prop);

        return res;
      }

      object value() { return property("VALUE"); }

      object get_list(string name)
      {
        auto entities = m_entity->getList(name);
        if (!entities)
          return None;
        else
        {
          py::list l;
          for (auto &i : *entities)
            l.append(wrap(i, m_context));
          return object(l);
        }
      }

      EntityPtr m_entity;
    };

    static object wrap(EntityPtr entity, ContextPtr context)
    {
      auto e = context->m_entity();
      EntityWrapper &wrap = extract<EntityWrapper &>(e);
      wrap.m_entity = entity;
      wrap.m_context = context;

      return e;
    }

    BOOST_PYTHON_MODULE(entity)
    {
      class_<EntityWrapper>("Entity", init<>())
          .def("name", &EntityWrapper::name)
          .def("property", &EntityWrapper::property)
          .def("value", &EntityWrapper::value)
          .def("get_list", &EntityWrapper::get_list);
    }

    using TransformFun = function<const entity::EntityPtr(const entity::EntityPtr)>;

    class PythonTransform : public pipeline::Transform
    {
    public:
      using pipeline::Transform::Transform;
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override
      {
        if (m_function)
          return m_function(entity);
        else
          return entity;
      }

      string name() { return m_name; }
      TransformFun m_function;
    };

    using PythonTransformPtr = shared_ptr<PythonTransform>;

    struct TransformWrapper : Wrapper, py::wrapper<TransformWrapper>
    {
      TransformWrapper() {}
      TransformWrapper(std::string name)
      {
        m_transform = shared_ptr<PythonTransform>(new PythonTransform(name));
        m_transform->m_function = [this](const EntityPtr entity) {
          object ent = wrap(entity, m_context);
          auto res = run(ent);
          EntityWrapper &wrap = extract<EntityWrapper &>(res);
          return wrap.m_entity;
        };
        m_transform->setGuard([this](const EntityPtr entity) {
          object obj = wrap(entity, m_context);
          return guard(obj);
        });
      }

      object next(object entity)
      {
        EntityWrapper &e = extract<EntityWrapper &>(entity);
        auto res = m_transform->next(e.m_entity);
        return wrap(res, m_context);
      }

      virtual object run(object entity)
      {
        if (override f = get_override("run"))
        {
          return f(entity);
        }
        else
        {
          EntityWrapper &e = extract<EntityWrapper &>(entity);
          auto res = (*m_transform)(e.m_entity);
          return wrap(res, m_context);
        }
      }

      virtual pipeline::GuardAction guard(object entity)
      {
        if (override f = get_override("guard"))
        {
          pipeline::GuardAction action = f(entity);
          return action;
        }
        else
        {
          EntityWrapper &e = extract<EntityWrapper &>(entity);
          auto res = m_transform->check(e.m_entity);
          return res;
        }
      }

      string name() { return m_transform->name(); }

      PythonTransformPtr m_transform;
    };

    struct PipelineWrapper : Wrapper
    {
      PipelineWrapper() : m_pipeline(nullptr) {}

      bool splice_before(std::string target, object transform)
      {
        TransformWrapper &xform = extract<TransformWrapper &>(transform);
        return m_pipeline->spliceBefore(target, xform.m_transform);
      }

      bool splice_after(std::string target, object transform)
      {
        TransformWrapper &xform = extract<TransformWrapper &>(transform);
        return m_pipeline->spliceAfter(target, xform.m_transform);
      }

      pipeline::Pipeline *m_pipeline;
    };

    BOOST_PYTHON_MODULE(pipeline)
    {
      class_<PipelineWrapper>("Pipeline", init<>())
          .def("splice_before", &PipelineWrapper::splice_before)
          .def("splice_after", &PipelineWrapper::splice_after);

      class_<TransformWrapper>("Transform", init<>())
          .def(init<std::string>())
          .def("run", &TransformWrapper::run)
          .def("guard", &TransformWrapper::guard)
          .def("next", &TransformWrapper::next);

      enum_<pipeline::GuardAction>("GuardAction")
          .value("run", pipeline::GuardAction::RUN)
          .value("continue", pipeline::GuardAction::CONTINUE)
          .value("skip", pipeline::GuardAction::SKIP);
    }

    struct SourceWrapper : Wrapper
    {
      SourceWrapper() {}

      object get_name() { return object(m_source->getName()); }
      object get_pipeline()
      {
        auto pipe = m_context->m_pipeline();
        PipelineWrapper &wrap = extract<PipelineWrapper &>(pipe);
        wrap.m_pipeline = m_source->getPipeline();
        wrap.m_context = m_context;

        return pipe;
      }

      SourcePtr m_source;
    };

    BOOST_PYTHON_MODULE(source)
    {
      class_<SourceWrapper>("Source", init<>())
          .def("get_name", &SourceWrapper::get_name)
          .def("get_pipeline", &SourceWrapper::get_pipeline);
    }

    struct AgentWrapper : Wrapper
    {
      AgentWrapper() {}
      object get_device(const std::string name)
      {
        if (m_agent == nullptr)
          return None;
        else
        {
          auto dev = m_agent->getDeviceByName(name);
          if (dev)
          {
            return wrap(dev, m_context);
          }
          else
          {
            return None;
          }
        }
      }

      py::list get_sources()
      {
        auto sources = py::list();
        if (m_agent == nullptr)
          return sources;

        for (auto source : m_agent->getSources())
        {
          auto src = m_context->m_source();
          SourceWrapper &wrap = extract<SourceWrapper &>(src);
          wrap.m_source = source;
          wrap.m_context = m_context;

          sources.append(src);
        }

        return sources;
      }

      object get_source(const std::string name)
      {
        if (m_agent != nullptr)
        {
          for (auto source : m_agent->getSources())
          {
            if (source->getName() == name)
            {
              auto src = m_context->m_source();
              SourceWrapper &wrap = extract<SourceWrapper &>(src);
              wrap.m_source = source;
              wrap.m_context = m_context;

              return src;
            }
          }
        }

        return None;
      }

#if 0
    object getDataItem(const std::string device, const std::string name);
    object getDataItem(boost::python::object device, const std::string name);
    object getDataItem(const std::string id);
#endif

      Agent *m_agent {nullptr};
    };

    Embedded::Embedded(Agent *agent, const ConfigOptions &options)
      : m_agent(agent), m_context(new Context()), m_options(options)
    {
      try
      {
        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        PyConfig_Read(&config);
        config.dev_mode = true;
        PyWideStringList_Append(&config.module_search_paths,
                                L"/Users/will/projects/MTConnect/agent/cppagent/modules");
        Py_InitializeFromConfig(&config);
        PyConfig_Clear(&config);

        wstring path(Py_GetPath());
        string s(path.begin(), path.end());
        cout << "Path: " << s << endl;

        object main_module = import("__main__");
        object main_namespace = main_module.attr("__dict__");
        auto pipe = object(boost::python::handle<>(PyInit_pipeline()));
        main_namespace["pipeline"] = pipe;
        auto entity = object(boost::python::handle<>(PyInit_entity()));
        main_namespace["entity"] = entity;
        auto source = object(boost::python::handle<>(PyInit_source()));
        main_namespace["source"] = source;

        m_context->m_source = source.attr("__dict__")["Source"];
        m_context->m_pipeline = pipe.attr("__dict__")["Pipeline"];
        m_context->m_transform = pipe.attr("__dict__")["Transform"];
        m_context->m_entity = entity.attr("__dict__")["Entity"];

        auto agentClass = class_<AgentWrapper>("Agent", init<>())
                              .def("get_device", &AgentWrapper::get_device)
                              .def("get_source", &AgentWrapper::get_source)
                              .def("get_sources", &AgentWrapper::get_sources);
        main_namespace["Agent"] = agentClass;
        auto pyagent = agentClass();
        AgentWrapper &wrapper = extract<AgentWrapper &>(pyagent);
        wrapper.m_agent = agent;
        wrapper.m_context = m_context;

        main_namespace["agent"] = pyagent;

        // Py_RunMain();
      }

      catch (error_already_set const &)
      {
        PyErr_Print();
      }
    }

    Embedded::~Embedded() { delete m_context; }
  }  // namespace python
}  // namespace mtconnect
