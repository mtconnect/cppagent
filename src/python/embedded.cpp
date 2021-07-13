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

#include <boost/python/dict.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/str.hpp>
#include <boost/python.hpp>
#include <iostream>
#include <string>

#include "embedded.hpp"
#include "agent.hpp"
#include "pipeline/transform.hpp"
#include "device_model/device.hpp"
#include "entity.hpp"

using namespace std;

namespace mtconnect {
namespace python {
  using namespace boost::python;
  namespace py = boost::python;
  using namespace entity;
  
  class PythonTransform : public pipeline::Transform
  {
  public:
    using pipeline::Transform::Transform;
    const entity::EntityPtr operator()(const entity::EntityPtr entity) override
    {
      return entity;
    }
    
    string name() { return m_name; }
  };
  
  BOOST_PYTHON_MODULE(pipeline)
  {
    class_<PythonTransform, shared_ptr<PythonTransform>>("Transform", init<std::string>())
      .def("name", &PythonTransform::name);
  }
  
  BOOST_PYTHON_MODULE(agent)
  {
    def("get_device", &Embedded::getDevice);
  }

  BOOST_PYTHON_MODULE(entity)
  {
    class_<Entity, shared_ptr<Entity>>("Entity", no_init);
  }

  
  Embedded::Embedded(Agent *agent)
    : m_agent(agent)
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

      auto pipe = object(boost::python::handle<>(PyInit_pipeline()));
      
      object main_module = import("__main__");
      object main_namespace = main_module.attr("__dict__");
      main_namespace["pipeline"] = pipe;
      dict d = extract<dict>(main_namespace); //.attr("__builtins__").attr("__dict__"));
      auto items = d.attr("items")(); // just plain d.items or d.iteritems for Python 2!
      for (auto it = stl_input_iterator<py::tuple>(items); it != stl_input_iterator<py::tuple>(); ++it) {
        py::tuple kv = *it;
        auto key = kv[0];
        auto value = kv[1];
        std::cout << extract<const char*>(str(key)) << " : " << extract<const char*>(str(value)) << std::endl;
      }
      
      object ignored = exec("result = 5 ** 2", main_namespace);
      int five_squared = extract<int>(main_namespace["result"]);
      
      cout << "Five Squared: " << five_squared << endl;;
      
      //Py_RunMain();
    }
    
    catch(error_already_set const &)
    {
      PyErr_Print();
    }
  }
  
  Embedded::~Embedded()
  {
  }

  boost::python::object Embedded::getDevice(const std::string name)
  {
    return object(detail::borrowed_reference(Py_None));
  }
  
  boost::python::object Embedded::getDataItem(const std::string device, const std::string name)
  {
    return object(detail::borrowed_reference(Py_None));
  }
  boost::python::object Embedded::getDataItem(boost::python::object device, const std::string name)
  {
    return object(detail::borrowed_reference(Py_None));
  }

  boost::python::object Embedded::getDataItem(const std::string id)
  {
    return object(detail::borrowed_reference(Py_None));
  }
  
  boost::python::object Embedded::getAdapter(const std::string url)
  {
    return object(detail::borrowed_reference(Py_None));
  }
}
}
