//
// Copyright Copyright 2012, System Insights, Inc.
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
#include <dlib/config_reader.h>
#include <dlib/logger.h>
#include <stdexcept>
#include <algorithm>
#include <sys/stat.h>
#include "rolling_file_logger.hpp"

// If Windows XP
#if defined(_WINDOWS) && (WINVER < 0x0600)
#include "shlwapi.h"
#define stat(P, B) (PathFileExists((const char*) P) ? 0 : -1)
#endif

#ifdef MACOSX
#include <mach-o/dyld.h>
#endif

using namespace std;
using namespace dlib;

static logger sLogger("init.config");

static inline const char *get_with_default(const config_reader::kernel_1a &reader, 
      const char *aKey, const char *aDefault) {
  if (reader.is_key_defined(aKey))
    return reader[aKey].c_str();
  else
    return aDefault;
}

static inline int get_with_default(const config_reader::kernel_1a &reader, 
      const char *aKey, int aDefault) {
  if (reader.is_key_defined(aKey))
    return atoi(reader[aKey].c_str());
  else
    return aDefault;
}

static inline const string &get_with_default(const config_reader::kernel_1a &reader, 
      const char *aKey, const string &aDefault) {
  if (reader.is_key_defined(aKey))
    return reader[aKey];
  else
    return aDefault;
}

static inline bool get_bool_with_default(const config_reader::kernel_1a &reader, 
      const char *aKey, bool aDefault) {
  if (reader.is_key_defined(aKey))
    return reader[aKey] == "true" || reader[aKey] == "yes";
  else
    return aDefault;
}

AgentConfiguration::AgentConfiguration() :
  mAgent(NULL), mLoggerFile(NULL), mMonitorFiles(false),
  mMinimumConfigReloadAge(15), mRestart(false), mExePath("")
{
  bool success = false;
  char pathSep = '/';
  
#if _WINDOWS
  char path[MAX_PATH];
  pathSep = '\\';
  success = GetModuleFileName(NULL, path, MAX_PATH) > 0;
#else
#ifdef __linux__
  char path[PATH_MAX];
  success = readlink("/proc/self/exe", path, PATH_MAX) >= 0;
#else
#ifdef MACOSX
  char path[PATH_MAX];
  uint32_t size = PATH_MAX;
  success = _NSGetExecutablePath(path, &size) == 0;
#else
  char *path = "";
  success = false;
#endif
#endif
#endif
  

  if (success)
  {
    mExePath = path;
    size_t found = mExePath.rfind(pathSep);
    if (found != std::string::npos)
    {
      mExePath.erase(found + 1);
    }
    
    cout << "Configuration search path: current directory and " << mExePath;
  } else {
    mExePath = "";
  }
}

void AgentConfiguration::initialize(int aArgc, const char *aArgv[])
{
  MTConnectService::initialize(aArgc, aArgv);
  
  const char *configFile = "agent.cfg";

  OptionsList optionList;
  optionList.append(new Option(0, configFile, "The configuration file", "file", false));  
  optionList.parse(aArgc, (const char**) aArgv);
  
  mConfigFile = configFile;

  try
  {
    struct stat buf;
    
    // Check first if the file is in the current working directory...
    if (stat(mConfigFile.c_str(), &buf) != 0) {
      if (!mExePath.empty())
      {
        sLogger << LINFO << "Cannot find " << mConfigFile << " in current directory, searching exe path: " << mExePath;
        cerr << "Cannot find " << mConfigFile << " in current directory, searching exe path: " << mExePath << endl;
        mConfigFile = mExePath + mConfigFile;
      }
      else
      {
        sLogger << LFATAL << "Agent failed to load: Cannot find configuration file: '" << mConfigFile;
        cerr << "Agent failed to load: Cannot find configuration file: '" << mConfigFile << std::endl;
        optionList.usage();
      }
    }
    
    ifstream file(mConfigFile.c_str());
    loadConfig(file);
  }
  catch (std::exception & e)
  {
    sLogger << LFATAL << "Agent failed to load: " << e.what();
    cerr << "Agent failed to load: " << e.what() << std::endl;
    optionList.usage();
  }
}

AgentConfiguration::~AgentConfiguration()
{
  if (mAgent != NULL)
    delete mAgent;
  if (mLoggerFile != NULL)
    delete mLoggerFile;
  set_all_logging_output_streams(cout);
}

void AgentConfiguration::monitorThread()
{
  struct stat devices_at_start, cfg_at_start;
  
  if (stat(mConfigFile.c_str(), &cfg_at_start) != 0)
    sLogger << LWARN << "Cannot stat config file: " << mConfigFile << ", exiting monitor";
  if (stat(mDevicesFile.c_str(), &devices_at_start) != 0)
    sLogger << LWARN << "Cannot stat devices file: " << mDevicesFile << ", exiting monitor";
  
  sLogger << LDEBUG << "Monitoring files: " << mConfigFile << " and " << mDevicesFile <<
    ", will warm start if they change.";
  
  bool changed = false;
  
  // Check every 10 seconds
  do {
    dlib::sleep(10000);
    
    struct stat devices, cfg;
    bool check = true;
    
    if (stat(mConfigFile.c_str(), &cfg) != 0) {
      sLogger << LWARN << "Cannot stat config file: " << mConfigFile << ", retrying in 10 seconds";
      check = false;
    }

    if (stat(mDevicesFile.c_str(), &devices) != 0) {
      sLogger << LWARN << "Cannot stat devices file: " << mDevicesFile << ", retrying in 10 seconds";
      check = false;
    }
    
    // Check if the files have changed.
    if (check && (cfg_at_start.st_mtime != cfg.st_mtime || devices_at_start.st_mtime != devices.st_mtime)) {
      time_t now = time(NULL);
      sLogger << LWARN << "Dected change in configuarion files. Will reload when youngest file is at least " << mMinimumConfigReloadAge
                       <<" seconds old";
      sLogger << LWARN << "    Devices.xml file modified " << (now - devices.st_mtime) << " seconds ago";
      sLogger << LWARN << "    ...cfg file modified " << (now - cfg.st_mtime) << " seconds ago";
      
      changed = (now - cfg.st_mtime) > mMinimumConfigReloadAge && (now - devices.st_mtime) > mMinimumConfigReloadAge;
    }
  } while (!changed && mAgent->is_running());

  
  // Restart agent if changed...
  // stop agent and signal to warm start
  if (mAgent->is_running() && changed)
  {
    sLogger << LWARN << "Monitor thread has detected change in configuration files, restarting agent.";

    mRestart = true;
    mAgent->clear();
    delete mAgent;
    mAgent = NULL;

    sLogger << LWARN << "Monitor agent has completed shutdown, reinitializing agent.";
    
    // Re initialize
    const char *argv[] = { mConfigFile.c_str() };
    initialize(1, argv);
  }

  sLogger << LDEBUG << "Monitor thread is exiting";
}

void AgentConfiguration::start()
{
  unique_ptr<dlib::thread_function> mon;
  
  do
  {
    mRestart = false;
    if (mMonitorFiles)
    {
      // Start the file monitor to check for changes to cfg or devices.
      sLogger << LDEBUG << "Waiting for monitor thread to exit to restart agent";
      mon.reset(new dlib::thread_function(make_mfp(*this, &AgentConfiguration::monitorThread)));
    }

    mAgent->start();
    
    if (mRestart && mMonitorFiles)
    {
      // Will destruct and wait to re-initialize.
      sLogger << LDEBUG << "Waiting for monitor thread to exit to restart agent";
      mon.reset(0);
      sLogger << LDEBUG << "Monitor has exited";
    }
  } while (mRestart);
}

void AgentConfiguration::stop()
{
  mAgent->clear();
}

Device *AgentConfiguration::defaultDevice()
{
  const std::vector<Device*> &devices = mAgent->getDevices();
  if (devices.size() == 1)
    return devices[0];
  else
    return NULL;
}

static const char *timestamp(char *aBuffer)
{
#ifdef _WINDOWS
  SYSTEMTIME st;
  GetSystemTime(&st);
  sprintf(aBuffer, "%4d-%02d-%02dT%02d:%02d:%02d.%04dZ", st.wYear, st.wMonth, st.wDay, st.wHour, 
          st.wMinute, st.wSecond, st.wMilliseconds);
#else
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  strftime(aBuffer, 64, "%Y-%m-%dT%H:%M:%S", gmtime(&tv.tv_sec));
  sprintf(aBuffer + strlen(aBuffer), ".%06dZ", (int) tv.tv_usec);
#endif
  
  return aBuffer;
}

void AgentConfiguration::LoggerHook(const std::string& aLoggerName,
                                    const dlib::log_level& l,
                                    const dlib::uint64 aThreadId,
                                    const char* aMessage)
{
  stringstream out;
  char buffer[64];
  timestamp(buffer);
  out << buffer << ": " << l.name << " [" << aThreadId << "] " << aLoggerName << ": " << aMessage;
#ifdef WIN32
  out << "\r\n";
#else
  out << "\n";
#endif
  if (mLoggerFile != NULL)
    mLoggerFile->write(out.str().c_str());
  else
    cout << out.str();
}

static dlib::log_level string_to_log_level (
                               const std::string& level
                               )
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
  else
  {
    return LINFO;
  }
}

void AgentConfiguration::configureLogger(dlib::config_reader::kernel_1a &aReader)
{
  if (mLoggerFile != NULL)
    delete mLoggerFile;

  if (mIsDebug) {
    set_all_logging_output_streams(cout);
    set_all_logging_levels(LDEBUG);
  }
  else
  {
    string name("agent.log");
    RollingFileLogger::RollingSchedule sched = RollingFileLogger::NEVER;
    int maxSize = 10 * 1024 * 1024;
    int maxIndex = 9;
    
    if (aReader.is_block_defined("logger_config"))
    {
      const config_reader::kernel_1a& cr = aReader.block("logger_config");
      
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
          if (one == "file" && three.size() == 0)
            name = two;
          else
            name = one;
        }
      }
      
      string maxSizeStr = get_with_default(cr, "max_size", "10M");
      stringstream ss(maxSizeStr);
      char mag = '\0';
      ss >> maxSize >> mag;

      switch(mag) {
      case 'G': case 'g': maxSize *= 1024;
      case 'M': case 'm': maxSize *= 1024;
      case 'K': case 'k': maxSize *= 1024;
      case 'B': case 'b': case '\0':
        break;
      }
        
      maxIndex = get_with_default(cr, "max_index", maxIndex);
      string schedCfg = get_with_default(cr, "schedule", "NEVER");
      if (schedCfg == "DAILY")
        sched = RollingFileLogger::DAILY;
      else if (schedCfg == "WEEKLY")
        sched = RollingFileLogger::WEEKLY;
    }
    
    mLoggerFile = new RollingFileLogger(name, maxIndex, maxSize, sched);
    set_all_logging_output_hooks<AgentConfiguration>(*this, &AgentConfiguration::LoggerHook);
  }
}

inline static void trim(std::string &str)
{
  size_t index = str.find_first_not_of(" \r\t");
  if (index != string::npos && index > 0)
    str.erase(0, index);
  index = str.find_last_not_of(" \r\t");
  if (index != string::npos)
    str.erase(index + 1);
}

void AgentConfiguration::loadConfig(std::istream &aFile)
{
  // Now get our configuration
  config_reader::kernel_1a reader(aFile);
  
  if (mLoggerFile == NULL)
    configureLogger(reader);

  bool defaultPreserve = get_bool_with_default(reader, "PreserveUUID", true);
  int port = get_with_default(reader, "Port", 5000);
  string serverIp = get_with_default(reader, "ServerIp", "");
  int bufferSize = get_with_default(reader, "BufferSize", DEFAULT_SLIDING_BUFFER_EXP);
  int maxAssets = get_with_default(reader, "MaxAssets", DEFAULT_MAX_ASSETS);
  int checkpointFrequency = get_with_default(reader, "CheckpointFrequency", 1000);
  int legacyTimeout = get_with_default(reader, "LegacyTimeout", 600);
  int reconnectInterval = get_with_default(reader, "ReconnectInterval", 10 * 1000);
  bool ignoreTimestamps = get_bool_with_default(reader, "IgnoreTimestamps", false);
  bool conversionRequired = get_bool_with_default(reader, "ConversionRequired", true);
  bool upcaseValue = get_bool_with_default(reader, "UpcaseDataItemValue", true);
  mMonitorFiles = get_bool_with_default(reader, "MonitorConfigFiles", false);
  mMinimumConfigReloadAge = get_with_default(reader, "MinimumConfigReloadAge", 15);
  
  mPidFile = get_with_default(reader, "PidFile", "agent.pid");
  std::vector<string> devices_files;
  if (reader.is_key_defined("Devices")) {
    string fileName = reader["Devices"];
    devices_files.push_back(fileName);
    if (!mExePath.empty() && fileName[0] != '/' && fileName[0] != '\\' && fileName[1] != ':')
      devices_files.push_back(mExePath + reader["Devices"]);
  }
  
  devices_files.push_back("Devices.xml");
  if (!mExePath.empty())
    devices_files.push_back(mExePath + "Devices.xml");
  devices_files.push_back("probe.xml");
  if (!mExePath.empty())
    devices_files.push_back(mExePath +"probe.xml");
  
  mDevicesFile.clear();
  
  for (string probe: devices_files)
  {
    struct stat buf;
    sLogger << LDEBUG << "Checking for Devices XML configuration file: " << probe;
	int res = stat(probe.c_str(), &buf);
	sLogger << LDEBUG << "  Stat returned: " << res;

    if (res == 0) {
      mDevicesFile = probe;
      break;
    }
	sLogger << LDEBUG << "Could not find file: " << probe;
	cout << "Could not locate file: " << probe << endl;
  }
  
  if (mDevicesFile.empty())
  {
    throw runtime_error(((string) "Please make sure the configuration "
                         "file probe.xml or Devices.xml is in the current "
                         "directory or specify the correct file "
                         "in the configuration file " + mConfigFile +
                         " using Devices = <file>").c_str());
  }
  

  mName = get_with_default(reader, "ServiceName", "MTConnect Agent");
  
  // Check for schema version
  string schemaVersion = get_with_default(reader, "SchemaVersion", "");
  if (!schemaVersion.empty())
    XmlPrinter::setSchemaVersion(schemaVersion);
  
  sLogger << LINFO << "Starting agent on port " << port;
  if (mAgent == NULL)
    mAgent = new Agent(mDevicesFile, bufferSize, maxAssets, checkpointFrequency);
  mAgent->set_listening_port(port);
  mAgent->set_listening_ip(serverIp);
  mAgent->setLogStreamData(get_bool_with_default(reader, "LogStreams", false));

  for (size_t i = 0; i < mAgent->getDevices().size(); i++)
    mAgent->getDevices()[i]->mPreserveUuid = defaultPreserve;
  
  if (XmlPrinter::getSchemaVersion().empty())
    XmlPrinter::setSchemaVersion("1.3");
  
  loadAllowPut(reader);
  loadAdapters(reader, defaultPreserve, legacyTimeout, reconnectInterval, ignoreTimestamps,
               conversionRequired, upcaseValue);
  
  // Files served by the Agent... allows schema files to be served by
  // agent.
  loadFiles(reader);
  
  // Load namespaces, allow for local file system serving as well.
  loadNamespace(reader, "DevicesNamespaces", &XmlPrinter::addDevicesNamespace);
  loadNamespace(reader, "StreamsNamespaces", &XmlPrinter::addStreamsNamespace);
  loadNamespace(reader, "AssetsNamespaces", &XmlPrinter::addAssetsNamespace);
  loadNamespace(reader, "ErrorNamespaces", &XmlPrinter::addErrorNamespace);
  
  loadStyle(reader, "DevicesStyle", &XmlPrinter::setDevicesStyle);
  loadStyle(reader, "StreamsStyle", &XmlPrinter::setStreamStyle);
  loadStyle(reader, "AssetsStyle", &XmlPrinter::setAssetsStyle);
  loadStyle(reader, "ErrorStyle", &XmlPrinter::setErrorStyle);
  
  loadTypes(reader);
}

void AgentConfiguration::loadAdapters(dlib::config_reader::kernel_1a &aReader,
                                      bool aDefaultPreserve, int aLegacyTimeout,
                                      int aReconnectInterval, bool aIgnoreTimestamps,
                                      bool aConversionRequired, bool aUpcaseValue)
{
  Device *device;
  if (aReader.is_block_defined("Adapters")) {
    const config_reader::kernel_1a &adapters = aReader.block("Adapters");
    std::vector<string> blocks;
    adapters.get_blocks(blocks);
    
    std::vector<string>::iterator block;
    for (block = blocks.begin(); block != blocks.end(); ++block)
    {      
      const config_reader::kernel_1a &adapter = adapters.block(*block);
      string deviceName;      
      if (adapter.is_key_defined("Device")) {
        deviceName = adapter["Device"].c_str();
      } else {
        deviceName = *block;
      }
      
      device = mAgent->getDeviceByName(deviceName);

      if (device == NULL) {
        sLogger << LWARN << "Cannot locate device name '" << deviceName << "', trying default";
        device = defaultDevice();
        if (device != NULL) {
          deviceName = device->getName();
          sLogger << LINFO << "Assigning default device " << deviceName << " to adapter";
        }
      }
      if (device == NULL) {
        sLogger << LWARN << "Cannot locate device name '" << deviceName << "', assuming dynamic";
      }
      
      const string host = get_with_default(adapter, "Host", (string)"localhost");
      int port = get_with_default(adapter, "Port", 7878);
      int legacyTimeout = get_with_default(adapter, "LegacyTimeout", aLegacyTimeout);
      int reconnectInterval = get_with_default(adapter, "ReconnectInterval", aReconnectInterval);

      
      sLogger << LINFO << "Adding adapter for " << deviceName << " on "
              << host << ":" << port;
      Adapter *adp = mAgent->addAdapter(deviceName, host, port, false, legacyTimeout);
      device->mPreserveUuid = get_bool_with_default(adapter, "PreserveUUID", aDefaultPreserve);
      
      // Add additional device information
      if (adapter.is_key_defined("UUID"))
        device->setUuid(adapter["UUID"]);
      if (adapter.is_key_defined("Manufacturer"))
        device->setManufacturer(adapter["Manufacturer"]);
      if (adapter.is_key_defined("Station"))
        device->setStation(adapter["Station"]);
      if (adapter.is_key_defined("SerialNumber"))
        device->setSerialNumber(adapter["SerialNumber"]);       
      
      adp->setDupCheck(get_bool_with_default(adapter, "FilterDuplicates", adp->isDupChecking()));
      adp->setAutoAvailable(get_bool_with_default(adapter, "AutoAvailable", adp->isAutoAvailable()));
      adp->setIgnoreTimestamps(get_bool_with_default(adapter, "IgnoreTimestamps", aIgnoreTimestamps ||
                                                                                  adp->isIgnoringTimestamps()));
      adp->setConversionRequired(get_bool_with_default(adapter, "ConversionRequired", aConversionRequired));
      adp->setRealTime(get_bool_with_default(adapter, "RealTime", false));
      adp->setRelativeTime(get_bool_with_default(adapter, "RelativeTime", false));
      adp->setReconnectInterval(reconnectInterval);
      adp->setUpcaseValue(get_bool_with_default(adapter, "UpcaseDataItemValue", aUpcaseValue));
      
      if (adapter.is_key_defined("AdditionalDevices")) {
        istringstream devices(adapter["AdditionalDevices"]);
        string name;
        while (getline(devices, name, ',')) 
        {
          size_t index = name.find_first_not_of(" \r\t");
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
  else if ((device = defaultDevice()) != NULL)
  {
    sLogger << LINFO << "Adding default adapter for " << device->getName() << " on localhost:7878";
    Adapter *adp = mAgent->addAdapter(device->getName(), "localhost", 7878, false, aLegacyTimeout);
    adp->setIgnoreTimestamps(aIgnoreTimestamps || adp->isIgnoringTimestamps());
    adp->setReconnectInterval(aReconnectInterval);
    device->mPreserveUuid = aDefaultPreserve;
  }
  else
  {
    throw runtime_error("Adapters must be defined if more than one device is present");
  }  
}

void AgentConfiguration::loadAllowPut(dlib::config_reader::kernel_1a &aReader)
{
  bool putEnabled = get_bool_with_default(aReader, "AllowPut", false);
  mAgent->enablePut(putEnabled);
  
  string putHosts = get_with_default(aReader, "AllowPutFrom", "");
  if (!putHosts.empty())
  {
    istringstream toParse(putHosts);
    string putHost;
    do {
      getline(toParse, putHost, ',');
      trim(putHost);
      if (!putHost.empty()) {
        string ip;
        int n;
        for (n = 0; dlib::hostname_to_ip(putHost, ip, n) == 0 && ip == "0.0.0.0"; n++)
          ip = "";
        if (!ip.empty()) {
          mAgent->enablePut();
          mAgent->allowPutFrom(ip);
        }
      }
    } while (!toParse.eof());
  }
}

void AgentConfiguration::loadNamespace(dlib::config_reader::kernel_1a &aReader,
                                       const char *aNamespaceType,
                                       NamespaceFunction *aCallback)
{
  // Load namespaces, allow for local file system serving as well.
  if (aReader.is_block_defined(aNamespaceType)) {
    const config_reader::kernel_1a &namespaces = aReader.block(aNamespaceType);
    std::vector<string> blocks;
    namespaces.get_blocks(blocks);
    
    std::vector<string>::iterator block;
    for (block = blocks.begin(); block != blocks.end(); ++block)
    {
      const config_reader::kernel_1a &ns = namespaces.block(*block);
      if (*block != "m" && !ns.is_key_defined("Urn"))
      {
        sLogger << LERROR << "Name space must have a Urn: " << *block;
      } else {
        string location, urn;
        if (ns.is_key_defined("Location"))
          location = ns["Location"];
        if (ns.is_key_defined("Urn"))
          urn = ns["Urn"];        
        (*aCallback)(urn, location, *block);
        if (ns.is_key_defined("Path") && !location.empty())
          mAgent->registerFile(location, ns["Path"]);        
      }
    }
  }
}

void AgentConfiguration::loadFiles(dlib::config_reader::kernel_1a &aReader)
{
  if (aReader.is_block_defined("Files")) {
    const config_reader::kernel_1a &files = aReader.block("Files");
    std::vector<string> blocks;
    files.get_blocks(blocks);
    
    std::vector<string>::iterator block;
    for (block = blocks.begin(); block != blocks.end(); ++block)
    {
      const config_reader::kernel_1a &file = files.block(*block);
      if (!file.is_key_defined("Location") || !file.is_key_defined("Path"))
      {
        sLogger << LERROR << "Name space must have a Location (uri) or Directory and Path: " << *block;
      } else {
        mAgent->registerFile(file["Location"], file["Path"]);
      }
    }
  }
}

void AgentConfiguration::loadStyle(dlib::config_reader::kernel_1a &aReader, const char *aDoc, StyleFunction *aFunction)
{
  if (aReader.is_block_defined(aDoc)) {
    const config_reader::kernel_1a &doc = aReader.block(aDoc);
    if (!doc.is_key_defined("Location")) {
      sLogger << LERROR << "A style must have a Location: " << aDoc;
    } else {
      string location = doc["Location"];
      aFunction(location);
      if (doc.is_key_defined("Path"))
        mAgent->registerFile(location, doc["Path"]);
    }
  }
}

void AgentConfiguration::loadTypes(dlib::config_reader::kernel_1a &aReader)
{
  if (aReader.is_block_defined("MimeTypes")) {
    const config_reader::kernel_1a &types = aReader.block("MimeTypes");
    std::vector<string> keys;
    types.get_keys(keys);
    
    std::vector<string>::iterator key;
    for (key = keys.begin(); key != keys.end(); ++key) {
      mAgent->addMimeType(*key, types[*key]);
    }
  }
}
