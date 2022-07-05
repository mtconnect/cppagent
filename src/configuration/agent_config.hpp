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

#pragma once

#include <boost/log/attributes.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <chrono>
#include <string>
#include <thread>

#include "agent.hpp"
#include "async_context.hpp"
#include "parser.hpp"
#include "service.hpp"
#include "sink/rest_sink/file_cache.hpp"
#include "sink/sink.hpp"
#include "source/adapter/adapter.hpp"
#include "source/adapter/shdr/shdr_pipeline.hpp"
#include "source/source.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace rest_sink {
    class Server;
  }
  namespace device_model {
    class Device;
  }

#ifdef WITH_PYTHON
  namespace python {
    class Embedded;
  }
#endif
#ifdef WITH_RUBY
  namespace ruby {
    class Embedded;
  }
#endif

  class XmlPrinter;
  namespace configuration {
    using DevicePtr = std::shared_ptr<device_model::Device>;

    class AgentConfiguration : public MTConnectService
    {
    public:
      using InitializationFn = void(const boost::property_tree::ptree &, AgentConfiguration &);
      using InitializationFunction = boost::function<InitializationFn>;

      using ptree = boost::property_tree::ptree;

      AgentConfiguration();
      virtual ~AgentConfiguration();

      // For MTConnectService
      void stop() override;
      void start() override;
      void initialize(const boost::program_options::variables_map &options) override;

      void configureLogger(const ptree &config);
      void loadConfig(const std::string &file);

      void setAgent(std::unique_ptr<Agent> &agent) { m_agent = std::move(agent); }
      const Agent *getAgent() const { return m_agent.get(); }
      auto &getContext() { return m_context->getContext(); }
      auto &getAsyncContext() { return *m_context.get(); }

      void updateWorkingDirectory() { m_working = std::filesystem::current_path(); }

      auto &getSinkFactory() { return m_sinkFactory; }
      auto &getSourceFactory() { return m_sourceFactory; }
      auto getPipelineContext() { return m_pipelineContext; }

      const auto &getLoggerSink() const { return m_sink; }
      const auto &getLogDirectory() const { return m_logDirectory; }
      const auto &getLogFileName() const { return m_logFileName; }
      const auto &getLogArchivePattern() const { return m_logArchivePattern; }
      auto getMaxLogFileSize() const { return m_maxLogFileSize; }
      auto getLogRotationSize() const { return m_logRotationSize; }
      auto getRotationLogInterval() const { return m_rotationLogInterval; }
      auto getLogLevel() const { return m_logLevel; }

      void setLoggingLevel(const boost::log::trivial::severity_level level);
      boost::log::trivial::severity_level setLoggingLevel(const std::string &level);

      boost::log::trivial::logger_type *getLogger() { return m_logger; }

    protected:
      DevicePtr defaultDevice();
      void loadAdapters(const ptree &tree, const ConfigOptions &options);
      void loadSinks(const ptree &sinks, ConfigOptions &options);

#ifdef WITH_PYTHON
      void configurePython(const ptree &tree, ConfigOptions &options);
#endif
#ifdef WITH_RUBY
      void configureRuby(const ptree &tree, ConfigOptions &options);
#endif

      void loadPlugins(const ptree &tree);
      bool loadPlugin(const std::string &name, const ptree &tree);

      std::optional<std::filesystem::path> checkPath(const std::string &name);

      void monitorFiles(boost::system::error_code ec);
      void scheduleMonitorTimer();

    protected:
      using text_sink = boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>;

      std::map<std::string, InitializationFunction> m_initializers;

      std::unique_ptr<AsyncContext> m_context;
      std::unique_ptr<Agent> m_agent;

      pipeline::PipelineContextPtr m_pipelineContext;
      std::unique_ptr<source::adapter::Handler> m_adapterHandler;
      boost::shared_ptr<text_sink> m_sink;
      std::string m_version;
      std::string m_devicesFile;
      std::filesystem::path m_exePath;
      std::filesystem::path m_working;

      // File monitoring
      boost::asio::steady_timer m_monitorTimer;
      bool m_monitorFiles = false;
      std::chrono::seconds m_monitorInterval;
      std::chrono::seconds m_monitorDelay;
      bool m_restart = false;
      std::optional<std::filesystem::file_time_type> m_configTime;
      std::optional<std::filesystem::file_time_type> m_deviceTime;

      // Logging info for testing
      std::filesystem::path m_logDirectory;
      std::filesystem::path m_logFileName;
      std::filesystem::path m_logArchivePattern;

      boost::log::trivial::severity_level m_logLevel {boost::log::trivial::severity_level::info};

      int64_t m_maxLogFileSize {0};
      int64_t m_logRotationSize {0};
      int64_t m_rotationLogInterval {0};

      // Factories
      sink::SinkFactory m_sinkFactory;
      source::SourceFactory m_sourceFactory;

      int m_workerThreadCount {1};

      // Reference to the global logger
      boost::log::trivial::logger_type *m_logger {nullptr};

#ifdef WITH_RUBY
      std::unique_ptr<ruby::Embedded> m_ruby;
#endif
#ifdef WITH_PYTHON
      std::unique_ptr<python::Embedded> m_python;
#endif
    };
  }  // namespace configuration
}  // namespace mtconnect
