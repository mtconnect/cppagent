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
      ContextPtr m_context;
    };

    class PythonTransform : public pipeline::Transform
    {
    public:
      using pipeline::Transform::Transform;
      const entity::EntityPtr operator()(const entity::EntityPtr entity) override { return entity; }

      string name() { return m_name; }
    };

    struct PipelineWrapper : Wrapper
    {
      PipelineWrapper() : m_pipeline(nullptr) {}

      pipeline::Pipeline *m_pipeline;
    };

    BOOST_PYTHON_MODULE(pipeline) { class_<PipelineWrapper>("Pipeline", init<>()); }

    struct EntityWrapper : Wrapper
    {
      EntityWrapper() {}

      EntityPtr m_entity;
    };

    BOOST_PYTHON_MODULE(entity) { class_<EntityWrapper>("Entity", init<>()); }

    struct SourceWrapper : Wrapper
    {
      SourceWrapper() {}

      object getName() { return object(m_source->getName()); }
      object getPipeline()
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
          .def("get_name", &SourceWrapper::getName)
          .def("get_pipeline", &SourceWrapper::getPipeline);
    }

    struct AgentWrapper : Wrapper
    {
      AgentWrapper() {}
      object getDevice(const std::string name)
      {
        if (m_agent == nullptr)
          return None;
        else
        {
          auto dev = m_agent->getDeviceByName(name);
          if (dev)
          {
            auto uuid = dev->getUuid();
            return object(*uuid);
          }
          else
          {
            return None;
          }
        }
      }

      py::list getSources()
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

      object getSource(const std::string name)
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

    Embedded::Embedded(Agent *agent, const ConfigOptions &options) : m_agent(agent), m_context(new Context()), m_options(options)
    {
      try
      {
        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        PyConfig_Read(&config);
        config.dev_mode = true;
        PyWideStringList_Append(&config.module_search_paths,
                                L"/Users/will/projects/MTConnect/agent/cppagent_dev/modules");
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

        auto agentClass = class_<AgentWrapper>("Agent", init<>())
                              .def("get_device", &AgentWrapper::getDevice)
                              .def("get_source", &AgentWrapper::getSource)
                              .def("get_sources", &AgentWrapper::getSources);
        main_namespace["Agent"] = agentClass;
        auto pyagent = agentClass();
        AgentWrapper &wrapper = extract<AgentWrapper &>(pyagent);
        wrapper.m_agent = agent;
        wrapper.m_context = m_context;

        main_namespace["agent"] = pyagent;

        //Py_RunMain();
      }

      catch (error_already_set const &)
      {
        PyErr_Print();
      }
    }

    Embedded::~Embedded() { delete m_context; }
  }  // namespace python
}  // namespace mtconnect
