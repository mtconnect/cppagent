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

#include <boost/log/attributes.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <chrono>
#include <string>
#include <thread>

#include "adapter/adapter.hpp"
#include "adapter/shdr/shdr_pipeline.hpp"
#include "parser.hpp"
#include "rest_sink/file_cache.hpp"
#include "service.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace rest_sink {
    class Server;
  }
  class Agent;
  namespace device_model {
    class Device;
  }

#ifdef WITH_PYTHON
  namespace python {
    class Embedded;
  }
#endif

  class XmlPrinter;
  namespace configuration {
    using DevicePtr = std::shared_ptr<device_model::Device>;

    class AgentConfiguration : public MTConnectService
    {
    public:
      using ptree = boost::property_tree::ptree;

      AgentConfiguration();
      virtual ~AgentConfiguration();

      // For MTConnectService
      void stop() override;
      void start() override;
      void initialize(int argc, const char *argv[]) override;

      void configureLogger(const ptree &config);
      void loadConfig(const std::string &file);

      void setAgent(std::unique_ptr<Agent> &agent) { m_agent = std::move(agent); }
      const Agent *getAgent() const { return m_agent.get(); }

      void updateWorkingDirectory() { m_working = std::filesystem::current_path(); }

    protected:
      DevicePtr defaultDevice();
      void loadAdapters(const ptree &tree, const ConfigOptions &options);
      void loadSinks(const ptree &sinks, ConfigOptions &options);

#ifdef WITH_PYTHON
      void configurePython(const ptree &tree, ConfigOptions &options);
#endif

      void loadPlugins(const ptree &tree);
      bool loadPlugin(const std::string &name, const ptree &tree);

      std::optional<std::filesystem::path> checkPath(const std::string &name);

      void boost_set_log_level(const boost::log::trivial::severity_level level);

      void monitorThread();

    protected:
      boost::asio::io_context m_context;
      std::list<std::thread> m_workers;
      std::unique_ptr<Agent> m_agent;
#ifdef WITH_PYTHON
      std::unique_ptr<python::Embedded> m_python;
#endif
      pipeline::PipelineContextPtr m_pipelineContext;
      std::unique_ptr<adapter::Handler> m_adapterHandler;
      boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>>
          m_sink;
      std::string m_version;
      bool m_monitorFiles = false;
      int m_minimumConfigReloadAge = 15;
      std::string m_devicesFile;
      bool m_restart = false;
      std::filesystem::path m_exePath;
      std::filesystem::path m_working;

      int m_workerThreadCount {1};
    };
  }  // namespace configuration
}  // namespace mtconnect
