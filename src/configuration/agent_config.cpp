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

#include <adapter/mqtt/mqtt_adapter.hpp>
#include <adapter/shdr/shdr_adapter.hpp>
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

#include "agent.hpp"
#include "configuration/config_options.hpp"
#include "device_model/device.hpp"
#include "option.hpp"
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
namespace b_logger = boost::log;
namespace dll = boost::dll;
namespace asio = boost::asio;

BOOST_LOG_ATTRIBUTE_KEYWORD(named_scope, "Scope", b_logger::attributes::named_scope::value_type);
BOOST_LOG_ATTRIBUTE_KEYWORD(utc_timestamp, "Timestamp",
                            b_logger::attributes::utc_clock::value_type);

namespace mtconnect {
  namespace configuration {

    AgentConfiguration::AgentConfiguration()
    {
      NAMED_SCOPE("AgentConfiguration::AgentConfiguration");

      bool success = false;

      rest_sink::RestService::registerFactory();
      adapter::shdr::ShdrAdapter::registerFactory();
      adapter::mqtt_adapter::MqttAdapter::registerFactory();

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
        m_exePath = ep.root_path().parent_path();
        cout << " and " << m_exePath;
      }
      cout << endl;
    }

    void AgentConfiguration::initialize(int argc, const char *argv[])
    {
      NAMED_SCOPE("AgentConfiguration::initialize");

      MTConnectService::initialize(argc, argv);

      const char *configFile = "agent.cfg";

      OptionsList optionList;
      optionList.append(new Option(0, configFile, "The configuration file", "file", false));
      optionList.parse(argc, (const char **)argv);

      m_configFile = configFile;

      try
      {
        struct stat buf = {0};

        // Check first if the file is in the current working directory...
        if (stat(m_configFile.c_str(), &buf))
        {
          if (!m_exePath.empty())
          {
            LOG(info) << "Cannot find " << m_configFile
                      << " in current directory, searching exe path: " << m_exePath;
            cerr << "Cannot find " << m_configFile
                 << " in current directory, searching exe path: " << m_exePath << endl;
            m_configFile = (m_exePath / m_configFile).string();
          }
          else
          {
            LOG(fatal) << "Agent failed to load: Cannot find configuration file: '" << m_configFile;
            cerr << "Agent failed to load: Cannot find configuration file: '" << m_configFile
                 << std::endl;
            optionList.usage();
          }
        }

        ifstream file(m_configFile.c_str());
        std::stringstream buffer;
        buffer << file.rdbuf();

        loadConfig(buffer.str());
      }
      catch (std::exception &e)
      {
        LOG(fatal) << "Agent failed to load: " << e.what();
        cerr << "Agent failed to load: " << e.what() << std::endl;
        optionList.usage();
      }
    }

    AgentConfiguration::~AgentConfiguration()
    {
      b_logger::core::get()->remove_all_sinks();
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

      if ((cfg_at_start = GetFileModificationTime(m_configFile)) == 0)
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

        if ((cfg = GetFileModificationTime(m_configFile)) == 0)
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
        const char *argv[] = {m_configFile.c_str()};
        initialize(1, argv);
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

    void AgentConfiguration::boost_set_log_level(const b_logger::trivial::severity_level level)
    {
      using namespace b_logger::trivial;
      b_logger::core::get()->set_filter(severity >= level);
    }

    static b_logger::trivial::severity_level boost_string_to_log_level(const std::string level)
    {
      using namespace b_logger::trivial;
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

    void AgentConfiguration::configureLogger(const ptree &config)
    {
      using namespace b_logger::trivial;

      m_sink.reset();

      //// Add the commonly used attributes; includes TimeStamp, ProcessID and ThreadID and others
      b_logger::add_common_attributes();
      b_logger::core::get()->add_global_attribute("Scope", b_logger::attributes::named_scope());
      b_logger::core::get()->add_global_attribute(
          b_logger::aux::default_attribute_names::thread_id(),
          b_logger::attributes::current_thread_id());
      b_logger::core::get()->add_global_attribute("Timestamp", b_logger::attributes::utc_clock());

      auto logger = config.get_child_optional("logger_config");

      if (m_isDebug)
      {
        b_logger::add_console_log(
            std::cout, b_logger::keywords::format =
                           (b_logger::expressions::stream
                            << b_logger::expressions::format_date_time<boost::posix_time::ptime>(
                                   "Timestamp", "%Y-%m-%dT%H:%M:%S.%fZ ")
                            << "("
                            << b_logger::expressions::attr<
                                   b_logger::attributes::current_thread_id::value_type>("ThreadID")
                            << ") "
                            << "[" << b_logger::trivial::severity << "] " << named_scope << ": "
                            << b_logger::expressions::smessage));
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
        string name("agent.log");
        int max_size = 10;               // in MB
        int rotation_size = 2;           // in MB
        int rotation_time_interval = 0;  // in hr
        int max_index = 9;

        if (logger)
        {
          if (logger->get_optional<string>("logging_level"))
            boost_set_log_level(boost_string_to_log_level(logger->get<string>("logging_level")));
          else
            boost_set_log_level(boost_string_to_log_level("info"));

          if (logger->get_optional<string>("output"))
          {
            auto output = logger->get<string>("output");
            if (output == "cout")
            {
              b_logger::add_console_log(
                  std::cout,
                  b_logger::keywords::format =
                      (b_logger::expressions::stream
                       << b_logger::expressions::format_date_time<boost::posix_time::ptime>(
                              "Timestamp", "%Y-%m-%dT%H:%M:%S.%fZ ")
                       << "("
                       << b_logger::expressions::attr<
                              b_logger::attributes::current_thread_id::value_type>("ThreadID")
                       << ") "
                       << "[" << b_logger::trivial::severity << "] " << named_scope << ": "
                       << b_logger::expressions::smessage));
              return;
            }
            else if (output == "cerr")
            {
              b_logger::add_console_log(
                  std::cerr,
                  b_logger::keywords::format =
                      (b_logger::expressions::stream
                       << b_logger::expressions::format_date_time<boost::posix_time::ptime>(
                              "Timestamp", "%Y-%m-%dT%H:%M:%S.%fZ ")
                       << "("
                       << b_logger::expressions::attr<
                              b_logger::attributes::current_thread_id::value_type>("ThreadID")
                       << ") "
                       << "[" << b_logger::trivial::severity << "] " << named_scope << ": "
                       << b_logger::expressions::smessage));
              return;
            }
            else
            {
              istringstream sin(output);
              string one, two, three;
              sin >> one;
              sin >> two;
              sin >> three;
              if (one == "file" && three.empty())
                name = two;
              else
                name = one;
            }
          }

          if (logger->get_optional<string>("max_size"))
          {
            stringstream ss(logger->get<string>("max_size"));
            ss >> max_size;
          }

          if (logger->get_optional<string>("max_index"))
          {
            stringstream si(logger->get<string>("max_index"));
            si >> max_index;
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

        auto log_directory = fs::path(name).parent_path();
        if (!log_directory.is_absolute())
          log_directory = fs::absolute(fs::current_path() / log_directory);

        log_directory /= name;

        boost::shared_ptr<b_logger::core> core = b_logger::core::get();

        // Create a text file sink
        typedef b_logger::sinks::synchronous_sink<b_logger::sinks::text_file_backend> text_sink;
        boost::shared_ptr<text_sink> m_sink = boost::make_shared<text_sink>();
        m_sink->locked_backend()->set_file_name_pattern(name);
        m_sink->locked_backend()->set_target_file_name_pattern(log_directory);
        m_sink->locked_backend()->auto_flush(true);
        m_sink->locked_backend()->set_open_mode(ios_base::out | ios_base::app);
        m_sink->locked_backend()->set_rotation_size(rotation_size * 1024 * 1024);

        // Set up where the rotated files will be stored
        m_sink->locked_backend()->set_file_collector(b_logger::sinks::file::make_collector(
            b_logger::keywords::target = log_directory,
            b_logger::keywords::max_size = max_size * 1024 * 1024,
            b_logger::keywords::max_files = max_index));

        if (rotation_time_interval > 0)
          m_sink->locked_backend()->set_time_based_rotation(
              b_logger::sinks::file::rotation_at_time_interval(
                  boost::posix_time::hours(rotation_time_interval)));

        // Upon restart, scan the target directory for files matching the file_name pattern
        m_sink->locked_backend()->scan_for_files();

        // Formatter for the logger
        m_sink->set_formatter(
            b_logger::expressions::stream
            << b_logger::expressions::format_date_time<boost::posix_time::ptime>(
                   "Timestamp", "%Y-%m-%dT%H:%M:%S.%fZ ")
            << "("
            << b_logger::expressions::attr<b_logger::attributes::current_thread_id::value_type>(
                   "ThreadID")
            << ") "
            << "[" << boost::log::trivial::severity << "] " << named_scope << ": "
            << b_logger::expressions::smessage);

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
                             m_configFile + " using Devices = <file>")
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
      LOG(info) << "Starting agent on port " << port;

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

          if (!Source::hasFactory(factory) && !loadPlugin(factory, block.second))
            continue;

          auto source = Source::make(factory, name, m_context, m_pipelineContext, adapterOptions,
                                     block.second);

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

        auto source =
            Source::make("shdr", "default", m_context, m_pipelineContext, adapterOptions, ptree {});
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

          if (!Sink::hasFactory(factory))
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
          
          auto sink = Sink::make(factory, sinkName, m_context, std::move(sinkContract), options,
                                 sinkBlock.second);
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

        auto sink = Sink::make("RestService", "RestService", m_context, std::move(sinkContract),
                               options, config);
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

      boost::filesystem::path sharedLibPath = boost::dll::program_location().parent_path();
      using RegistrationFunction = void(const ptree &block);
      using RegistrationFactory = boost::function<RegistrationFunction>;

      static map<string, RegistrationFactory> factories;

      // Check if already loaded
      if (factories.count(name) > 0)
        return true;

      // expect the dynamic library's location is same as the executable
      // the library's name is sink's id
      auto dllPath = sharedLibPath / name;
      dllPath = boost::dll::detail::shared_library_impl::decorate(dllPath);

      // keep the list of dynamic sink creators
      // the share library will be unloaded if they are out of scope
      RegistrationFactory registration;
      try
      {
        registration = boost::dll::import_alias<RegistrationFunction>(dllPath,  // path to library
                                                                      "register_factory");
      }
      catch (exception &e)
      {
        LOG(info) << "Cannot load plugin " << name << " from " << sharedLibPath
                  << " Reason: " << e.what();
      }

      if (registration.empty())
      {
        // try current working directory
        auto dllPath = boost::filesystem::current_path() / name;
        try
        {
          registration = boost::dll::import_alias<RegistrationFunction>(dllPath,  // path to library
                                                                        "register_factory");
        }
        catch (exception &e)
        {
          LOG(error) << "Cannot load plugin " << name << " from " << sharedLibPath
                     << " Reason: " << e.what();
          return false;
        }
      }

      factories.insert_or_assign(name, registration);

      // Register the plugin
      registration(plugin);
      return true;
    }
  }  // namespace configuration
}  // namespace mtconnect
