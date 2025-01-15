//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "async_context.hpp"
#include "hook_manager.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/sink/rest_sink/file_cache.hpp"
#include "mtconnect/sink/sink.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/shdr/shdr_pipeline.hpp"
#include "mtconnect/source/source.hpp"
#include "mtconnect/utilities.hpp"
#include "parser.hpp"
#include "service.hpp"

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

  /// @brief Configuration namespace
  namespace configuration {
    using DevicePtr = std::shared_ptr<device_model::Device>;

    /// @brief Parses the configuration file and creates the `Agent`. Manages config
    ///        file tracking and restarting of the agent.
    class AGENT_LIB_API AgentConfiguration : public MTConnectService
    {
    public:
      enum FileFormat
      {
        MTCONNECT,
        JSON,
        XML,
        UNKNOWN
      };

      using InitializationFn = void(const boost::property_tree::ptree &, AgentConfiguration &);
      using InitializationFunction = boost::function<InitializationFn>;

      using ptree = boost::property_tree::ptree;

      /// @brief Construct the agent configuration
      AgentConfiguration();
      virtual ~AgentConfiguration();

      /// @name Callbacks for initialization phases
      ///@{

      /// @brief Get the callback manager after the agent is created
      /// @return the callback manager
      auto &afterAgentHooks() { return m_afterAgentHooks; }
      /// @brief Get the callback manager after the agent is started
      /// @return the callback manager
      auto &beforeStartHooks() { return m_beforeStartHooks; }
      /// @brief Get the callback manager after the agent is stopped
      /// @return the callback manager
      auto &beforeStopHooks() { return m_beforeStopHooks; }
      ///@}

      /// @brief stops the agent. Used in daemons.
      void stop() override;
      /// @brief starts the agent. Used in daemons.
      void start() override;
      /// @brief initializes the configuration of the agent from the command line parameters
      /// @param[in] options command line parameters
      void initialize(const boost::program_options::variables_map &options) override;

      /// @brief  Configure the logger with the config node from the config file
      /// @param channelName the log channel name
      /// @param config the configuration node
      /// @param formatter optional custom message format
      void configureLoggerChannel(
          const std::string &channelName, const ptree &config,
          std::optional<boost::log::basic_formatter<char>> formatter = std::nullopt);

      /// @brief  Configure the agent logger with the config node from the config file
      /// @param config the configuration node
      void configureLogger(const ptree &config);

      /// @brief load a configuration text
      /// @param[in] text the configuration text loaded from a file
      /// @param[in] fmt the file format, can be MTCONNECT, JSON, or XML
      void loadConfig(const std::string &text, FileFormat fmt = MTCONNECT);

      /// @brief assign the agent associated with this configuration
      /// @param[in] agent the agent the configuration will take ownership of
      void setAgent(std::unique_ptr<Agent> &agent) { m_agent = std::move(agent); }
      /// @brief get the agent associated with the configuration
      Agent *getAgent() const { return m_agent.get(); }
      /// @brief get the boost asio io context
      auto &getContext() { return m_context->get(); }
      /// @brief get a pointer to the async io manager
      auto &getAsyncContext() { return *m_context.get(); }

      /// @brief sets the path for the working directory to the current path
      void updateWorkingDirectory() { m_working = std::filesystem::current_path(); }

      /// @name Configuration factories
      ///@{
      /// @brief get the factory for creating sinks
      /// @return the factory
      auto &getSinkFactory() { return m_sinkFactory; }
      /// @brief get the factory for creating sources
      /// @return the factory
      auto &getSourceFactory() { return m_sourceFactory; }
      ///@}

      /// @brief get the pipeline context for this configuration
      /// @note set after the agent is created
      auto getPipelineContext() { return m_pipelineContext; }

      /// @name Logging methods
      ///@{
      /// @brief gets the boost log sink
      /// @return boost log sink
      const auto &getLoggerSink(const std::string &channelName = "agent")
      {
        return m_logChannels[channelName].m_logSink;
      }
      /// @brief gets the log directory
      /// @return log directory
      const auto &getLogDirectory(const std::string &channelName = "agent")
      {
        return m_logChannels[channelName].m_logDirectory;
      }
      /// @brief get the logging file name
      /// @return log file name
      const auto &getLogFileName(const std::string &channelName = "agent")
      {
        return m_logChannels[channelName].m_logFileName;
      }
      /// @brief for log rolling, get the log archive pattern
      /// @return log archive pattern
      const auto &getLogArchivePattern(const std::string &channelName = "agent")
      {
        return m_logChannels[channelName].m_logArchivePattern;
      }
      /// @brief Get the maximum size of all the log files
      /// @return the maximum size of all log files
      auto getMaxLogFileSize(const std::string &channelName = "agent")
      {
        return m_logChannels[channelName].m_maxLogFileSize;
      }
      /// @brief the maximum size of a log file when it triggers rolling over
      /// @return the maxumum site of a log file
      auto getLogRotationSize(const std::string &channelName = "agent")
      {
        return m_logChannels[channelName].m_logRotationSize;
      }
      /// @brief How often to roll over the log file
      ///
      /// One of:
      /// - `DAILY`
      /// - `WEEKLY`
      /// - `NEVER`
      ///
      /// @return the log file interval
      auto getRotationLogInterval(const std::string &channelName = "agent")
      {
        return m_logChannels[channelName].m_rotationLogInterval;
      }
      /// @brief Get the current log level
      /// @return log level
      auto getLogLevel(const std::string &channelName = "agent")
      {
        return m_logChannels[channelName].m_logLevel;
      }

      /// @brief set the logging level
      /// @param[in] level the new logging level
      void setLoggingLevel(const boost::log::trivial::severity_level level);
      /// @brief Set the logging level as a string
      /// @param level the new logging level
      /// @return the logging level
      boost::log::trivial::severity_level setLoggingLevel(const std::string &level);

      std::optional<std::filesystem::path> findConfigFile(const std::string &file)
      {
        return findFile(m_configPaths, file);
      }

      std::optional<std::filesystem::path> findDataFile(const std::string &file)
      {
        return findFile(m_dataPaths, file);
      }

      /// @brief Create a sink contract with functions to find config and data files.
      /// @return shared pointer to a sink contract
      sink::SinkContractPtr makeSinkContract()
      {
        auto contract = m_agent->makeSinkContract();
        contract->m_findConfigFile =
            [this](const std::string &n) -> std::optional<std::filesystem::path> {
          return findConfigFile(n);
        };
        contract->m_findDataFile =
            [this](const std::string &n) -> std::optional<std::filesystem::path> {
          return findDataFile(n);
        };
        return contract;
      }

      /// @brief add a path to the config paths
      /// @param path the path to add
      void addConfigPath(const std::filesystem::path &path) { addPathBack(m_configPaths, path); }

      /// @brief add a path to the data paths
      /// @param path the path to add
      void addDataPath(const std::filesystem::path &path) { addPathBack(m_dataPaths, path); }

      /// @brief add a path to the plugin paths
      /// @param path the path to add
      void addPluginPath(const std::filesystem::path &path) { addPathBack(m_pluginPaths, path); }

    protected:
      DevicePtr getDefaultDevice();
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
      void monitorFiles(boost::system::error_code ec);
      void scheduleMonitorTimer();

    protected:
      std::optional<std::filesystem::path> findFile(const std::list<std::filesystem::path> &paths,
                                                    const std::string file)
      {
        for (const auto &path : paths)
        {
          auto tst = path / file;
          std::error_code ec;
          if (std::filesystem::exists(tst, ec) && !ec)
          {
            LOG(debug) << "Found file '" << file << "' "
                       << " in path " << path;
            auto con {std::filesystem::canonical(tst)};
            return con;
          }
          else
          {
            LOG(debug) << "Cannot find file '" << file << "' "
                       << " in path " << path;
          }
        }

        return std::nullopt;
      }

      void addPathBack(std::list<std::filesystem::path> &paths, std::filesystem::path path)
      {
        std::error_code ec;
        auto con {std::filesystem::canonical(path, ec)};
        if (std::find(paths.begin(), paths.end(), con) != paths.end())
          return;

        if (!ec)
          paths.emplace_back(con);
        else
          LOG(debug) << "Cannot file path: " << path << ", " << ec.message();
      }

      void addPathFront(std::list<std::filesystem::path> &paths, std::filesystem::path path)
      {
        std::error_code ec;
        auto con {std::filesystem::canonical(path, ec)};
        paths.remove(con);
        if (!ec)
          paths.emplace_front(con);
        else
          LOG(debug) << "Cannot file path: " << path << ", " << ec.message();
      }

      template <typename T>
      void logPaths(T lvl, const std::list<std::filesystem::path> &paths)
      {
        for (const auto &p : paths)
        {
          BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),
                                       (::boost::log::keywords::severity = lvl))
              << "  " << p;
        }
      }

      void expandConfigVariables(boost::property_tree::ptree &);

    protected:
      using text_sink = boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>;
      using console_sink =
          boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>;

      struct LogChannel
      {
        std::string m_channelName;
        std::filesystem::path m_logDirectory;
        std::filesystem::path m_logArchivePattern;
        std::filesystem::path m_logFileName;

        int64_t m_maxLogFileSize {0};
        int64_t m_logRotationSize {0};
        int64_t m_rotationLogInterval {0};

        boost::log::trivial::severity_level m_logLevel {boost::log::trivial::severity_level::info};

        boost::shared_ptr<boost::log::sinks::sink> m_logSink;
      };

      std::map<std::string, LogChannel> m_logChannels;

      std::map<std::string, InitializationFunction> m_initializers;

      std::unique_ptr<AsyncContext> m_context;
      std::unique_ptr<Agent> m_agent;

      pipeline::PipelineContextPtr m_pipelineContext;
      std::unique_ptr<source::adapter::Handler> m_adapterHandler;

      std::string m_version;
      std::string m_devicesFile;
      std::filesystem::path m_exePath;
      std::filesystem::path m_working;

      std::list<std::filesystem::path> m_configPaths;
      std::list<std::filesystem::path> m_dataPaths;
      std::list<std::filesystem::path> m_pluginPaths;

      // File monitoring
      boost::asio::steady_timer m_monitorTimer;
      bool m_monitorFiles = false;
      std::chrono::seconds m_monitorInterval;
      std::chrono::seconds m_monitorDelay;
      bool m_restart = false;
      std::optional<std::filesystem::file_time_type> m_configTime;
      std::optional<std::filesystem::file_time_type> m_deviceTime;

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

      HookManager<AgentConfiguration> m_afterAgentHooks;
      HookManager<AgentConfiguration> m_beforeStartHooks;
      HookManager<AgentConfiguration> m_beforeStopHooks;
    };
  }  // namespace configuration
}  // namespace mtconnect
