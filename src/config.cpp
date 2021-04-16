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

#include "config.hpp"
#include "config_options.hpp"

#include "agent.hpp"
#include "device_model/device.hpp"
#include "options.hpp"
#include "rolling_file_logger.hpp"
#include "version.h"
#include "xml_printer.hpp"
#include <sys/stat.h>

#include <date/date.h>

#include <dlib/config_reader.h>
#include <dlib/logger.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

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
using namespace dlib;
namespace fs = std::filesystem;

namespace mtconnect
{
  static logger g_logger("init.config");

  static inline const char *get_with_default(const config_reader::kernel_1a &reader,
                                             const char *key, const char *defaultValue)
  {
    if (reader.is_key_defined(key))
      return reader[key].c_str();
    else
      return defaultValue;
  }

  static inline int get_with_default(const config_reader::kernel_1a &reader, const char *key,
                                     int defaultValue)
  {
    if (reader.is_key_defined(key))
      return atoi(reader[key].c_str());
    else
      return defaultValue;
  }

  static inline Milliseconds get_with_default(const config_reader::kernel_1a &reader,
                                              const char *key, Milliseconds defaultValue)
  {
    if (reader.is_key_defined(key))
      return Milliseconds{atoi(reader[key].c_str())};
    else
      return defaultValue;
  }

  static inline Seconds get_with_default(const config_reader::kernel_1a &reader, const char *key,
                                         Seconds defaultValue)
  {
    if (reader.is_key_defined(key))
      return Seconds{atoi(reader[key].c_str())};
    else
      return defaultValue;
  }

  static inline const string &get_with_default(const config_reader::kernel_1a &reader,
                                               const char *key, const string &defaultValue)
  {
    if (reader.is_key_defined(key))
      return reader[key];
    else
      return defaultValue;
  }

  static inline bool get_bool_with_default(const config_reader::kernel_1a &reader, const char *key,
                                           bool defaultValue)
  {
    if (reader.is_key_defined(key))
      return reader[key] == "true" || reader[key] == "yes";
    else
      return defaultValue;
  }

  static inline void assign_value(const char *key, const config_reader::kernel_1a &reader,
                                  ConfigOptions &options)
  {
    if (reader.is_key_defined(key))
      options[key] = reader[key];
  }

  static inline void assign_bool_value(const char *key, const config_reader::kernel_1a &reader,
                                       ConfigOptions &options, std::optional<bool> deflt = nullopt)
  {
    if (reader.is_key_defined(key))
      options[key] = reader[key] == "true" || reader[key] == "yes";
    else if (deflt)
      options[key] = *deflt;
  }
  
  istream &operator >>(istream &str, Seconds &value)
  {
    int64_t v{0};
    str >> v;
    value = Seconds(v);
    return str;
  }

  template <typename T>
  static inline void assign_value(const char *key, const config_reader::kernel_1a &reader,
                                           ConfigOptions &options,
                                           std::optional<T> deflt = nullopt)
  {
    if (reader.is_key_defined(key))
    {
      stringstream ss(reader[key]);
      T value;
      ss >> value;
      options[key] = value;
    }
    else if (deflt)
      options[key] = *deflt;
  }

  AgentConfiguration::AgentConfiguration()
  {
    bool success = false;
    char pathSep = '/';

#if _WINDOWS
    char path[MAX_PATH];
    pathSep = '\\';
    success = GetModuleFileName(nullptr, path, MAX_PATH) > 0;
#else
#ifdef __linux__
    char path[PATH_MAX];
    success = readlink("/proc/self/exe", path, PATH_MAX) >= 0;
#else
#ifdef MACOSX
    char path[PATH_MAX];
    uint32_t size = PATH_MAX;
    success = !_NSGetExecutablePath(path, &size);
#else
    char *path = "";
    success = false;
#endif
#endif
#endif

    if (success)
    {
      string ep(path);
      size_t found = ep.rfind(pathSep);

      if (found != std::string::npos)
        ep.erase(found + 1);
      m_exePath = fs::path(ep);
      cout << "Configuration search path: current directory and " << m_exePath << endl;
    }
    m_working = fs::current_path();
  }

  void AgentConfiguration::initialize(int argc, const char *argv[])
  {
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
          g_logger << LINFO << "Cannot find " << m_configFile
                   << " in current directory, searching exe path: " << m_exePath;
          cerr << "Cannot find " << m_configFile
               << " in current directory, searching exe path: " << m_exePath << endl;
          m_configFile = (m_exePath / m_configFile).string();
        }
        else
        {
          g_logger << LFATAL << "Agent failed to load: Cannot find configuration file: '"
                   << m_configFile;
          cerr << "Agent failed to load: Cannot find configuration file: '" << m_configFile
               << std::endl;
          optionList.usage();
        }
      }

      ifstream file(m_configFile.c_str());
      loadConfig(file);
    }
    catch (std::exception &e)
    {
      g_logger << LFATAL << "Agent failed to load: " << e.what();
      cerr << "Agent failed to load: " << e.what() << std::endl;
      optionList.usage();
    }
  }

  AgentConfiguration::~AgentConfiguration() { set_all_logging_output_streams(cout); }

#ifdef _WINDOWS
  static time_t GetFileModificationTime(const string &file)
  {
    FILETIME createTime, accessTime, writeTime = {0, 0};
    auto handle =
        CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
      g_logger << LWARN << "Could not find file: " << file;
      return 0;
    }
    if (!GetFileTime(handle, &createTime, &accessTime, &writeTime))
    {
      g_logger << LWARN << "GetFileTime failed for: " << file;
      writeTime = {0, 0};
    }
    CloseHandle(handle);

    uint64_t windowsFileTime = (writeTime.dwLowDateTime | uint64_t(writeTime.dwHighDateTime) << 32);

    return windowsFileTime / 10000000ull - 11644473600ull;
  }
#else
  static time_t GetFileModificationTime(const string &file)
  {
    struct stat buf = {0};
    if (stat(file.c_str(), &buf) != 0)
    {
      g_logger << LWARN << "Cannot stat file (" << errno << "): " << file;
      perror("Cannot stat file");
      return 0;
    }

    return buf.st_mtime;
  }
#endif

  void AgentConfiguration::monitorThread()
  {
    // shut this off for now.
    return;
    
    time_t devices_at_start = 0, cfg_at_start = 0;

    g_logger << LDEBUG << "Monitoring files: " << m_configFile << " and " << m_devicesFile
             << ", will warm start if they change.";

    if ((cfg_at_start = GetFileModificationTime(m_configFile)) == 0)
    {
      g_logger << LWARN << "Cannot stat config file: " << m_configFile << ", exiting monitor";
      return;
    }
    if ((devices_at_start = GetFileModificationTime(m_devicesFile)) == 0)
    {
      g_logger << LWARN << "Cannot stat devices file: " << m_devicesFile << ", exiting monitor";
      return;
    }

    g_logger << LTRACE << "Configuration start time: " << cfg_at_start;
    g_logger << LTRACE << "Device start time: " << devices_at_start;

    bool changed = false;

    // Check every 10 seconds
    do
    {
      dlib::sleep(10000);

      time_t devices = 0, cfg = 0;
      bool check = true;

      if ((cfg = GetFileModificationTime(m_configFile)) == 0)
      {
        g_logger << LWARN << "Cannot stat config file: " << m_configFile
                 << ", retrying in 10 seconds";
        check = false;
      }

      if ((devices = GetFileModificationTime(m_devicesFile)) == 0)
      {
        g_logger << LWARN << "Cannot stat devices file: " << m_devicesFile
                 << ", retrying in 10 seconds";
        check = false;
      }

      g_logger << LTRACE << "Configuration times: " << cfg_at_start << " -- " << cfg;
      g_logger << LTRACE << "Device times: " << devices_at_start << " -- " << devices;

      // Check if the files have changed.
      if (check && (cfg_at_start != cfg || devices_at_start != devices))
      {
        time_t now = time(nullptr);
        g_logger
            << LWARN
            << "Dected change in configuarion files. Will reload when youngest file is at least "
            << m_minimumConfigReloadAge << " seconds old";
        g_logger << LWARN << "    Devices.xml file modified " << (now - devices) << " seconds ago";
        g_logger << LWARN << "    ...cfg file modified " << (now - cfg) << " seconds ago";

        changed =
            (now - cfg) > m_minimumConfigReloadAge && (now - devices) > m_minimumConfigReloadAge;
      }
    } while (!changed);  // && m_agent->is_running());

    // TODO: Put monitor thread back in place
#if 0
    // Restart agent if changed...
    // stop agent and signal to warm start
    if (m_agent->is_running() && changed)
    {
      g_logger << LWARN
               << "Monitor thread has detected change in configuration files, restarting agent.";

      m_restart = true;
      m_agent->stop();
      delete m_agent;
      m_agent = nullptr;

      g_logger << LWARN << "Monitor agent has completed shutdown, reinitializing agent.";

      // Re initialize
      const char *argv[] = {m_configFile.c_str()};
      initialize(1, argv);
    }
#endif
    g_logger << LDEBUG << "Monitor thread is exiting";
  }

  void AgentConfiguration::start()
  {
    unique_ptr<dlib::thread_function> mon;

    do
    {
      m_restart = false;
      if (m_monitorFiles)
      {
        // Start the file monitor to check for changes to cfg or devices.
        g_logger << LDEBUG << "Waiting for monitor thread to exit to restart agent";
        mon = std::make_unique<dlib::thread_function>(
            make_mfp(*this, &AgentConfiguration::monitorThread));
      }

      m_agent->start();

      if (m_restart && m_monitorFiles)
      {
        // Will destruct and wait to re-initialize.
        g_logger << LDEBUG << "Waiting for monitor thread to exit to restart agent";
        mon.reset(nullptr);
        g_logger << LDEBUG << "Monitor has exited";
      }
    } while (m_restart);
  }

  void AgentConfiguration::stop()
  {
    g_logger << dlib::LINFO << "Agent stopping";
    m_restart = false;
    m_agent->stop();
    g_logger << dlib::LINFO << "Agent Configuration stopped";
  }

  Device *AgentConfiguration::defaultDevice() { return m_agent->defaultDevice(); }

  static std::string timestamp()
  {
    return format(date::floor<std::chrono::microseconds>(std::chrono::system_clock::now()));
  }

  void AgentConfiguration::LoggerHook(const std::string &loggerName, const dlib::log_level &l,
                                      const dlib::uint64 threadId, const char *message)
  {
    stringstream out;
    out << timestamp() << ": " << l.name << " [" << threadId << "] " << loggerName << ": "
        << message;
#ifdef WIN32
    out << "\r\n";
#else
    out << "\n";
#endif
    if (m_loggerFile)
      m_loggerFile->write(out.str().c_str());
    else
      cout << out.str();
  }

  static dlib::log_level string_to_log_level(const std::string &level)
  {
    using namespace std;
    if (level == "LALL" || level == "ALL" || level == "all")
      return LALL;
    else if (level == "LNONE" || level == "NONE" || level == "none")
      return LNONE;
    else if (level == "LTRACE" || level == "TRACE" || level == "trace")
      return LTRACE;
    else if (level == "LDEBUG" || level == "DEBUG" || level == "debug")
      return LDEBUG;
    else if (level == "LINFO" || level == "INFO" || level == "info")
      return LINFO;
    else if (level == "LWARN" || level == "WARN" || level == "warn")
      return LWARN;
    else if (level == "LERROR" || level == "ERROR" || level == "error")
      return LERROR;
    else if (level == "LFATAL" || level == "FATAL" || level == "fatal")
      return LFATAL;

    return LINFO;
  }

  void AgentConfiguration::configureLogger(ConfigReader &reader)
  {
    m_loggerFile.reset();
    if (m_isDebug)
    {
      set_all_logging_output_streams(cout);
      set_all_logging_levels(LDEBUG);

      if (reader.is_block_defined("logger_config"))
      {
        const config_reader::kernel_1a &cr = reader.block("logger_config");

        if (cr.is_key_defined("logging_level"))
        {
          auto level = string_to_log_level(cr["logging_level"]);
          if (level.priority < LDEBUG.priority)
            set_all_logging_levels(level);
        }
      }
    }
    else
    {
      string name("agent.log");
      auto sched = RollingFileLogger::NEVER;
      uint64 maxSize = 10ull * 1024ull * 1024ull;  // 10MB
      int maxIndex = 9;

      if (reader.is_block_defined("logger_config"))
      {
        const auto &cr = reader.block("logger_config");

        if (cr.is_key_defined("logging_level"))
          set_all_logging_levels(string_to_log_level(cr["logging_level"]));
        else
          set_all_logging_levels(LINFO);

        if (cr.is_key_defined("output"))
        {
          string output = cr["output"];
          if (output == "cout")
            set_all_logging_output_streams(cout);
          else if (output == "cerr")
            set_all_logging_output_streams(cerr);
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

        string maxSizeStr = get_with_default(cr, "max_size", "10M");
        stringstream ss(maxSizeStr);
        char mag = '\0';
        ss >> maxSize >> mag;

        switch (mag)
        {
          case 'G':
          case 'g':
            maxSize *= 1024ull;
          case 'M':
          case 'm':
            maxSize *= 1024ull;
          case 'K':
          case 'k':
            maxSize *= 1024ull;
          case 'B':
          case 'b':
          case '\0':
            break;
        }

        maxIndex = get_with_default(cr, "max_index", maxIndex);
        string schedCfg = get_with_default(cr, "schedule", "NEVER");
        if (schedCfg == "DAILY")
          sched = RollingFileLogger::DAILY;
        else if (schedCfg == "WEEKLY")
          sched = RollingFileLogger::WEEKLY;
      }

      m_loggerFile = make_unique<RollingFileLogger>(name, maxIndex, maxSize, sched);
      set_all_logging_output_hooks<AgentConfiguration>(*this, &AgentConfiguration::LoggerHook);
    }
  }

  inline static void trim(std::string &str)
  {
    auto index = str.find_first_not_of(" \r\t");
    if (index != string::npos && index > 0)
      str.erase(0, index);
    index = str.find_last_not_of(" \r\t");
    if (index != string::npos)
      str.erase(index + 1);
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

  void AgentConfiguration::loadConfig(std::istream &file)
  {
    // Now get our configuration
    config_reader::kernel_1a reader(file);

    if (!m_loggerFile)
      configureLogger(reader);

    auto defaultPreserve = get_bool_with_default(reader, configuration::PreserveUUID, true);
    auto port = get_with_default(reader, configuration::Port, 5000);
    string serverIp = get_with_default(reader, configuration::ServerIp, "");
    auto bufferSize = get_with_default(reader, configuration::BufferSize, DEFAULT_SLIDING_BUFFER_EXP);
    auto maxAssets = get_with_default(reader, configuration::MaxAssets, DEFAULT_MAX_ASSETS);
    auto checkpointFrequency = get_with_default(reader, configuration::CheckpointFrequency, 1000);
    auto legacyTimeout = get_with_default(reader, configuration::LegacyTimeout, 600s);
    auto reconnectInterval = get_with_default(reader, configuration::ReconnectInterval, 10000ms);
    auto ignoreTimestamps = get_bool_with_default(reader, configuration::IgnoreTimestamps, false);
    auto conversionRequired = get_bool_with_default(reader, configuration::ConversionRequired, true);
    auto upcaseValue = get_bool_with_default(reader, configuration::UpcaseDataItemValue, true);
    auto filterDuplicates = get_bool_with_default(reader, configuration::FilterDuplicates, false);

    m_monitorFiles = get_bool_with_default(reader, configuration::MonitorConfigFiles, false);
    m_minimumConfigReloadAge = get_with_default(reader, configuration::MinimumConfigReloadAge, 15);
    m_pretty = get_bool_with_default(reader, configuration::Pretty, false);

    m_pidFile = get_with_default(reader, configuration::PidFile, "agent.pid");

    if (reader.is_key_defined(configuration::Devices))
    {
      auto name = reader[configuration::Devices];
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

    m_name = get_with_default(reader, configuration::ServiceName, "MTConnect Agent");

    // Get the HTTP Headers
    ConfigOptions options;
    loadHttpHeaders(reader, options);

    // Check for schema version
    m_version =
        get_with_default(reader, configuration::SchemaVersion,
                         to_string(AGENT_VERSION_MAJOR) + "." + to_string(AGENT_VERSION_MINOR));
    g_logger << LINFO << "Starting agent on port " << port;

    auto server = make_unique<http_server::Server>(port, serverIp, options);
    loadAllowPut(reader, server.get());

    auto cp = make_unique<http_server::FileCache>();

    // Make the Agent
    m_agent = make_unique<Agent>(server, cp, m_devicesFile, bufferSize, maxAssets, m_version,
                                 checkpointFrequency, m_pretty);
    XmlPrinter *xmlPrinter = dynamic_cast<XmlPrinter *>(m_agent->getPrinter("xml"));
    m_agent->setLogStreamData(get_bool_with_default(reader, "LogStreams", false));
    auto cache = m_agent->getFileCache();

    // Make the PipelineContext
    m_pipelineContext = std::make_shared<pipeline::PipelineContext>();
    m_pipelineContext->m_contract = m_agent->makePipelineContract();

    options[configuration::PreserveUUID] = defaultPreserve;
    options[configuration::LegacyTimeout] = legacyTimeout;
    options[configuration::ReconnectInterval] = reconnectInterval;
    options[configuration::IgnoreTimestamps] = ignoreTimestamps;
    options[configuration::ConversionRequired] = conversionRequired;
    options[configuration::UpcaseDataItemValue] = upcaseValue;
    options[configuration::FilterDuplicates] = filterDuplicates;
    assign_value<int>(configuration::ShdrVersion, reader, options, 1);
    assign_bool_value(configuration::SuppressIPAddress, reader, options, false);

    m_agent->initialize(m_pipelineContext, options);

    for (auto device : m_agent->getDevices())
      device->m_preserveUuid = defaultPreserve;

    loadAdapters(reader, options);

    // Files served by the Agent... allows schema files to be served by
    // agent.
    loadFiles(xmlPrinter, reader, cache);

    // Load namespaces, allow for local file system serving as well.
    loadNamespace(reader, "DevicesNamespaces", cache, xmlPrinter, &XmlPrinter::addDevicesNamespace);
    loadNamespace(reader, "StreamsNamespaces", cache, xmlPrinter, &XmlPrinter::addStreamsNamespace);
    loadNamespace(reader, "AssetsNamespaces", cache, xmlPrinter, &XmlPrinter::addAssetsNamespace);
    loadNamespace(reader, "ErrorNamespaces", cache, xmlPrinter, &XmlPrinter::addErrorNamespace);

    loadStyle(reader, "DevicesStyle", cache, xmlPrinter, &XmlPrinter::setDevicesStyle);
    loadStyle(reader, "StreamsStyle", cache, xmlPrinter, &XmlPrinter::setStreamStyle);
    loadStyle(reader, "AssetsStyle", cache, xmlPrinter, &XmlPrinter::setAssetsStyle);
    loadStyle(reader, "ErrorStyle", cache, xmlPrinter, &XmlPrinter::setErrorStyle);

    loadTypes(reader, cache);
    
  }

  void AgentConfiguration::loadAdapters(ConfigReader &reader, const ConfigOptions &options)
  {
    using namespace adapter;
    using namespace pipeline;

    Device *device;
    if (reader.is_block_defined("Adapters"))
    {
      const auto &adapters = reader.block("Adapters");
      std::vector<string> blocks;
      adapters.get_blocks(blocks);

      for (const auto &block : blocks)
      {
        ConfigOptions adapterOptions = options;

        const auto &adapter = adapters.block(block);
        string deviceName;
        if (adapter.is_key_defined(configuration::Device))
          deviceName = adapter[configuration::Device];
        else
          deviceName = block;

        device = m_agent->getDeviceByName(deviceName);

        if (!device)
        {
          g_logger << LWARN << "Cannot locate device name '" << deviceName << "', trying default";
          device = defaultDevice();
          if (device)
          {
            deviceName = device->getName();
            adapterOptions[configuration::Device] = deviceName;
            g_logger << LINFO << "Assigning default device " << deviceName << " to adapter";
          }
        }
        else
        {
          adapterOptions[configuration::Device] = device->getName();
        }
        if (!device)
        {
          g_logger << LWARN << "Cannot locate device name '" << deviceName << "', assuming dynamic";
        }

        assign_value(configuration::UUID, adapter, adapterOptions);
        assign_value(configuration::Manufacturer, adapter, adapterOptions);
        assign_value(configuration::Station, adapter, adapterOptions);
        assign_value(configuration::SerialNumber, adapter, adapterOptions);
        assign_bool_value(configuration::FilterDuplicates, adapter, adapterOptions);
        assign_bool_value(configuration::AutoAvailable, adapter, adapterOptions);
        assign_bool_value(configuration::IgnoreTimestamps, adapter, adapterOptions);
        assign_bool_value(configuration::ConversionRequired, adapter, adapterOptions);
        assign_bool_value(configuration::RealTime, adapter, adapterOptions);
        assign_bool_value(configuration::RelativeTime, adapter, adapterOptions);
        assign_bool_value(configuration::UpcaseDataItemValue, adapter, adapterOptions);
        assign_value<int>(configuration::ShdrVersion, adapter, adapterOptions);
        assign_value<Seconds>(configuration::ReconnectInterval, adapter, adapterOptions);
        assign_value<Seconds>(configuration::LegacyTimeout, adapter, adapterOptions);
        assign_bool_value(configuration::PreserveUUID, adapter, adapterOptions);
        assign_bool_value(configuration::SuppressIPAddress, adapter, adapterOptions);
        device->m_preserveUuid = get<bool>(adapterOptions[configuration::PreserveUUID]);

        const string host = get_with_default(adapter, configuration::Host, (string) "localhost");
        auto port = get_with_default(adapter, configuration::Port, 7878);

        if (adapter.is_key_defined(configuration::AdditionalDevices))
        {
          StringList deviceList;
          istringstream devices(adapter[configuration::AdditionalDevices]);
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

        g_logger << LINFO << "Adding adapter for " << deviceName << " on " << host << ":" << port;

        auto pipeline = make_unique<adapter::AdapterPipeline>(m_pipelineContext);
        auto adp = new Adapter(host, port, adapterOptions, pipeline);
        m_agent->addAdapter(adp, false);
      }
    }
    else if ((device = defaultDevice()))
    {
      ConfigOptions adapterOptions{options};

      auto deviceName = device->getName();
      adapterOptions[configuration::Device] = deviceName;
      g_logger << LINFO << "Adding default adapter for " << device->getName()
               << " on localhost:7878";

      auto pipeline = make_unique<adapter::AdapterPipeline>(m_pipelineContext);
      auto adp = new Adapter("localhost", 7878, adapterOptions, pipeline);
      m_agent->addAdapter(adp, false);
    }
    else
    {
      throw runtime_error("Adapters must be defined if more than one device is present");
    }
  }

  void AgentConfiguration::loadAllowPut(ConfigReader &reader, http_server::Server *server)
  {
    auto putEnabled = get_bool_with_default(reader, configuration::AllowPut, false);
    server->enablePut(putEnabled);

    string putHosts = get_with_default(reader, configuration::AllowPutFrom, "");
    if (!putHosts.empty())
    {
      istringstream toParse(putHosts);
      string putHost;
      do
      {
        getline(toParse, putHost, ',');
        trim(putHost);
        if (!putHost.empty())
        {
          string ip;
          for (auto n = 0; !dlib::hostname_to_ip(putHost, ip, n) && ip == "0.0.0.0"; n++)
          {
            ip = "";
          }
          if (!ip.empty())
          {
            server->enablePut();
            server->allowPutFrom(ip);
          }
        }
      } while (!toParse.eof());
    }
  }

  void AgentConfiguration::loadNamespace(ConfigReader &reader, const char *namespaceType,
                                         http_server::FileCache *cache, XmlPrinter *xmlPrinter,
                                         NamespaceFunction callback)
  {
    // Load namespaces, allow for local file system serving as well.
    if (reader.is_block_defined(namespaceType))
    {
      const auto &namespaces = reader.block(namespaceType);
      std::vector<string> blocks;
      namespaces.get_blocks(blocks);

      for (const auto &block : blocks)
      {
        const auto &ns = namespaces.block(block);
        if (block != "m" && !ns.is_key_defined("Urn"))
        {
          g_logger << LERROR << "Name space must have a Urn: " << block;
        }
        else
        {
          string location, urn;
          if (ns.is_key_defined("Location"))
            location = ns["Location"];
          if (ns.is_key_defined("Urn"))
            urn = ns["Urn"];
          (xmlPrinter->*callback)(urn, location, block);
          if (ns.is_key_defined("Path") && !location.empty())
          {
            auto xns = cache->registerFile(location, ns["Path"], m_version);
            if (!xns)
            {
              auto p = ns["Path"];
              g_logger << LDEBUG << "Cannot register " << urn << " at " << location << " and path "
                       << p;
            }
          }
        }
      }
    }
  }

  void AgentConfiguration::loadFiles(XmlPrinter *xmlPrinter, ConfigReader &reader,
                                     http_server::FileCache *cache)
  {
    if (reader.is_block_defined("Files"))
    {
      const auto &files = reader.block("Files");
      std::vector<string> blocks;
      files.get_blocks(blocks);

      for (const auto &block : blocks)
      {
        const auto &file = files.block(block);
        if (!file.is_key_defined("Location") || !file.is_key_defined("Path"))
        {
          g_logger << LERROR
                   << "Name space must have a Location (uri) or Directory and Path: " << block;
        }
        else
        {
          auto namespaces = cache->registerFiles(file["Location"], file["Path"], m_version);
          for (auto &ns : namespaces)
          {
            if (ns.first.find(configuration::Devices) != string::npos)
            {
              xmlPrinter->addDevicesNamespace(ns.first, ns.second, "m");
            }
            else if (ns.first.find("Streams") != string::npos)
            {
              xmlPrinter->addStreamsNamespace(ns.first, ns.second, "m");
            }
            else if (ns.first.find("Assets") != string::npos)
            {
              xmlPrinter->addAssetsNamespace(ns.first, ns.second, "m");
            }
            else if (ns.first.find("Error") != string::npos)
            {
              xmlPrinter->addErrorNamespace(ns.first, ns.second, "m");
            }
          }
        }
      }
    }
  }

  void AgentConfiguration::loadHttpHeaders(ConfigReader &reader, ConfigOptions &options)
  {
    if (reader.is_block_defined(configuration::HttpHeaders))
    {
      const config_reader::kernel_1a &headers = reader.block(configuration::HttpHeaders);
      StringList fields;
      std::vector<string> keys;
      headers.get_keys(keys);

      for (auto &f : keys)
      {
        fields.emplace_back(f + ": " + headers[f]);
      }
      
      options[configuration::HttpHeaders] = fields;
    }
  }

  void AgentConfiguration::loadStyle(ConfigReader &reader, const char *styleName,
                                     http_server::FileCache *cache, XmlPrinter *xmlPrinter,
                                     StyleFunction styleFunction)
  {
    if (reader.is_block_defined(styleName))
    {
      const config_reader::kernel_1a &doc = reader.block(styleName);
      if (!doc.is_key_defined("Location"))
      {
        g_logger << LERROR << "A style must have a Location: " << styleName;
      }
      else
      {
        string location = doc["Location"];
        (xmlPrinter->*styleFunction)(location);
        if (doc.is_key_defined("Path"))
        {
          cache->registerFile(location, doc["Path"], m_version);
        }
      }
    }
  }

  void AgentConfiguration::loadTypes(ConfigReader &reader, http_server::FileCache *cache)
  {
    if (reader.is_block_defined("MimeTypes"))
    {
      const auto &types = reader.block("MimeTypes");
      std::vector<string> keys;
      types.get_keys(keys);

      for (const auto &key : keys)
      {
        cache->addMimeType(key, types[key]);
      }
    }
  }
}  // namespace mtconnect
