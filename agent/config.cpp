/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

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
#include <sys/stat.h>
#include "rolling_file_logger.hpp"

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
  mAgent(NULL), mLoggerFile(NULL)
{
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

void AgentConfiguration::start()
{
  mAgent->start();
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
  
  mPidFile = get_with_default(reader, "PidFile", "agent.pid");
  const char *probe;
  struct stat buf;
  if (reader.is_key_defined("Devices")) {
    probe = reader["Devices"].c_str();
    if (stat(probe, &buf) != 0) {
      throw runtime_error(((string) "Please make sure the Devices XML configuration "
			   "file " + probe + " is in the current path ").c_str());
    }
  } else {
    probe = "probe.xml";
    if (stat(probe, &buf) != 0)
      probe = "Devices.xml";    
    if (stat(probe, &buf) != 0) {
      throw runtime_error(((string) "Please make sure the configuration "
			   "file probe.xml or Devices.xml is in the current "
			   "directory or specify the correct file "
			   "in the configuration file " + mConfigFile +
			   " using Devices = <file>").c_str());
    }
  }

  mName = get_with_default(reader, "ServiceName", "MTConnect Agent");
  
  // Check for schema version
  string schemaVersion = get_with_default(reader, "SchemaVersion", "");
  if (!schemaVersion.empty())
    XmlPrinter::setSchemaVersion(schemaVersion);
  
  sLogger << LINFO << "Starting agent on port " << port;
  if (mAgent == NULL)
    mAgent = new Agent(probe, bufferSize, maxAssets, checkpointFrequency);
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
