
#include "config.hpp"
#include "agent.hpp"
#include "options.hpp"
#include "device.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <dlib/config_reader.h>
#include <stdexcept>

using namespace std;
using namespace dlib;

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

AgentConfiguration::AgentConfiguration()
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
    loadConfig();
  }
  catch (std::exception & e)
  {
    logEvent("Cppagent::Main", e.what());
    std::cerr << "Agent failed to load: " << e.what() << std::endl;
    optionList.usage();
  }
}

AgentConfiguration::~AgentConfiguration()
{
  delete mAgent;
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
  const vector<Device*> &devices = mAgent->getDevices();
  if (devices.size() == 1)
    return devices[0];
  else
    return NULL;
}

void AgentConfiguration::loadConfig()
{
  ifstream file(mConfigFile.c_str());
  config_reader::kernel_1a reader(file);
  
  int port = get_with_default(reader, "Port", 5000);
  int bufferSize = get_with_default(reader, "BufferSize", DEFAULT_SLIDING_BUFFER_EXP);
  int checkpointFrequency = get_with_default(reader, "CheckpointFrequency", 1000);
  const char *probe = get_with_default(reader, "Devices", "probe.xml");
  mName = get_with_default(reader, "ServiceName", "MTConnect Agent"); 
  gLogFile = get_with_default(reader, "agent.log", "agent.log");
  
  mAgent = new Agent(probe, bufferSize, checkpointFrequency);
  mAgent->set_listening_port(port);
  Device *device;
  if (reader.is_block_defined("Adapters")) {
    const config_reader::kernel_1a &adapters = reader.block("Adapters");
    vector<string> blocks;
    adapters.get_blocks(blocks);
    
    vector<string>::iterator block;
    for (block = blocks.begin(); block != blocks.end(); ++block)
    {
      device;
      if (*block == "*")
        device = defaultDevice();
      else
        device = mAgent->getDeviceByName(*block);
      if (device == NULL) {
        throw new runtime_error("Can't locate device " + *block + " in XML configuration file.");
      }
      
      const config_reader::kernel_1a &adapter = adapters.block(*block);
      const string &host = get_with_default(adapter, "Host", (string)"localhost");
      int port = get_with_default(adapter, "Port", 7878);
      
      mAgent->addAdapter(device->getName(), host, port);

      // Add additional device information
      if (adapter.is_key_defined("UUID"))
        device->setUuid(adapter["UUID"]);
      if (adapter.is_key_defined("Manufacturer"))
        device->setManufacturer(adapter["Manufacturer"]);
      if (adapter.is_key_defined("Station"))
        device->setStation(adapter["Station"]);
      if (adapter.is_key_defined("SerialNumber"))
        device->setSerialNumber(adapter["SerialNumber"]);
    }
  }
  else if ((device = defaultDevice()) != NULL)
  {
    mAgent->addAdapter(device->getName(), "localhost", 7878);
  }
  else
  {
    throw new runtime_error("Adapters must be defined if more than one device is present");
  }
}
