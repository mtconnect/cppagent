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

#include "agent_config.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <boost/dll/import.hpp>
#include <boost/function.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/regex.hpp>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <algorithm>
#include <chrono>
#include <date/date.h>
#include <errno.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <sys/stat.h>
#include <vector>

#include "adapter/mqtt/mqtt_adapter.hpp"
#include "adapter/shdr/shdr_adapter.hpp"
#include "agent.hpp"
#include "configuration/config_options.hpp"
#include "device_model/device.hpp"
#include "rest_sink/rest_service.hpp"
#include "version.h"
#include "xml_printer.hpp"

#ifdef WITH_PYTHON
#include "python/embedded.hpp"
#endif

// If Windows XP
#if defined(_WINDOWS)
#if WINVER < 0x0600
#include "shlwapi.h"
#define stat(P, B) (PathFileExists((const char *)P) ? 0 : -1)
#endif
#endif

#ifdef MACOSX
#include <mach-o/dyld.h>
#endif

#define _strfy(line) #line
#define strfy(line) _strfy(line)

using namespace std;
namespace fs = std::filesystem;
namespace pt = boost::property_tree;
namespace blog = boost::log;
namespace dll = boost::dll;
namespace asio = boost::asio;

BOOST_LOG_ATTRIBUTE_KEYWORD(named_scope, "Scope", blog::attributes::named_scope::value_type);
BOOST_LOG_ATTRIBUTE_KEYWORD(utc_timestamp, "Timestamp",
                            blog::attributes::utc_clock::value_type);

namespace mtconnect {
  namespace configuration {

    AgentConfiguration::AgentConfiguration()
    {
      NAMED_SCOPE("AgentConfiguration::AgentConfiguration");

      bool success = false;

      rest_sink::RestService::registerFactory(m_sinkFactory);
      adapter::shdr::ShdrAdapter::registerFactory(m_sourceFactory);
      adapter::mqtt_adapter::MqttAdapter::registerFactory(m_sourceFactory);

#if _WINDOWS
      char execPath[MAX_PATH];
      memset(execPath, 0, MAX_PATH);
      success = GetModuleFileName(nullptr, execPath, MAX_PATH) > 0;
#else
#ifdef __linux__
      char execPath[PATH_MAX];
      memset(execPath, 0, PATH_MAX);
      success = readlink("/proc/self/exe", execPath, PATH_MAX) >= 0;
#else
#ifdef __APPLE__
      char execPath[PATH_MAX];
      uint32_t size = PATH_MAX;
      success = !_NSGetExecutablePath(execPath, &size);
#else
      success = false;
#endif
#endif
#endif

      m_working = fs::current_path();
      cout << "Configuration search path: " << m_working;
      if (success)
      {
        fs::path ep(execPath);
        m_exePath = ep.parent_path();
        cout << " and " << m_exePath;
      }
      cout << endl;
    }

    void AgentConfiguration::initialize(const boost::program_options::variables_map &options)
    {
      NAMED_SCOPE("AgentConfiguration::initialize");

      string configFile = "agent.cfg";
      if (options.count("config-file") > 0)
      {
        auto cfg = options["config-file"].as<boost::optional<string>>();
        if (cfg)
          configFile = *cfg;
      }

      try
      {
        list<fs::path> paths {m_working / configFile, m_exePath / configFile};
        for (auto path : paths)
        {
          // Check first if the file is in the current working directory...
          if (fs::exists(path))
          {
            LOG(info) << "Loading configuration from: " << path;
            cerr << "Loading configuration from:" << path;

            m_configFile = fs::absolute(path);
            ifstream file(m_configFile.c_str());
            std::stringstream buffer;
            buffer << file.rdbuf();

            loadConfig(buffer.str());

            return;
          }
          else
          {
            LOG(info) << "Cannot find config file:" << path << ", keep searching";
            cerr << "Cannot find config file:" << path << ", keep searching";
          }
        }

        LOG(fatal) << "Agent failed to load: Cannot find configuration file: '" << configFile;
        cerr << "Agent failed to load: Cannot find configuration file: '" << configFile
             << std::endl;
        usage(1);
      }
      catch (std::exception &e)
      {
        LOG(fatal) << "Agent failed to load: " << e.what() << " from " << m_configFile;
        cerr << "Agent failed to load: " << e.what() << " from " << m_configFile << std::endl;
        usage(1);
      }
    }

    AgentConfiguration::~AgentConfiguration()
    {
      blog::core::get()->remove_all_sinks();
      m_pipelineContext.reset();
      m_adapterHandler.reset();
      m_agent.reset();

      m_sinkFactory.clear();
      m_sourceFactory.clear();
      m_initializers.clear();

      if (m_sink)
        m_sink.reset();
    }

#ifdef _WINDOWS
    static time_t GetFileModificationTime(const string &file)
    {
      FILETIME createTime, accessTime, writeTime = {0, 0};
      auto handle =
          CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      if (handle == INVALID_HANDLE_VALUE)
      {
        LOG(warning) << "Could not find file: " << file;
        return 0;
      }
      if (!GetFileTime(handle, &createTime, &accessTime, &writeTime))
      {
        LOG(warning) << "GetFileTime failed for: " << file;
        writeTime = {0, 0};
      }
      CloseHandle(handle);

      uint64_t windowsFileTime =
          (writeTime.dwLowDateTime | uint64_t(writeTime.dwHighDateTime) << 32);

      return windowsFileTime / 10000000ull - 11644473600ull;
    }
#else
    static time_t GetFileModificationTime(const string &file)
    {
      struct stat buf = {0};
      if (stat(file.c_str(), &buf) != 0)
      {
        LOG(warning) << "Cannot stat file (" << errno << "): " << file;
        perror("Cannot stat file");
        return 0;
      }

      return buf.st_mtime;
    }
#endif

    void AgentConfiguration::monitorThread()
    {
      NAMED_SCOPE("AgentConfiguration::monitorThread");

      time_t devices_at_start = 0, cfg_at_start = 0;

      LOG(debug) << "Monitoring files: " << m_configFile << " and " << m_devicesFile
                 << ", will warm start if they change.";

      if ((cfg_at_start = GetFileModificationTime(m_configFile.string())) == 0)
      {
        LOG(warning) << "Cannot stat config file: " << m_configFile << ", exiting monitor";
        return;
      }
      if ((devices_at_start = GetFileModificationTime(m_devicesFile)) == 0)
      {
        LOG(warning) << "Cannot stat devices file: " << m_devicesFile << ", exiting monitor";
        return;
      }

      LOG(trace) << "Configuration start time: " << cfg_at_start;
      LOG(trace) << "Device start time: " << devices_at_start;

      bool changed = false;

      // Check every 10 seconds
      do
      {
        this_thread::sleep_for(10ms);

        time_t devices = 0, cfg = 0;
        bool check = true;

        if ((cfg = GetFileModificationTime(m_configFile.string())) == 0)
        {
          LOG(warning) << "Cannot stat config file: " << m_configFile << ", retrying in 10 seconds";
          check = false;
        }

        if ((devices = GetFileModificationTime(m_devicesFile)) == 0)
        {
          LOG(warning) << "Cannot stat devices file: " << m_devicesFile
                       << ", retrying in 10 seconds";
          check = false;
        }

        LOG(trace) << "Configuration times: " << cfg_at_start << " -- " << cfg;
        LOG(trace) << "Device times: " << devices_at_start << " -- " << devices;

        // Check if the files have changed.
        if (check && (cfg_at_start != cfg || devices_at_start != devices))
        {
          time_t now = time(nullptr);
          LOG(warning) << "Detected change in configuration files. Will reload when youngest file "
                          "is at least "
                       << m_minimumConfigReloadAge << " seconds old";
          LOG(warning) << "    Devices.xml file modified " << (now - devices) << " seconds ago";
          LOG(warning) << "    ...cfg file modified " << (now - cfg) << " seconds ago";

          changed =
              (now - cfg) > m_minimumConfigReloadAge && (now - devices) > m_minimumConfigReloadAge;
        }
      } while (!changed);  // && m_agent->is_running());

      // Restart agent if changed...
      // stop agent and signal to warm start
      if (!m_context.stopped() && changed)
      {
        LOG(warning)
            << "Monitor thread has detected change in configuration files, restarting agent.";

        m_restart = true;
        m_agent->stop();
        m_agent.reset();

        LOG(warning) << "Monitor agent has completed shutdown, reinitializing agent.";

        // Re initialize
        boost::program_options::variables_map options;
        boost::program_options::variable_value value(boost::optional<string>(m_configFile.string()),
                                                     false);
        options.insert(make_pair("config-file"s, value));
        initialize(options);
      }
      LOG(debug) << "Monitor thread is exiting";
    }

    void AgentConfiguration::start()
    {
      do
      {
        m_restart = false;
        if (m_monitorFiles)
        {
          // Start the file monitor to check for changes to cfg or devices.
          LOG(debug) << "Waiting for monitor thread to exit to restart agent";
          // mon = std::make_unique<>(
          // make_mfp(*this, &AgentConfiguration::monitorThread));
        }

        m_agent->start();

        for (int i = 0; i < m_workerThreadCount; i++)
        {
          m_workers.emplace_back(std::thread([this]() { m_context.run(); }));
        }
        for (auto &w : m_workers)
        {
          w.join();
        }

        if (m_restart && m_monitorFiles)
        {
          // Will destruct and wait to re-initialize.
          LOG(debug) << "Waiting for monitor thread to exit to restart agent";
          // mon.reset(nullptr);
          LOG(debug) << "Monitor has exited";
        }
      } while (m_restart);
    }

    void AgentConfiguration::stop()
    {
      LOG(info) << "Agent stopping";
      m_restart = false;
      m_agent->stop();
      m_context.stop();
      LOG(info) << "Agent Configuration stopped";
    }

    DevicePtr AgentConfiguration::defaultDevice() { return m_agent->defaultDevice(); }

    void AgentConfiguration::boost_set_log_level(const blog::trivial::severity_level level)
    {
      using namespace blog::trivial;
      blog::core::get()->set_filter(severity >= level);
    }

    static blog::trivial::severity_level boost_string_to_log_level(const std::string level)
    {
      using namespace blog::trivial;
      if (level == "LALL" || level == "ALL" || level == "all")
        return severity_level::trace;  // Boost.Log does not have "ALL" so "trace" is the lowest
      else if (level == "LNONE" || level == "NONE" || level == "none")
        return severity_level::fatal;  // Boost.Log does not have a "NONE"
      else if (level == "LTRACE" || level == "TRACE" || level == "trace")
        return severity_level::trace;
      else if (level == "LDEBUG" || level == "DEBUG" || level == "debug")
        return severity_level::debug;
      else if (level == "LINFO" || level == "INFO" || level == "info")
        return severity_level::info;
      else if (level == "LWARN" || level == "WARN" || level == "warn")
        return severity_level::warning;
      else if (level == "LERROR" || level == "ERROR" || level == "error")
        return severity_level::error;
      else if (level == "LFATAL" || level == "FATAL" || level == "fatal")
        return severity_level::fatal;

      return severity_level::info;
    }
    
    static inline int convertFileSize(const AgentConfiguration::ptree &config, const string &name, int &size)
    {
      static const regex pat("([0-9]+)([GgMmKkBb]*)");
      if (auto opt = config.get_optional<string>(name))
      {
        smatch match;
        if (regex_match(*opt, match, pat))
        {
          size = boost::lexical_cast<int>(match[1]);
          if (match.size() > 1)
          {
            switch (match[2].first[0])
            {
              case 'G':
              case 'g':
                size *= 1024;
                
              case 'M':
              case 'm':
                size *= 1024;

              case 'K':
              case 'k':
                size *= 1024;
            }
          }
        }
        else
        {
          cerr << "Invalid value for " << name << ": " << *opt << endl;
        }
      }
      
      return size;
    }

    void AgentConfiguration::configureLogger(const ptree &config)
    {
      using namespace blog::trivial;

      m_sink.reset();

      //// Add the commonly used attributes; includes TimeStamp, ProcessID and ThreadID and others
      blog::add_common_attributes();
      blog::core::get()->add_global_attribute("Scope", blog::attributes::named_scope());
      blog::core::get()->add_global_attribute(
          blog::aux::default_attribute_names::thread_id(),
          blog::attributes::current_thread_id());
      blog::core::get()->add_global_attribute("Timestamp", blog::attributes::utc_clock());
      
      auto logger = config.get_child_optional("logger_config");

      if (m_isDebug)
      {
        blog::add_console_log(
            std::cout, blog::keywords::format =
                           (blog::expressions::stream
                            << blog::expressions::format_date_time<boost::posix_time::ptime>(
                                   "Timestamp", "%Y-%m-%dT%H:%M:%S.%fZ ")
                            << "("
                            << blog::expressions::attr<
                                   blog::attributes::current_thread_id::value_type>("ThreadID")
                            << ") "
                            << "[" << blog::trivial::severity << "] " << named_scope << ": "
                            << blog::expressions::smessage));
        if (logger)
        {
          if (logger->get_optional<string>("logging_level"))
          {
            auto level = boost_string_to_log_level(logger->get<string>("logging_level"));
            if (level > severity_level::debug)
              level = severity_level::debug;
            boost_set_log_level(level);
          }
        }
        else
        {
          boost_set_log_level(boost_string_to_log_level("debug"));
        }
      }
      else
      {
        string activeName("agent.log");
        string name("agent_%Y-%m-%d_%H-%M-%S_%N.log");
        int max_size = 10 * 1024 * 1024;               // in MB
        int rotation_size = 2  * 1024 * 1024;           // in MB
        int rotation_time_interval = 0;  // in hr
        int max_index = 9;

        if (logger)
        {
          if (auto opt = logger->get_optional<string>("logging_level"))
            boost_set_log_level(boost_string_to_log_level(*opt));
          else
            boost_set_log_level(boost_string_to_log_level("info"));

          if (logger->get_optional<string>("output"))
          {
            auto output = logger->get<string>("output");
            if (output == "cout")
            {
              blog::add_console_log(
                  std::cout,
                  blog::keywords::format =
                      (blog::expressions::stream
                       << blog::expressions::format_date_time<boost::posix_time::ptime>(
                              "Timestamp", "%Y-%m-%dT%H:%M:%S.%fZ ")
                       << "("
                       << blog::expressions::attr<
                              blog::attributes::current_thread_id::value_type>("ThreadID")
                       << ") "
                       << "[" << blog::trivial::severity << "] " << named_scope << ": "
                       << blog::expressions::smessage));
              return;
            }
            else if (output == "cerr")
            {
              blog::add_console_log(
                  std::cerr,
                  blog::keywords::format =
                      (blog::expressions::stream
                       << blog::expressions::format_date_time<boost::posix_time::ptime>(
                              "Timestamp", "%Y-%m-%dT%H:%M:%S.%fZ ")
                       << "("
                       << blog::expressions::attr<
                              blog::attributes::current_thread_id::value_type>("ThreadID")
                       << ") "
                       << "[" << blog::trivial::severity << "] " << named_scope << ": "
                       << blog::expressions::smessage));
              return;
            }
            else
            {
              vector<string> parts;
              boost::split(parts, output, boost::is_space());
              if (parts.size() > 0)
              {
                if (parts[0] == "file" && parts.size() > 1)
                  name = parts[1];
                else
                  name = parts[0];
                if (parts.size() > 2)
                  activeName = parts[2];
              }
            }
          }

          convertFileSize(*logger, "max_size", max_size);
          convertFileSize(*logger, "rotation_size", rotation_size);

          if (auto opt = logger->get_optional<string>("max_index"))
          {
            try {
              max_index = boost::lexical_cast<int>(*opt);
            }
            catch (boost::bad_lexical_cast &) {
              cerr << "Bad value for logger max_index: " << *opt << endl;
            }
          }

          if (logger->get_optional<string>("schedule"))
          {
            auto sched = logger->get<string>("schedule");
            if (sched == "DAILY")
              rotation_time_interval = 24;
            else if (sched == "WEEKLY")
              rotation_time_interval = 168;
            else if (sched != "NEVER")
              LOG(error) << "Invalid schedule value.";
          }
        }
        
        fs::path log_path(name);
        if (log_path.is_relative())
          log_path = fs::current_path() / log_path;
        
        name = log_path.filename();
                
        fs::path log_directory(log_path.parent_path());
        
        fs::path active_name(activeName);
        if (!active_name.has_parent_path())
          active_name = log_directory / active_name;
        
        boost::shared_ptr<blog::core> core = blog::core::get();

        // Create a text file sink
        namespace kw = boost::log::keywords;
        using text_sink = blog::sinks::synchronous_sink<blog::sinks::text_file_backend>;
        auto m_sink = boost::make_shared<text_sink>(kw::file_name = active_name,
                                                    kw::target_file_name = name,
                                                    kw::auto_flush = true,
                                                    kw::rotation_size = rotation_size,
                                                    kw::open_mode = ios_base::out | ios_base::app);

        // Set up where the rotated files will be stored
        m_sink->locked_backend()->set_file_collector(blog::sinks::file::make_collector(
            kw::target = log_directory,
            kw::max_size = max_size,
            kw::max_files = max_index));

        if (rotation_time_interval > 0)
          m_sink->locked_backend()->set_time_based_rotation(
              blog::sinks::file::rotation_at_time_interval(
                  boost::posix_time::hours(rotation_time_interval)));

        // Upon restart, scan the target directory for files matching the file_name pattern
        m_sink->locked_backend()->scan_for_files();

        // Formatter for the logger
        m_sink->set_formatter(
            blog::expressions::stream
            << blog::expressions::format_date_time<boost::posix_time::ptime>(
                   "Timestamp", "%Y-%m-%dT%H:%M:%S.%fZ ")
            << "("
            << blog::expressions::attr<blog::attributes::current_thread_id::value_type>(
                   "ThreadID")
            << ") "
            << "[" << boost::log::trivial::severity << "] " << named_scope << ": "
            << blog::expressions::smessage);

        core->add_sink(m_sink);
      }
    }

    std::optional<fs::path> AgentConfiguration::checkPath(const std::string &name)
    {
      auto work = m_working / name;
      if (fs::exists(work))
      {
        return work;
      }
      if (!m_exePath.empty())
      {
        auto exec = m_exePath / name;
        if (fs::exists(exec))
        {
          return exec;
        }
      }

      return nullopt;
    }

    void AgentConfiguration::loadConfig(const std::string &file)
    {
      NAMED_SCOPE("AgentConfiguration::loadConfig");

      // Now get our configuration
      auto config = Parser::parse(file);

      // if (!m_loggerFile)
      if (!m_sink)
      {
        configureLogger(config);
      }

      ConfigOptions options;
      GetOptions(config, options,
                 {{configuration::PreserveUUID, true},
                  {configuration::WorkingDirectory, m_working.string()},
                  {configuration::ExecDirectory, m_exePath.string()},
                  {configuration::ServerIp, "0.0.0.0"s},
                  {configuration::BufferSize, int(DEFAULT_SLIDING_BUFFER_EXP)},
                  {configuration::MaxAssets, int(DEFAULT_MAX_ASSETS)},
                  {configuration::CheckpointFrequency, 1000},
                  {configuration::LegacyTimeout, 600s},
                  {configuration::ReconnectInterval, 10000ms},
                  {configuration::IgnoreTimestamps, false},
                  {configuration::ConversionRequired, true},
                  {configuration::UpcaseDataItemValue, true},
                  {configuration::FilterDuplicates, false},
                  {configuration::MonitorConfigFiles, false},
                  {configuration::MinimumConfigReloadAge, 15},
                  {configuration::Pretty, false},
                  {configuration::PidFile, "agent.pid"s},
                  {configuration::Port, 5000},
                  {configuration::MaxCachedFileSize, "20k"s},
                  {configuration::ServiceName, "MTConnect Agent"s},
                  {configuration::SchemaVersion,
                   to_string(AGENT_VERSION_MAJOR) + "."s + to_string(AGENT_VERSION_MINOR)},
                  {configuration::LogStreams, false},
                  {configuration::ShdrVersion, 1},
                  {configuration::WorkerThreads, 1},
                  {configuration::TlsCertificateChain, ""s},
                  {configuration::TlsPrivateKey, ""s},
                  {configuration::TlsDHKey, ""s},
                  {configuration::TlsCertificatePassword, ""s},
                  {configuration::AllowPut, false},
                  {configuration::TlsOnly, false},
                  {configuration::TlsVerifyClientCertificate, false},
                  {configuration::TlsClientCAs, ""s},
                  {configuration::SuppressIPAddress, false},
                  {configuration::AllowPutFrom, ""s}});

      m_workerThreadCount = *GetOption<int>(options, configuration::WorkerThreads);

      auto devices = config.get_optional<string>(configuration::Devices);
      if (devices)
      {
        auto name = *devices;
        auto path = checkPath(name);
        if (path)
          m_devicesFile = (*path).string();
      }
      else
      {
        auto path = checkPath("Devices.xml");
        if (path)
          m_devicesFile = (*path).string();
        else
        {
          auto probe = checkPath("probe.xml");
          if (probe)
            m_devicesFile = (*probe).string();
        }
      }

      if (m_devicesFile.empty())
      {
        throw runtime_error(((string) "Please make sure the configuration "
                                      "file probe.xml or Devices.xml is in the current "
                                      "directory or specify the correct file "
                                      "in the configuration file " +
                             m_configFile.string() + " using Devices = <file>")
                                .c_str());
      }

      m_name = get<string>(options[configuration::ServiceName]);

      auto plugins = config.get_child_optional("Plugins");
      if (plugins)
      {
        loadPlugins(*plugins);
      }

      // Check for schema version
      m_version = get<string>(options[configuration::SchemaVersion]);
      auto port = get<int>(options[configuration::Port]);
      LOG(info) << "Starting agent on port " << int(port);

      // Make the Agent
      m_agent = make_unique<Agent>(m_context, m_devicesFile, options);

      // Make the PipelineContext
      m_pipelineContext = std::make_shared<pipeline::PipelineContext>();
      m_pipelineContext->m_contract = m_agent->makePipelineContract();

      loadSinks(config, options);

      m_agent->initialize(m_pipelineContext);

      DevicePtr device;
      if (get<bool>(options[configuration::PreserveUUID]))
      {
        for (auto device : m_agent->getDevices())
          device->setPreserveUuid(get<bool>(options[configuration::PreserveUUID]));
      }

      loadAdapters(config, options);

#ifdef WITH_PYTHON
      configurePython(config, options);
#endif
    }

    void parseUrl(ConfigOptions &options)
    {
      string host, protocol, path;
      auto url = *GetOption<string>(options, configuration::Url);

      boost::regex pat("^([^:]+)://([^:/]+)(:[0-9]+)?(/?.+)");
      boost::match_results<string::const_iterator> match;
      if (boost::regex_match(url, match, pat))
      {
        if (match[1].matched)
          options[configuration::Protocol] = string(match[1].first, match[1].second);
        if (match[2].matched)
          options[configuration::Host] = string(match[2].first, match[2].second);
        if (match[3].matched)
        {
          try
          {
            options[configuration::Port] =
                boost::lexical_cast<int>(string(match[3].first + 1, match[3].second).c_str());
          }
          catch (boost::bad_lexical_cast &e)
          {
            LOG(error) << "Cannot intrepret the port for " << match[3] << ": " << e.what();
          }
        }
        if (match[4].matched)
          options[configuration::Topics] = StringList {string(match[4].first, match[4].second)};
      }
    }

    void AgentConfiguration::loadAdapters(const pt::ptree &config, const ConfigOptions &options)
    {
      using namespace adapter;
      using namespace pipeline;

      NAMED_SCOPE("AgentConfiguration::loadAdapters");

      DevicePtr device;
      auto adapters = config.get_child_optional("Adapters");
      if (adapters)
      {
        for (const auto &block : *adapters)
        {
          ConfigOptions adapterOptions = options;

          GetOptions(block.second, adapterOptions, options);
          AddOptions(block.second, adapterOptions,
                     {{configuration::Url, string()}, {configuration::Device, string()}});

          auto qname = entity::QName(block.first);
          auto [factory, name] = qname.getPair();

          auto deviceName = GetOption<string>(adapterOptions, configuration::Device).value_or(name);
          device = m_agent->getDeviceByName(deviceName);

          if (!device)
          {
            LOG(warning) << "Cannot locate device name '" << deviceName << "', trying default";
            device = defaultDevice();
            if (device)
            {
              deviceName = *device->getComponentName();
              adapterOptions[configuration::Device] = deviceName;
              LOG(info) << "Assigning default device " << deviceName << " to adapter";
            }
          }
          else
          {
            adapterOptions[configuration::Device] = *device->getUuid();
          }
          if (!device)
          {
            LOG(warning) << "Cannot locate device name '" << deviceName << "', assuming dynamic";
          }

          auto additional = block.second.get_optional<string>(configuration::AdditionalDevices);
          if (additional)
          {
            StringList deviceList;
            istringstream devices(*additional);
            string name;
            while (getline(devices, name, ','))
            {
              auto index = name.find_first_not_of(" \r\t");
              if (index != string::npos && index > 0)
                name.erase(0, index);
              index = name.find_last_not_of(" \r\t");
              if (index != string::npos)
                name.erase(index + 1);

              deviceList.push_back(name);
            }

            adapterOptions[configuration::AdditionalDevices] = deviceList;
          }

          // Get protocol, hosts, and topics from URL
          if (HasOption(adapterOptions, configuration::Url))
          {
            parseUrl(adapterOptions);
          }

          // Override if protocol if not specified
          string protocol;
          AddDefaultedOptions(block.second, adapterOptions, {{configuration::Protocol, "shdr"s}});
          protocol = *GetOption<string>(adapterOptions, configuration::Protocol);

          if (factory.empty())
            factory = protocol;

          if (!m_sourceFactory.hasFactory(factory) && !loadPlugin(factory, block.second))
            continue;

          auto source = m_sourceFactory.make(factory, name, m_context, m_pipelineContext,
                                             adapterOptions, block.second);

          if (source)
          {
            m_agent->addSource(source, false);
            LOG(info) << protocol << ": Adding adapter for " << deviceName << ": " << block.first;
          }
        }
      }
      else if ((device = defaultDevice()))
      {
        ConfigOptions adapterOptions {options};

        auto deviceName = *device->getComponentName();
        adapterOptions[configuration::Device] = deviceName;
        LOG(info) << "Adding default adapter for " << device->getName() << " on localhost:7878";

        auto source = m_sourceFactory.make("shdr", "default", m_context, m_pipelineContext,
                                           adapterOptions, ptree {});
        m_agent->addSource(source, false);
      }
      else
      {
        throw runtime_error("Adapters must be defined if more than one device is present");
      }
    }

#ifdef WITH_PYTHON
    void AgentConfiguration::configurePython(const ptree &tree, ConfigOptions &options)
    {
      m_python = make_unique<python::Embedded>(m_agent.get(), options);
    }
#endif

    void AgentConfiguration::loadSinks(const ptree &config, ConfigOptions &options)
    {
      NAMED_SCOPE("AgentConfiguration::loadSinks");

      auto sinks = config.get_child_optional("Sinks");
      if (sinks)
      {
        for (const auto &sinkBlock : *sinks)
        {
          auto qname = entity::QName(sinkBlock.first);
          auto [factory, name] = qname.getPair();

          if (factory.empty())
            factory = name;

          if (!m_sinkFactory.hasFactory(factory))
          {
            if (!loadPlugin(factory, sinkBlock.second))
              continue;
          }

          ConfigOptions sinkOptions = options;
          GetOptions(sinkBlock.second, sinkOptions, options);
          AddOptions(sinkBlock.second, sinkOptions, {{"Name", string()}});

          auto sinkName = GetOption<string>(sinkOptions, "Name").value_or(name);
          auto sinkContract = m_agent->makeSinkContract();
          sinkContract->m_pipelineContext = m_pipelineContext;

          auto sink = m_sinkFactory.make(factory, sinkName, m_context, std::move(sinkContract),
                                         options, sinkBlock.second);
          if (sink)
          {
            m_agent->addSink(sink);
            LOG(info) << "Loaded sink plugin " << sinkBlock.first;
          }
        }
      }

      // Make sure we have a rest sink
      auto rest = m_agent->findSink("RestService");
      if (!rest)
      {
        auto sinkContract = m_agent->makeSinkContract();
        sinkContract->m_pipelineContext = m_pipelineContext;

        auto sink = m_sinkFactory.make("RestService", "RestService", m_context,
                                       std::move(sinkContract), options, config);
        m_agent->addSink(sink);
      }
    }

    void AgentConfiguration::loadPlugins(const ptree &plugins)
    {
      NAMED_SCOPE("AgentConfiguration::loadPlugins");

      for (const auto &plugin : plugins)
      {
        loadPlugin(plugin.first, plugin.second);
      }
    }

    bool AgentConfiguration::loadPlugin(const std::string &name, const ptree &plugin)
    {
      NAMED_SCOPE("AgentConfiguration::loadPlugin");

      namespace dll = boost::dll;
      namespace fs = boost::filesystem;

      auto sharedLibPath = dll::program_location().parent_path();

      // Cache the initializers to avoid reload and keep a reference to the
      // dll so it does not get unloaded.
      // Check if already loaded
      if (m_initializers.count(name) > 0)
        return true;

      // Try to find the plugin in the path or the application or
      // current working directory.
      list<fs::path> paths {dll::detail::shared_library_impl::decorate(sharedLibPath / name),
                            fs::current_path() / name};

      for (auto path : paths)
      {
        try
        {
          InitializationFunction init =
              dll::import_alias<InitializationFn>(path,  // path to library
                                                  "initialize_plugin");

          // Remember this initializer so it does not get unloaded.
          m_initializers.insert_or_assign(name, init);

          // Register the plugin
          init(plugin, *this);
          return true;
        }
        catch (exception &e)
        {
          LOG(info) << "Cannot load plugin " << name << " from " << path << " Reason: " << e.what();
        }
      }

      // If the paths did not match, return false.
      return false;
    }
  }  // namespace configuration
}  // namespace mtconnect
