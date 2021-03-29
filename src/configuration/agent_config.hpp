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

#include <boost/asio.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <chrono>
#include <string>

#include <dlib/logger.h>

#include "adapter/adapter.hpp"
#include "adapter/adapter_pipeline.hpp"
#include "http_server/file_cache.hpp"
#include "parser.hpp"
#include "service.hpp"
#include "utilities.hpp"

namespace mtconnect
{
  namespace http_server
  {
    class Server;
  }
  class Agent;
  namespace device_model
  {
    class Device;
  }

  class XmlPrinter;
  namespace configuration
  {
    using DevicePtr = std::shared_ptr<device_model::Device>;

    using ConfigReader = dlib::config_reader::kernel_1a;

    using NamespaceFunction = void (XmlPrinter::*)(const std::string &, const std::string &,
                                                   const std::string &);
    using StyleFunction = void (XmlPrinter::*)(const std::string &);

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
      void loadAllowPut(http_server::Server *server, ConfigOptions &options);
      void loadNamespace(const ptree &tree, const char *namespaceType,
                         http_server::FileCache *cache, XmlPrinter *printer,
                         NamespaceFunction callback);
      void loadFiles(XmlPrinter *xmlPrinter, const ptree &tree, http_server::FileCache *cache);
      void loadStyle(const ptree &tree, const char *styleName, http_server::FileCache *cache,
                     XmlPrinter *printer, StyleFunction styleFunction);
      void loadTypes(const ptree &tree, http_server::FileCache *cache);
      void loadHttpHeaders(const ptree &tree, ConfigOptions &options);

      std::optional<std::filesystem::path> checkPath(const std::string &name);

      void boost_set_log_level(const boost::log::trivial::severity_level level);

      void monitorThread();

    protected:
      std::unique_ptr<Agent> m_agent;
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
      
      boost::asio::io_context m_context;
      std::list<std::thread> m_workers;
      int m_workerThreadCount {1};
    };
  }  // namespace configuration
}  // namespace mtconnect
