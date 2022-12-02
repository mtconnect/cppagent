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

#include "agent_config.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <boost/dll/import.hpp>
#include <boost/filesystem.hpp>
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

#include "agent.hpp"
#include "configuration/config_options.hpp"
#include "device_model/device.hpp"
#include "printer/xml_printer.hpp"
#include "sink/mqtt_sink/mqtt_service.hpp"
#include "sink/rest_sink/rest_service.hpp"
#include "source/adapter/agent_adapter/agent_adapter.hpp"
#include "source/adapter/mqtt/mqtt_adapter.hpp"
#include "source/adapter/shdr/shdr_adapter.hpp"
#include "version.h"

#ifdef WITH_PYTHON
#include "python/embedded.hpp"
#endif

#ifdef WITH_RUBY
#include "ruby/embedded.hpp"
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
namespace bfs = boost::filesystem;
namespace pt = boost::property_tree;
namespace logr = boost::log;
namespace dll = boost::dll;
namespace asio = boost::asio;

BOOST_LOG_ATTRIBUTE_KEYWORD(named_scope, "Scope", logr::attributes::named_scope::value_type);
BOOST_LOG_ATTRIBUTE_KEYWORD(utc_timestamp, "Timestamp", logr::attributes::utc_clock::value_type);

namespace mtconnect::configuration {
  boost::log::trivial::logger_type *gAgentLogger = nullptr;

  AgentConfiguration::AgentConfiguration()
    : m_context {make_unique<AsyncContext>()}, m_monitorTimer(m_context->getContext())
  {
    NAMED_SCOPE("AgentConfiguration::AgentConfiguration");
    using namespace source;

    bool success = false;

    sink::mqtt_sink::MqttService::registerFactory(m_sinkFactory);
    sink::rest_sink::RestService::registerFactory(m_sinkFactory);
    adapter::shdr::ShdrAdapter::registerFactory(m_sourceFactory);
    adapter::mqtt_adapter::MqttAdapter::registerFactory(m_sourceFactory);
    adapter::agent_adapter::AgentAdapter::registerFactory(m_sourceFactory);

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
      cerr << "Agent failed to load: Cannot find configuration file: '" << configFile << std::endl;
      usage(1);
    }
    catch (std::exception &e)
    {
      cerr << std::endl
           << "Agent failed to load: " << e.what() << " from " << m_configFile << std::endl;
      LOG(fatal) << std::endl << "Agent failed to load: " << e.what() << " from " << m_configFile;
      usage(1);
    }
  }

  AgentConfiguration::~AgentConfiguration()
  {
    stop();

    m_sinkFactory.clear();
    m_sourceFactory.clear();

    m_pipelineContext.reset();
    m_adapterHandler.reset();

    m_agent.reset();

    m_initializers.clear();
#ifdef WITH_RUBY
    m_ruby.reset();
#endif
    m_context.reset();

    if (m_sink)
      m_sink.reset();

    logr::core::get()->remove_all_sinks();
  }

  void AgentConfiguration::monitorFiles(boost::system::error_code ec)
  {
    using namespace std;
    using namespace chrono;
    using namespace chrono_literals;

    if (ec)
    {
      LOG(info) << "Monitor files stopped";
      return;
    }

    NAMED_SCOPE("AgentConfiguration::monitorThread");

    LOG(debug) << "Monitoring files: " << m_configFile << " and " << m_devicesFile
               << ", will warm start if they change.";

    std::error_code fec;
    auto cfgTime = filesystem::last_write_time(m_configFile, fec);
    if (fec)
    {
      LOG(warning) << "Cannot stat config file: " << m_configFile << ", exiting monitor";
      scheduleMonitorTimer();
      return;
    }

    auto devTime = filesystem::last_write_time(m_devicesFile, fec);
    if (fec)
    {
      LOG(warning) << "Cannot stat devices file: " << m_devicesFile << ", exiting monitor";
      scheduleMonitorTimer();
      return;
    }

    if (!m_deviceTime || !m_configTime)
    {
      m_deviceTime.emplace(devTime);
      m_configTime.emplace(cfgTime);

      LOG(debug) << "Setting device and config times";

      scheduleMonitorTimer();
      return;
    }

    if (devTime == *m_deviceTime && cfgTime == *m_configTime)
    {
      scheduleMonitorTimer();
      return;
    }

    // Check if the files have changed.
    auto now = filesystem::file_time_type::clock::now();

    using namespace date;

    LOG(warning)
        << "Detected change in configuration files. Will reload when youngest file is at least "
        << m_monitorDelay.count() << " seconds old";

    if (devTime != *m_deviceTime)
    {
      auto t = bfs::last_write_time(m_devicesFile);
      LOG(warning) << "Dected change in Devices file: " << m_devicesFile;
      LOG(warning) << "... File changed at: " << put_time(localtime(&t), "%F %T");
    }

    if (cfgTime != *m_configTime)
    {
      auto t = bfs::last_write_time(m_devicesFile);
      LOG(warning) << "Dected change in Config file: " << m_configFile;
      LOG(warning) << "... File changed at: " << put_time(localtime(&t), "%F %T");
    }

    auto delta = min(now - cfgTime, now - devTime);
    if (delta < m_monitorDelay)
    {
      LOG(warning) << "... Waiting " << int32_t((m_monitorDelay - delta).count()) << " seconds";
      scheduleMonitorTimer();
    }
    else
    {
      if (cfgTime != *m_configTime)
      {
        LOG(warning) << "Monitor thread has detected change in configuration files.";
        LOG(warning) << ".... Restarting agent: " << m_configFile;

        m_agent->stop();

        m_context->pause(
            [this](AsyncContext &context) {
              m_agent.reset();
              m_configTime.reset();
              m_deviceTime.reset();

              // Re initialize
              boost::program_options::variables_map options;
              boost::program_options::variable_value value(
                  boost::optional<string>(m_configFile.string()), false);
              options.insert(make_pair("config-file"s, value));
              initialize(options);
              m_agent->start();

              if (m_monitorFiles)
              {
                scheduleMonitorTimer();
              }
            },
            true);
      }
      else if (devTime != *m_deviceTime)
      {
        // Handle device changed by delivering the device file to the agent
        LOG(warning) << "Monitor thread has detected change in devices files.";
        LOG(warning) << "... Reloading Devices File: " << m_devicesFile;

        m_context->pause([this](AsyncContext &context) {
          if (!m_agent->reloadDevices(m_devicesFile))
          {
            m_configTime.emplace(m_configTime->min());
            using namespace chrono;
            using namespace chrono_literals;

            using boost::placeholders::_1;

            m_monitorTimer.expires_from_now(100ms);
            m_monitorTimer.async_wait(boost::bind(&AgentConfiguration::monitorFiles, this, _1));
          }
          else
          {
            m_deviceTime.reset();
            scheduleMonitorTimer();
          }
        });
      }
    }

    return;
  }

  void AgentConfiguration::scheduleMonitorTimer()
  {
    using namespace chrono;
    using namespace chrono_literals;

    using boost::placeholders::_1;

    m_monitorTimer.expires_from_now(m_monitorInterval);
    m_monitorTimer.async_wait(boost::bind(&AgentConfiguration::monitorFiles, this, _1));
  }

  void AgentConfiguration::start()
  {
    if (m_monitorFiles)
    {
      // Start the file monitor to check for changes to cfg or devices.
      LOG(debug) << "Waiting for monitor thread to exit to restart agent";

      boost::system::error_code ec;
      AgentConfiguration::monitorFiles(ec);
    }

    m_context->setThreadCount(m_workerThreadCount);
    m_agent->start();
    m_context->start();
  }

  void AgentConfiguration::stop()
  {
    LOG(info) << "Agent stopping";
    m_monitorTimer.cancel();
    m_restart = false;
    if (m_agent)
      m_agent->stop();
    m_context->stop();
    LOG(info) << "Agent Configuration stopped";
  }

  DevicePtr AgentConfiguration::defaultDevice() { return m_agent->defaultDevice(); }

  void AgentConfiguration::setLoggingLevel(const logr::trivial::severity_level level)
  {
    using namespace logr::trivial;
    m_logLevel = level;
    logr::core::get()->set_filter(severity >= level);
  }

  static logr::trivial::severity_level StringToLogLevel(const std::string &level)
  {
    using namespace logr::trivial;
    string_view lev(level.c_str());
    if (lev[0] == 'L' || lev[0] == 'l')
      lev.remove_prefix(1);

    struct compare
    {
      bool operator()(const string_view &s1, const string_view &s2) const
      {
        return boost::ilexicographical_compare(s1, s2);
      }
    };

    static const map<string_view, severity_level, compare> levels = {
        {"ALL", severity_level::trace},       {"NONE", severity_level::fatal},
        {"TRACE", severity_level::trace},     {"DEBUG", severity_level::debug},
        {"INFO", severity_level::info},       {"WARN", severity_level::warning},
        {"WARNING", severity_level::warning}, {"ERROR", severity_level::error},
        {"FATAL", severity_level::fatal}};

    auto res = levels.find(lev);
    if (res == levels.end())
      return severity_level::info;
    else
      return res->second;
  }

  logr::trivial::severity_level AgentConfiguration::setLoggingLevel(const string &level)
  {
    logr::trivial::severity_level l = StringToLogLevel(level);
    setLoggingLevel(l);
    return l;
  }

  void AgentConfiguration::configureLogger(const ptree &config)
  {
    using namespace logr::trivial;
    namespace kw = boost::log::keywords;
    namespace expr = logr::expressions;

    logr::core::get()->remove_all_sinks();
    m_sink.reset();

    //// Add the commonly used attributes; includes TimeStamp, ProcessID and ThreadID and others
    logr::add_common_attributes();
    logr::core::get()->add_global_attribute("Scope", logr::attributes::named_scope());
    logr::core::get()->add_global_attribute(logr::aux::default_attribute_names::thread_id(),
                                            logr::attributes::current_thread_id());
    logr::core::get()->add_global_attribute("Timestamp", logr::attributes::utc_clock());

    ptree empty;
    auto logger = config.get_child_optional("logger_config").value_or(empty);
    setLoggingLevel(severity_level::info);

    static const string defaultFileName {"agent.log"};
    static const string defaultArchivePattern("agent_%Y-%m-%d_%H-%M-%S_%N.log");

    ConfigOptions options;
    AddDefaultedOptions(logger, options,
                        {{"max_size", "10mb"s},
                         {"rotation_size", "2mb"s},
                         {"max_index", 9},
                         {"file_name", defaultFileName},
                         {"archive_pattern", defaultArchivePattern}});
    AddOptions(logger, options,
               {{"output", string()}, {"level", string()}, {"logging_level", string()}});

    auto output = GetOption<string>(options, "output");
    auto level = setLoggingLevel(
        GetOption<string>(options, "level")
            .value_or(GetOption<string>(options, "logging_level").value_or("info"s)));

    gAgentLogger = m_logger = &::boost::log::trivial::logger::get();

    auto formatter =
        expr::stream << expr::format_date_time<boost::posix_time::ptime>("Timestamp",
                                                                         "%Y-%m-%dT%H:%M:%S.%fZ ")
                     << "("
                     << expr::attr<logr::attributes::current_thread_id::value_type>("ThreadID")
                     << ") [" << severity << "] " << named_scope << ": " << expr::smessage;

    if (m_isDebug || (output && (*output == "cout" || *output == "cerr")))
    {
      ostream *out;
      if (output && *output == "cerr")
        out = &std::cerr;
      else
        out = &std::cout;

      logr::add_console_log(*out, kw::format = formatter);

      if (m_isDebug && level >= severity_level::debug)
        setLoggingLevel(severity_level::debug);

      return;
    }

    auto archiveFileName = [](fs::path fileName) -> string {
      return fileName.stem().string() + "_%Y-%m-%d_%H-%M-%S_%N" + fileName.extension().string();
    };

    // Output is backward compatible with old logging format.
    if (output)
    {
      vector<string> parts;
      boost::split(parts, *output, boost::is_space());
      if (parts.size() > 0)
      {
        if (parts[0] == "file" && parts.size() > 1)
          options["file_name"] = parts[1];
        else
          options["file_name"] = parts[0];
        if (parts.size() > 2)
          options["archive_pattern"] = parts[2];
        else
        {
          options["archive_pattern"] = archiveFileName(get<string>(options["file_name"]));
        }
      }
    }

    m_maxLogFileSize = ConvertFileSize(options, "max_size", m_maxLogFileSize);
    m_logRotationSize = ConvertFileSize(options, "rotation_size", m_logRotationSize);
    int max_index = *GetOption<int>(options, "max_index");

    if (auto sched = GetOption<string>(options, "schedule"))
    {
      if (*sched == "DAILY")
        m_rotationLogInterval = 24;
      else if (*sched == "WEEKLY")
        m_rotationLogInterval = 168;
      else if (*sched != "NEVER")
        LOG(error) << "Invalid schedule value.";
    }

    // Get file names and patterns from the options.
    auto file_name = *GetOption<string>(options, "file_name");
    auto archive_pattern = *GetOption<string>(options, "archive_pattern");

    m_logArchivePattern = fs::path(archive_pattern);
    if (!m_logArchivePattern.has_filename())
    {
      m_logArchivePattern =
          m_logArchivePattern / archiveFileName(get<string>(options["file_name"]));
    }

    if (m_logArchivePattern.is_relative())
      m_logArchivePattern = fs::current_path() / m_logArchivePattern;

    // Get the log directory from the archive path.
    m_logDirectory = m_logArchivePattern.parent_path();

    // If the file name does not specify a log directory, use the
    // archive directory
    m_logFileName = fs::path(file_name);
    if (!m_logFileName.has_parent_path())
      m_logFileName = m_logDirectory / m_logFileName;
    else if (m_logFileName.is_relative())
      m_logFileName = fs::current_path() / m_logFileName;

    boost::shared_ptr<logr::core> core = logr::core::get();

    // Create a text file sink
    m_sink = boost::make_shared<text_sink>(
        kw::file_name = m_logFileName, kw::target_file_name = m_logArchivePattern.filename(),
        kw::auto_flush = true, kw::rotation_size = m_logRotationSize,
        kw::open_mode = ios_base::out | ios_base::app, kw::format = formatter);

    // Set up where the rotated files will be stored
    m_sink->locked_backend()->set_file_collector(logr::sinks::file::make_collector(
        kw::target = m_logDirectory, kw::max_size = m_maxLogFileSize, kw::max_files = max_index));

    if (m_rotationLogInterval > 0)
    {
      m_sink->locked_backend()->set_time_based_rotation(
          logr::sinks::file::rotation_at_time_interval(
              boost::posix_time::hours(m_rotationLogInterval)));
    }

    // Upon restart, scan the target directory for files matching the file_name pattern
    m_sink->locked_backend()->scan_for_files();
    m_sink->set_formatter(formatter);

    // Formatter for the logger
    core->add_sink(m_sink);
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
                {configuration::DisableAgentDevice, false},
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
                {configuration::JsonVersion, 2},
                {configuration::UpcaseDataItemValue, true},
                {configuration::FilterDuplicates, false},
                {configuration::MonitorConfigFiles, false},
                {configuration::MonitorInterval, 10s},
                {configuration::VersionDeviceXmlUpdates, false},
                {configuration::MinimumConfigReloadAge, 15s},
                {configuration::Pretty, false},
                {configuration::PidFile, "agent.pid"s},
                {configuration::Port, 5000},
                {configuration::MaxCachedFileSize, "20k"s},
                {configuration::MinCompressFileSize, "100k"s},
                {configuration::ServiceName, "MTConnect Agent"s},
                {configuration::SchemaVersion, ""s},
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
    m_monitorFiles = *GetOption<bool>(options, configuration::MonitorConfigFiles);
    m_monitorInterval = *GetOption<Seconds>(options, configuration::MonitorInterval);
    m_monitorDelay = *GetOption<Seconds>(options, configuration::MinimumConfigReloadAge);

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
    auto port = get<int>(options[configuration::Port]);
    LOG(info) << "Starting agent on port " << int(port);

    // Make the Agent
    m_agent = make_unique<Agent>(getAsyncContext(), m_devicesFile, options);

    // Make the PipelineContext
    m_pipelineContext = std::make_shared<pipeline::PipelineContext>();
    m_pipelineContext->m_contract = m_agent->makePipelineContract();

    loadSinks(config, options);

    m_agent->initialize(m_pipelineContext);
    m_version = *m_agent->getSchemaVersion();

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
#ifdef WITH_RUBY
    configureRuby(config, options);
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
    using namespace source::adapter;
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

        auto blockOptions = block.second;
        if (!blockOptions.get_child_optional("logger_config"))
        {
          auto logger = config.get_child_optional("logger_config");
          if (logger)
            blockOptions.add_child("logger_config", *logger);
        }

        auto source = m_sourceFactory.make(factory, name, getAsyncContext(), m_pipelineContext,
                                           adapterOptions, blockOptions);

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

      auto source = m_sourceFactory.make("shdr", "default", getAsyncContext(), m_pipelineContext,
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

#ifdef WITH_RUBY
  void AgentConfiguration::configureRuby(const ptree &tree, ConfigOptions &options)
  {
    ConfigOptions rubyOptions = options;

    auto ruby = tree.get_child_optional("Ruby");
    if (ruby)
    {
      GetOptions(*ruby, rubyOptions, options);
      AddOptions(*ruby, rubyOptions, {{"module", string()}, {"initialization", string()}});
    }
    m_ruby = make_unique<ruby::Embedded>(m_agent.get(), rubyOptions);
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

        ptree sinkBlockOptions = sinkBlock.second;
        if (!sinkBlockOptions.get_child_optional("logger_config"))
        {
          auto logger = config.get_child_optional("logger_config");
          if (logger)
            sinkBlockOptions.add_child("logger_config", *logger);
        }

        auto sinkName = GetOption<string>(sinkOptions, "Name").value_or(name);
        auto sinkContract = m_agent->makeSinkContract();
        sinkContract->m_pipelineContext = m_pipelineContext;

        auto sink = m_sinkFactory.make(factory, sinkName, getAsyncContext(),
                                       std::move(sinkContract), options, sinkBlockOptions);
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

      auto sink = m_sinkFactory.make("RestService", "RestService", getAsyncContext(),
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
    list<fs::path> paths {sharedLibPath / name, fs::current_path() / name};

    for (auto path : paths)
    {
      try
      {
        InitializationFunction init = dll::import_alias<InitializationFn>(
            dll::detail::shared_library_impl::decorate(path), "initialize_plugin");

        // Remember this initializer so it does not get unloaded.
        m_initializers.insert_or_assign(name, init);

        // Register the plugin
        NAMED_SCOPE("initialize_plugin");
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
}  // namespace mtconnect::configuration
