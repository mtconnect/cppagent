//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include "agent.hpp"
#include "options.hpp"
#include "device.hpp"
#include "xml_printer.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <dlib/config_reader.h>
#include <dlib/logger.h>
#include <stdexcept>
#include <algorithm>
#include <sys/stat.h>
#include "rolling_file_logger.hpp"
#include <date/date.h>

// If Windows XP
#if defined(_WINDOWS)
#if WINVER < 0x0600
#include "shlwapi.h"
#define stat(P, B) (PathFileExists((const char*) P) ? 0 : -1)
#endif
#endif

#ifdef MACOSX
#include <mach-o/dyld.h>
#endif

using namespace std;
using namespace dlib;

namespace mtconnect {
  static logger g_logger("init.config");
  
  static inline const char *get_with_default(
                                             const config_reader::kernel_1a &reader, 
                                             const char *key,
                                             const char *defaultValue)
  {
    if (reader.is_key_defined(key))
      return reader[key].c_str();
    else
      return defaultValue;
  }
  
  
  static inline int get_with_default(
                                     const config_reader::kernel_1a &reader, 
                                     const char *key,
                                     int defaultValue)
  {
    if (reader.is_key_defined(key))
      return atoi(reader[key].c_str());
    else
      return defaultValue;
  }
  
  
  static inline std::chrono::milliseconds get_with_default(
                                                           const config_reader::kernel_1a &reader, 
                                                           const char *key,
                                                           std::chrono::milliseconds defaultValue)
  {
    if (reader.is_key_defined(key))
      return std::chrono::milliseconds{ atoi(reader[key].c_str()) };
    else
      return defaultValue;
  }
  
  
  static inline std::chrono::seconds get_with_default(
                                                      const config_reader::kernel_1a &reader, 
                                                      const char *key,
                                                      std::chrono::seconds defaultValue)
  {
    if (reader.is_key_defined(key))
      return std::chrono::seconds{ atoi(reader[key].c_str()) };
    else
      return defaultValue;
  }
  
  
  static inline const string &get_with_default(
                                               const config_reader::kernel_1a &reader, 
                                               const char *key,
                                               const string &defaultValue)
  {
    if (reader.is_key_defined(key))
      return reader[key];
    else
      return defaultValue;
  }
  
  
  static inline bool get_bool_with_default(
                                           const config_reader::kernel_1a &reader, 
                                           const char *key,
                                           bool defaultValue)
  {
    if (reader.is_key_defined(key))
      return reader[key] == "true" || reader[key] == "yes";
    else
      return defaultValue;
  }
  
  
  AgentConfiguration::AgentConfiguration() :
  m_agent(nullptr),
  m_loggerFile(nullptr),
  m_monitorFiles(false),
  m_minimumConfigReloadAge(15),
  m_restart(false),
  m_exePath("")
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
      m_exePath = path;
      size_t found = m_exePath.rfind(pathSep);
      
      if (found != std::string::npos)
        m_exePath.erase(found + 1);
      
      cout << "Configuration search path: current directory and " << m_exePath;
    }
    else
      m_exePath = "";
  }
  
  void AgentConfiguration::initialize(int argc, const char *argv[])
  {
    MTConnectService::initialize(argc, argv);
    
    const char *configFile = "agent.cfg";
    
    OptionsList optionList;
    optionList.append(new Option(0, configFile, "The configuration file", "file", false));
    optionList.parse(argc, (const char**) argv);
    
    m_configFile = configFile;
    
    try
    {
      struct stat buf = {0};
      
      // Check first if the file is in the current working directory...
      if (stat(m_configFile.c_str(), &buf))
      {
        if (!m_exePath.empty())
        {
          g_logger << LINFO << "Cannot find " << m_configFile << " in current directory, searching exe path: "
          << m_exePath;
          cerr << "Cannot find " << m_configFile << " in current directory, searching exe path: " <<
          m_exePath << endl;
          m_configFile = m_exePath + m_configFile;
        }
        else
        {
          g_logger << LFATAL << "Agent failed to load: Cannot find configuration file: '" << m_configFile;
          cerr << "Agent failed to load: Cannot find configuration file: '" << m_configFile << std::endl;
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
  
  
  AgentConfiguration::~AgentConfiguration()
  {
    if (m_agent)
    {
      delete m_agent;
      m_agent = nullptr;
    }
    if (m_loggerFile)
    {
      delete m_loggerFile;
      m_loggerFile = nullptr;
    }
    set_all_logging_output_streams(cout);
  }
  
  
  void AgentConfiguration::monitorThread()
  {
    struct stat devices_at_start = {0}, cfg_at_start = {0};
    
    if (stat(m_configFile.c_str(), &cfg_at_start))
      g_logger << LWARN << "Cannot stat config file: " << m_configFile << ", exiting monitor";
    if (stat(m_devicesFile.c_str(), &devices_at_start))
      g_logger << LWARN << "Cannot stat devices file: " << m_devicesFile << ", exiting monitor";
    
    g_logger << LDEBUG << "Monitoring files: " << m_configFile << " and " << m_devicesFile <<
    ", will warm start if they change.";
    
    bool changed = false;
    
    // Check every 10 seconds
    do
    {
      this_thread::sleep_for(10s);
      
      struct stat devices = {0}, cfg = {0};
      bool check = true;
      
      if (stat(m_configFile.c_str(), &cfg))
      {
        g_logger << LWARN << "Cannot stat config file: " << m_configFile << ", retrying in 10 seconds";
        check = false;
      }
      
      if (stat(m_devicesFile.c_str(), &devices))
      {
        g_logger << LWARN << "Cannot stat devices file: " << m_devicesFile << ", retrying in 10 seconds";
        check = false;
      }
      
      // Check if the files have changed.
      if (check &&
          (cfg_at_start.st_mtime != cfg.st_mtime || devices_at_start.st_mtime != devices.st_mtime) )
      {
        auto now = time(nullptr);
        g_logger << LWARN <<
        "Dected change in configuarion files. Will reload when youngest file is at least " <<
        m_minimumConfigReloadAge
        << " seconds old";
        g_logger << LWARN << "    Devices.xml file modified " << (now - devices.st_mtime) << " seconds ago";
        g_logger << LWARN << "    ...cfg file modified " << (now - cfg.st_mtime) << " seconds ago";
        
        changed = (now - cfg.st_mtime) > m_minimumConfigReloadAge &&
        (now - devices.st_mtime) > m_minimumConfigReloadAge;
      }
    } while (!changed && m_agent->is_running());
    
    // Restart agent if changed...
    // stop agent and signal to warm start
    if (m_agent->is_running() && changed)
    {
      g_logger << LWARN << "Monitor thread has detected change in configuration files, restarting agent.";
      
      m_restart = true;
      m_agent->clear();
      delete m_agent;
      m_agent = nullptr;
      
      g_logger << LWARN << "Monitor agent has completed shutdown, reinitializing agent.";
      
      // Re initialize
      const char *argv[] = { m_configFile.c_str() };
      initialize(1, argv);
    }
    
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
        mon.reset(new dlib::thread_function(make_mfp(*this, &AgentConfiguration::monitorThread)));
      }
      
      m_agent->start();
      
      if (m_restart && m_monitorFiles)
      {
        // Will destruct and wait to re-initialize.
        g_logger << LDEBUG << "Waiting for monitor thread to exit to restart agent";
        mon.reset(0);
        g_logger << LDEBUG << "Monitor has exited";
      }
    } while (m_restart);
  }
  
  
  void AgentConfiguration::stop()
  {
    m_agent->clear();
  }
  
  
  Device *AgentConfiguration::defaultDevice()
  {
    const auto &devices = m_agent->getDevices();
    if (devices.size() == 1)
      return devices[0];
    else
      return nullptr;
  }
  
  
  static std::string timestamp()
  {
    return date::format(
                        "%Y-%m-%dT%H:%M:%SZ",
                        date::floor<std::chrono::microseconds>(std::chrono::system_clock::now()));
  }
  
  
  void AgentConfiguration::LoggerHook(
                                      const std::string& loggerName,
                                      const dlib::log_level& l,
                                      const dlib::uint64 threadId,
                                      const char* message)
  {
    stringstream out;
    out << timestamp() << ": " << l.name << " [" << threadId << "] " << loggerName << ": " << message;
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
  
  
  static dlib::log_level string_to_log_level (const std::string& level)
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
  
  
  void AgentConfiguration::configureLogger(dlib::config_reader::kernel_1a &reader)
  {
    if (m_loggerFile)
    {
      delete m_loggerFile;
      m_loggerFile = nullptr;
    }
    
    if (m_isDebug)
    {
      set_all_logging_output_streams(cout);
      set_all_logging_levels(LDEBUG);
    }
    else
    {
      string name("agent.log");
      auto sched = RollingFileLogger::NEVER;
      uint64 maxSize = 10ull * 1024ull * 1024ull; // 10MB
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
            if (one == "file" && !three.size())
              name = two;
            else
              name = one;
          }
        }
        
        string maxSizeStr = get_with_default(cr, "max_size", "10M");
        stringstream ss(maxSizeStr);
        char mag = '\0';
        ss >> maxSize >> mag;
        
        switch(mag)
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
      
      m_loggerFile = new RollingFileLogger(name, maxIndex, maxSize, sched);
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
  
  
  void AgentConfiguration::loadConfig(std::istream &file)
  {
    // Now get our configuration
    config_reader::kernel_1a reader(file);
    
    if (!m_loggerFile)
      configureLogger(reader);
    
    auto defaultPreserve = get_bool_with_default(reader, "PreserveUUID", true);
    auto port = get_with_default(reader, "Port", 5000);
    string serverIp = get_with_default(reader, "ServerIp", "");
    auto bufferSize = get_with_default(reader, "BufferSize", DEFAULT_SLIDING_BUFFER_EXP);
    auto maxAssets = get_with_default(reader, "MaxAssets", DEFAULT_MAX_ASSETS);
    auto checkpointFrequency = get_with_default(reader, "CheckpointFrequency", 1000ms);
    auto legacyTimeout = get_with_default(reader, "LegacyTimeout", 600s);
    auto reconnectInterval = get_with_default(reader, "ReconnectInterval", 10000ms);
    auto ignoreTimestamps = get_bool_with_default(reader, "IgnoreTimestamps", false);
    auto conversionRequired = get_bool_with_default(reader, "ConversionRequired", true);
    auto upcaseValue = get_bool_with_default(reader, "UpcaseDataItemValue", true);
    auto filterDuplicates = get_bool_with_default(reader, "FilterDuplicates", false);

    m_monitorFiles = get_bool_with_default(reader, "MonitorConfigFiles", false);
    m_minimumConfigReloadAge = get_with_default(reader, "MinimumConfigReloadAge", 15);
    m_pretty = get_bool_with_default(reader, "Pretty", false);
    
    m_pidFile = get_with_default(reader, "PidFile", "agent.pid");
    std::vector<string> devices_files;
    
    if (reader.is_key_defined("Devices"))
    {
      auto fileName = reader["Devices"];
      devices_files.push_back(fileName);
      
      if (!m_exePath.empty() &&
          fileName[0] != '/' &&
          fileName[0] != '\\' &&
          fileName[1] != ':')
      {
        devices_files.push_back(m_exePath + reader["Devices"]);
      }
    }
    
    devices_files.push_back("Devices.xml");
    
    if (!m_exePath.empty())
      devices_files.push_back(m_exePath + "Devices.xml");
    
    devices_files.push_back("probe.xml");
    
    if (!m_exePath.empty())
      devices_files.push_back(m_exePath + "probe.xml");
    
    m_devicesFile.clear();
    
    for (const auto &probe : devices_files)
    {
      struct stat buf = {0};
      g_logger << LDEBUG << "Checking for Devices XML configuration file: " << probe;
      auto res = stat(probe.c_str(), &buf);
      g_logger << LDEBUG << "  Stat returned: " << res;
      
      if (!res)
      {
        m_devicesFile = probe;
        break;
      }
      
      g_logger << LDEBUG << "Could not find file: " << probe;
      cout << "Could not locate file: " << probe << endl;
    }
    
    if (m_devicesFile.empty())
    {
      throw runtime_error(((string) "Please make sure the configuration "
                           "file probe.xml or Devices.xml is in the current "
                           "directory or specify the correct file "
                           "in the configuration file " + m_configFile +
                           " using Devices = <file>").c_str());
    }
    
    m_name = get_with_default(reader, "ServiceName", "MTConnect Agent");
    
    // Check for schema version
    string schemaVersion = get_with_default(reader, "SchemaVersion", "");    
    g_logger << LINFO << "Starting agent on port " << port;
    
    if (!m_agent)
      m_agent = new Agent(m_devicesFile, bufferSize, maxAssets,
                          schemaVersion, checkpointFrequency,
                          m_pretty);
    XmlPrinter *xmlPrinter = dynamic_cast<XmlPrinter*>(m_agent->getPrinter("xml"));
    
    m_agent->set_listening_port(port);
    m_agent->set_listening_ip(serverIp);
    m_agent->setLogStreamData(get_bool_with_default(reader, "LogStreams", false));
    
    for (size_t i = 0; i < m_agent->getDevices().size(); i++)
      m_agent->getDevices()[i]->m_preserveUuid = defaultPreserve;
        
    loadAllowPut(reader);
    loadAdapters(
                 reader,
                 defaultPreserve,
                 legacyTimeout,
                 reconnectInterval,
                 ignoreTimestamps,
                 conversionRequired,
                 upcaseValue,
                 filterDuplicates);
    
    // Files served by the Agent... allows schema files to be served by
    // agent.
    loadFiles(reader);
    
    // Load namespaces, allow for local file system serving as well.
    loadNamespace(reader, "DevicesNamespaces", xmlPrinter,  &XmlPrinter::addDevicesNamespace);
    loadNamespace(reader, "StreamsNamespaces", xmlPrinter,  &XmlPrinter::addStreamsNamespace);
    loadNamespace(reader, "AssetsNamespaces", xmlPrinter,  &XmlPrinter::addAssetsNamespace);
    loadNamespace(reader, "ErrorNamespaces", xmlPrinter,  &XmlPrinter::addErrorNamespace);
    
    loadStyle(reader, "DevicesStyle", xmlPrinter,  &XmlPrinter::setDevicesStyle);
    loadStyle(reader, "StreamsStyle", xmlPrinter, &XmlPrinter::setStreamStyle);
    loadStyle(reader, "AssetsStyle", xmlPrinter, &XmlPrinter::setAssetsStyle);
    loadStyle(reader, "ErrorStyle", xmlPrinter, &XmlPrinter::setErrorStyle);
    
    loadTypes(reader);
  }
  
  
  void AgentConfiguration::loadAdapters(
                                        dlib::config_reader::kernel_1a &reader,
                                        bool defaultPreserve,
                                        std::chrono::seconds legacyTimeout,
                                        std::chrono::milliseconds reconnectInterval,
                                        bool ignoreTimestamps,
                                        bool conversionRequired,
                                        bool upcaseValue,
                                        bool filterDuplicates)
  {
    Device *device;
    if (reader.is_block_defined("Adapters"))
    {
      const auto &adapters = reader.block("Adapters");
      std::vector<string> blocks;
      adapters.get_blocks(blocks);
      
      for (const auto &block : blocks)
      {
        const auto &adapter = adapters.block(block);
        string deviceName;
        if (adapter.is_key_defined("Device"))
          deviceName = adapter["Device"].c_str();
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
            g_logger << LINFO << "Assigning default device " << deviceName << " to adapter";
          }
        }
        if (!device)
        {
          g_logger << LWARN << "Cannot locate device name '" << deviceName << "', assuming dynamic";
        }
        
        const string host = get_with_default(adapter, "Host", (string)"localhost");
        auto port = get_with_default(adapter, "Port", 7878);
        
        
        g_logger << LINFO << "Adding adapter for " << deviceName << " on "
        << host << ":" << port;
        auto adp = m_agent->addAdapter(
                                       deviceName,
                                       host,
                                       port,
                                       false,
                                       get_with_default(adapter, "LegacyTimeout", legacyTimeout) );
        device->m_preserveUuid = get_bool_with_default(adapter, "PreserveUUID", defaultPreserve);
        
        // Add additional device information
        if (adapter.is_key_defined("UUID"))
          device->setUuid(adapter["UUID"]);
        if (adapter.is_key_defined("Manufacturer"))
          device->setManufacturer(adapter["Manufacturer"]);
        if (adapter.is_key_defined("Station"))
          device->setStation(adapter["Station"]);
        if (adapter.is_key_defined("SerialNumber"))
          device->setSerialNumber(adapter["SerialNumber"]);
        
        adp->setDupCheck(get_bool_with_default(adapter, "FilterDuplicates", filterDuplicates));
        adp->setAutoAvailable(get_bool_with_default(adapter, "AutoAvailable", adp->isAutoAvailable()));
        adp->setIgnoreTimestamps(get_bool_with_default(adapter, "IgnoreTimestamps", ignoreTimestamps ||
                                                       adp->isIgnoringTimestamps()));
        adp->setConversionRequired(get_bool_with_default(adapter, "ConversionRequired", conversionRequired));
        adp->setRealTime(get_bool_with_default(adapter, "RealTime", false));
        adp->setRelativeTime(get_bool_with_default(adapter, "RelativeTime", false));
        adp->setReconnectInterval(get_with_default(adapter, "ReconnectInterval", reconnectInterval));
        adp->setUpcaseValue(get_bool_with_default(adapter, "UpcaseDataItemValue", upcaseValue));
        
        if (adapter.is_key_defined("AdditionalDevices"))
        {
          istringstream devices(adapter["AdditionalDevices"]);
          string name;
          while (getline(devices, name, ','))
          {
            auto index = name.find_first_not_of(" \r\t");
            if (index != string::npos && index > 0)
              name.erase(0, index);
            index = name.find_last_not_of(" \r\t");
            if (index != string::npos)
              name.erase(index + 1);
            
            adp->addDevice(name);
          }
        }
      }
    }
    else if ((device = defaultDevice()))
    {
      g_logger << LINFO << "Adding default adapter for " << device->getName() << " on localhost:7878";
      auto adp = m_agent->addAdapter(device->getName(), "localhost", 7878, false, legacyTimeout);
      adp->setIgnoreTimestamps(ignoreTimestamps || adp->isIgnoringTimestamps());
      adp->setReconnectInterval(reconnectInterval);
      device->m_preserveUuid = defaultPreserve;
    }
    else
    {
      throw runtime_error("Adapters must be defined if more than one device is present");
    }
  }
  
  
  void AgentConfiguration::loadAllowPut(dlib::config_reader::kernel_1a &reader)
  {
    auto putEnabled = get_bool_with_default(reader, "AllowPut", false);
    m_agent->enablePut(putEnabled);
    
    string putHosts = get_with_default(reader, "AllowPutFrom", "");
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
            ip = "";
          if (!ip.empty())
          {
            m_agent->enablePut();
            m_agent->allowPutFrom(ip);
          }
        }
      } while (!toParse.eof());
    }
  }
  
  
  void AgentConfiguration::loadNamespace(
                                         dlib::config_reader::kernel_1a &reader,
                                         const char *namespaceType,
                                         XmlPrinter *xmlPrinter,
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
            m_agent->registerFile(location, ns["Path"]);
        }
      }
    }
  }
  
  
  void AgentConfiguration::loadFiles(dlib::config_reader::kernel_1a &reader)
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
          g_logger << LERROR << "Name space must have a Location (uri) or Directory and Path: " << block;
        else
          m_agent->registerFile(file["Location"], file["Path"]);
      }
    }
  }
  
  
  void AgentConfiguration::loadStyle(dlib::config_reader::kernel_1a &reader, const char *styleName, XmlPrinter *xmlPrinter, StyleFunction styleFunction)
  {
    if (reader.is_block_defined(styleName))
    {
      const config_reader::kernel_1a &doc = reader.block(styleName);
      if (!doc.is_key_defined("Location"))
        g_logger << LERROR << "A style must have a Location: " << styleName;
      else
      {
        string location = doc["Location"];
        (xmlPrinter->*styleFunction)(location);
        if (doc.is_key_defined("Path"))
          m_agent->registerFile(location, doc["Path"]);
      }
    }
  }
  
  
  void AgentConfiguration::loadTypes(dlib::config_reader::kernel_1a &reader)
  {
    if (reader.is_block_defined("MimeTypes"))
    {
      const auto &types = reader.block("MimeTypes");
      std::vector<string> keys;
      types.get_keys(keys);
      
      for (const auto &key : keys)
        m_agent->addMimeType(key, types[key]);
    }
  }
}
