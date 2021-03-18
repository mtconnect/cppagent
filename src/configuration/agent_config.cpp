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

#include "agent.hpp"
#include "configuration/config_options.hpp"
#include "device_model/device.hpp"
#include "option.hpp"
#include "rolling_file_logger.hpp"
#include "version.h"
#include "xml_printer.hpp"
#include <boost/asio.hpp>
#include <sys/stat.h>

#include <date/date.h>

#include <dlib/logger.h>

#include <algorithm>
#include <chrono>
#include <errno.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
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
namespace pt = boost::property_tree;

namespace mtconnect
{
  static logger g_logger("init.config");

  namespace configuration
  {
    static inline auto Convert(const std::string &s, const ConfigOption &def)
    {
      ConfigOption option;
      visit(overloaded {[&option, &s](const std::string &) { option = s; },
                        [&option, &s](const int &) { option = stoi(s); },
                        [&option, &s](const Milliseconds &) { option = Milliseconds {stoi(s)}; },
                        [&option, &s](const Seconds &) { option = Seconds {stoi(s)}; },
                        [&option, &s](const double &) { option = stod(s); },
                        [&option, &s](const bool &) { option = s == "yes" || s == "true"; },
                        [](const auto &) {}},
            def);
      return option;
    }

    static inline void AddOptions(const pt::ptree &tree, ConfigOptions &options,
                                  const ConfigOptions &entries)
    {
      for (auto &e : entries)
      {
        auto val = tree.get_optional<string>(e.first);
        if (val)
          options.insert_or_assign(e.first, Convert(*val, e.second));
      }
    }

    static inline void AddDefaultedOptions(const pt::ptree &tree, ConfigOptions &options,
                                           const ConfigOptions &entries)
    {
      for (auto &e : entries)
      {
        auto val = tree.get_optional<string>(e.first);
        if (val)
          options.insert_or_assign(e.first, Convert(*val, e.second));
        else
          options.insert_or_assign(e.first, e.second);
      }
    }

    static inline void GetOptions(const pt::ptree &tree, ConfigOptions &options,
                                  const ConfigOptions &entries)
    {
      options = entries;
      AddOptions(tree, options, entries);
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
        std::stringstream buffer;
        buffer << file.rdbuf();

        loadConfig(buffer.str());
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
          g_logger << LWARN << "    Devices.xml file modified " << (now - devices)
                   << " seconds ago";
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
      m_context.stop();
      g_logger << dlib::LINFO << "Agent Configuration stopped";
    }

    DevicePtr AgentConfiguration::defaultDevice() { return m_agent->defaultDevice(); }

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

    void AgentConfiguration::configureLogger(const ptree &config)
    {
#if 0
      m_loggerFile.reset();
      if (m_isDebug)
      {
        set_all_logging_output_streams(cout);
        set_all_logging_levels(LDEBUG);
        
        auto logger = config.get_child_optional("logger_config");
        if (logger)
        {
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
        
        auto logger = config.get_child_optional("logger_config");
        if (logger)
        {
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
#endif
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
      // Now get our configuration
      auto config = Parser::parse(file);

      if (!m_loggerFile)
        configureLogger(config);

      ConfigOptions options;
      GetOptions(config, options,
                 {{configuration::PreserveUUID, true},
                  {configuration::Port, 5000},
                  {configuration::ServerIp, string()},
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
                  {configuration::ServiceName, "MTConnect Agent"s},
                  {configuration::SchemaVersion,
                   to_string(AGENT_VERSION_MAJOR) + "."s + to_string(AGENT_VERSION_MINOR)},
                  {configuration::LogStreams, false},
                  {configuration::ShdrVersion, 1},
                  {configuration::WorkerThreads, 1},
                  {configuration::AllowPut, false},
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

      // Get the HTTP Headers
      loadHttpHeaders(config, options);

      // Check for schema version
      m_version = get<string>(options[configuration::SchemaVersion]);
      auto port = get<int>(options[configuration::Port]);
      g_logger << LINFO << "Starting agent on port " << port;

      auto server = make_unique<http_server::Server>(
          port, get<string>(options[configuration::ServerIp]), options);
      loadAllowPut(server.get(), options);

      auto cp = make_unique<http_server::FileCache>();

      // Make the Agent
      m_agent = make_unique<Agent>(server, cp, m_devicesFile,
                                   get<int>(options[configuration::BufferSize]),
                                   get<int>(options[configuration::MaxAssets]), m_version,
                                   get<int>(options[configuration::CheckpointFrequency]),
                                   get<bool>(options[configuration::Pretty]));
      XmlPrinter *xmlPrinter = dynamic_cast<XmlPrinter *>(m_agent->getPrinter("xml"));

      m_agent->setLogStreamData(get<bool>(options[configuration::LogStreams]));
      auto cache = m_agent->getFileCache();

      // Make the PipelineContext
      m_pipelineContext = std::make_shared<pipeline::PipelineContext>();
      m_pipelineContext->m_contract = m_agent->makePipelineContract();

      m_agent->initialize(m_pipelineContext, options);

      if (get<bool>(options[configuration::PreserveUUID]))
      {
        for (auto device : m_agent->getDevices())
          device->setPreserveUuid(get<bool>(options[configuration::PreserveUUID]));
      }

      loadAdapters(config, options);

      // Files served by the Agent... allows schema files to be served by
      // agent.
      loadFiles(xmlPrinter, config, cache);

      // Load namespaces, allow for local file system serving as well.
      loadNamespace(config, "DevicesNamespaces", cache, xmlPrinter,
                    &XmlPrinter::addDevicesNamespace);
      loadNamespace(config, "StreamsNamespaces", cache, xmlPrinter,
                    &XmlPrinter::addStreamsNamespace);
      loadNamespace(config, "AssetsNamespaces", cache, xmlPrinter, &XmlPrinter::addAssetsNamespace);
      loadNamespace(config, "ErrorNamespaces", cache, xmlPrinter, &XmlPrinter::addErrorNamespace);

      loadStyle(config, "DevicesStyle", cache, xmlPrinter, &XmlPrinter::setDevicesStyle);
      loadStyle(config, "StreamsStyle", cache, xmlPrinter, &XmlPrinter::setStreamStyle);
      loadStyle(config, "AssetsStyle", cache, xmlPrinter, &XmlPrinter::setAssetsStyle);
      loadStyle(config, "ErrorStyle", cache, xmlPrinter, &XmlPrinter::setErrorStyle);

      loadTypes(config, cache);
    }

    void AgentConfiguration::loadAdapters(const pt::ptree &config, const ConfigOptions &options)
    {
      using namespace adapter;
      using namespace pipeline;

      DevicePtr device;
      auto adapters = config.get_child_optional("Adapters");
      if (adapters)
      {
        for (const auto &block : *adapters)
        {
          ConfigOptions adapterOptions = options;

          GetOptions(block.second, adapterOptions, options);
          AddOptions(block.second, adapterOptions,
                     {{configuration::UUID, string()},
                      {configuration::Manufacturer, string()},
                      {configuration::Station, string()}});

          AddDefaultedOptions(block.second, adapterOptions,
                              {{configuration::Host, "localhost"s},
                               {configuration::Port, 7878},
                               {configuration::AutoAvailable, false},
                               {configuration::RealTime, false},
                               {configuration::RelativeTime, false}});

          auto deviceName =
              block.second.get_optional<string>(configuration::Device).value_or(block.first);

          device = m_agent->getDeviceByName(deviceName);

          if (!device)
          {
            g_logger << LWARN << "Cannot locate device name '" << deviceName << "', trying default";
            device = defaultDevice();
            if (device)
            {
              deviceName = *device->getComponentName();
              adapterOptions[configuration::Device] = deviceName;
              g_logger << LINFO << "Assigning default device " << deviceName << " to adapter";
            }
          }
          else
          {
            adapterOptions[configuration::Device] = *device->getComponentName();
          }
          if (!device)
          {
            g_logger << LWARN << "Cannot locate device name '" << deviceName
                     << "', assuming dynamic";
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

          g_logger << LINFO << "Adding adapter for " << deviceName << " on "
                   << get<string>(adapterOptions[configuration::Host]) << ":"
                   << get<string>(adapterOptions[configuration::Host]);

          auto pipeline = make_unique<adapter::AdapterPipeline>(m_pipelineContext);
          auto adp =
              new Adapter(m_context, get<string>(adapterOptions[configuration::Host]),
                          get<int>(adapterOptions[configuration::Port]), adapterOptions, pipeline);
          m_agent->addAdapter(adp, false);
        }
      }
      else if ((device = defaultDevice()))
      {
        ConfigOptions adapterOptions {options};

        auto deviceName = *device->getComponentName();
        adapterOptions[configuration::Device] = deviceName;
        g_logger << LINFO << "Adding default adapter for " << device->getName()
                 << " on localhost:7878";

        auto pipeline = make_unique<adapter::AdapterPipeline>(m_pipelineContext);
        auto adp = new Adapter(m_context, "localhost", 7878, adapterOptions, pipeline);
        m_agent->addAdapter(adp, false);
      }
      else
      {
        throw runtime_error("Adapters must be defined if more than one device is present");
      }
    }

    void AgentConfiguration::loadAllowPut(http_server::Server *server, ConfigOptions &options)
    {
      namespace asio = boost::asio;
      namespace ip = asio::ip;

      server->enablePut(get<bool>(options[configuration::AllowPut]));
      string hosts = get<string>(options[configuration::AllowPutFrom]);
      if (!hosts.empty())
      {
        istringstream line(hosts);
        do
        {
          string host;
          getline(line, host, ',');
          host = trim(host);
          if (!host.empty())
          {
            // Check if it is a simple numeric address
            using br = ip::resolver_base;
            boost::system::error_code ec;
            auto addr = ip::make_address(host, ec);
            if (ec)
            {
              asio::io_service ios;
              ip::tcp::resolver resolver(ios);
              ip::tcp::resolver::query query(host, "0", br::v4_mapped);

              auto it = resolver.resolve(query, ec);
              if (ec)
              {
                cout << "Failed to resolve " << host << ": " << ec.message() << endl;
              }
              else
              {
                ip::tcp::resolver::iterator end;
                for (; it != end; it++)
                {
                  const auto &addr = it->endpoint().address();
                  if (!addr.is_multicast() && !addr.is_unspecified())
                  {
                    server->enablePut();
                    server->allowPutFrom(addr.to_string());
                  }
                }
              }
            }
            else
            {
              server->enablePut();
              server->allowPutFrom(addr.to_string());
            }
          }
        } while (!line.eof());
      }
    }

    void AgentConfiguration::loadNamespace(const ptree &tree, const char *namespaceType,
                                           http_server::FileCache *cache, XmlPrinter *xmlPrinter,
                                           NamespaceFunction callback)
    {
      // Load namespaces, allow for local file system serving as well.
      auto ns = tree.get_child_optional(namespaceType);
      if (ns)
      {
        for (const auto &block : *ns)
        {
          auto urn = block.second.get_optional<string>("Urn");
          if (block.first != "m" && !urn)
          {
            g_logger << LERROR << "Name space must have a Urn: " << block.first;
          }
          else
          {
            auto location = block.second.get_optional<string>("Location").value_or("");
            (xmlPrinter->*callback)(urn.value_or(""), location, block.first);
            auto path = block.second.get_optional<string>("Path");
            if (path && !location.empty())
            {
              auto xns = cache->registerFile(location, *path, m_version);
              if (!xns)
              {
                g_logger << LDEBUG << "Cannot register " << urn << " at " << location
                         << " and path " << *path;
              }
            }
          }
        }
      }
    }

    void AgentConfiguration::loadFiles(XmlPrinter *xmlPrinter, const ptree &tree,
                                       http_server::FileCache *cache)
    {
      auto files = tree.get_child_optional("Files");
      if (files)
      {
        for (const auto &file : *files)
        {
          auto location = file.second.get_optional<string>("Location");
          auto path = file.second.get_optional<string>("Path");
          if (!location || !path)
          {
            g_logger << LERROR << "Name space must have a Location (uri) or Directory and Path: "
                     << file.first;
          }
          else
          {
            auto namespaces = cache->registerFiles(*location, *path, m_version);
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

    void AgentConfiguration::loadHttpHeaders(const ptree &tree, ConfigOptions &options)
    {
      auto headers = tree.get_child_optional(configuration::HttpHeaders);
      if (headers)
      {
        list<string> fields;
        for (auto &f : *headers)
        {
          fields.emplace_back(f.first + ": " + f.second.data());
        }

        options[configuration::HttpHeaders] = fields;
      }
    }

    void AgentConfiguration::loadStyle(const ptree &tree, const char *styleName,
                                       http_server::FileCache *cache, XmlPrinter *xmlPrinter,
                                       StyleFunction styleFunction)
    {
      auto style = tree.get_child_optional(styleName);
      if (style)
      {
        auto location = style->get_optional<string>("Location");
        if (!location)
        {
          g_logger << LERROR << "A style must have a Location: " << styleName;
        }
        else
        {
          (xmlPrinter->*styleFunction)(*location);
          auto path = style->get_optional<string>("Path");
          if (path)
          {
            cache->registerFile(*location, *path, m_version);
          }
        }
      }
    }

    void AgentConfiguration::loadTypes(const ptree &tree, http_server::FileCache *cache)
    {
      auto types = tree.get_child_optional("MimeTypes");
      if (types)
      {
        for (const auto &type : *types)
        {
          cache->addMimeType(type.first, type.second.data());
        }
      }
    }
  }  // namespace configuration
}  // namespace mtconnect
