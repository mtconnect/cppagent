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

#include "adapter/adapter.hpp"
#include "adapter/adapter_pipeline.hpp"
#include "http_server/file_cache.hpp"
#include "service.hpp"
#include "utilities.hpp"

#include <dlib/logger.h>

#include <chrono>
#include <string>

namespace mtconnect
{
  namespace http_server
  {
    class Server;
  }
  class Agent;
  class Device;
  class RollingFileLogger;
  class XmlPrinter;
  
  using ConfigReader = dlib::config_reader::kernel_1a;

  using NamespaceFunction = void (XmlPrinter::*)(const std::string &, const std::string &,
                                                 const std::string &);
  using StyleFunction = void (XmlPrinter::*)(const std::string &);

  class AgentConfiguration : public MTConnectService
  {
  public:
    AgentConfiguration();
    virtual ~AgentConfiguration();

    // For MTConnectService
    void stop() override;
    void start() override;
    void initialize(int argc, const char *argv[]) override;

    void configureLogger(ConfigReader &reader);
    void loadConfig(std::istream &file);

    void setAgent(std::unique_ptr<Agent> &agent) { m_agent = std::move(agent); }
    const Agent *getAgent() const { return m_agent.get(); }

    const RollingFileLogger *getLogger() const { return m_loggerFile.get(); }

    void updateWorkingDirectory() { m_working = std::filesystem::current_path(); }

  protected:
    Device *defaultDevice();
    void loadAdapters(ConfigReader &reader, const ConfigOptions &options);
    void loadAllowPut(ConfigReader &reader, http_server::Server *server);
    void loadNamespace(ConfigReader &reader, const char *namespaceType,
                       http_server::FileCache *cache, XmlPrinter *printer,
                       NamespaceFunction callback);
    void loadFiles(XmlPrinter *xmlPrinter, ConfigReader &reader, http_server::FileCache *cache);
    void loadStyle(ConfigReader &reader, const char *styleName, http_server::FileCache *cache,
                   XmlPrinter *printer, StyleFunction styleFunction);
    void loadTypes(ConfigReader &reader, http_server::FileCache *cache);
    void loadHttpHeaders(ConfigReader &reader, ConfigOptions &options);

    void LoggerHook(const std::string &loggerName, const dlib::log_level &l,
                    const dlib::uint64 threadId, const char *message);

    std::optional<std::filesystem::path> checkPath(const std::string &name);

    void monitorThread();

  protected:
    std::unique_ptr<Agent> m_agent;
    pipeline::PipelineContextPtr m_pipelineContext;
    std::unique_ptr<adapter::Handler> m_adapterHandler;
    std::unique_ptr<RollingFileLogger> m_loggerFile;
    std::string m_version;
    bool m_monitorFiles = false;
    int m_minimumConfigReloadAge = 15;
    std::string m_devicesFile;
    bool m_restart = false;
    std::filesystem::path m_exePath;
    std::filesystem::path m_working;
    bool m_pretty = false;
  };
}  // namespace mtconnect
