
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

void AgentConfiguration::loadConfig()
{
  ifstream file(mConfigFile.c_str());
  config_reader::kernel_1a reader(file);
  
  int port = 5000;
  if (reader.is_key_defined("Port"))
    port = atoi(reader["Port"].c_str());
  
  int bufferSize = DEFAULT_SLIDING_BUFFER_EXP;
  if (reader.is_key_defined("BufferSize"))
    bufferSize = atoi(reader["BufferSize"].c_str());
  
  int checkpointFrequency = 1000;
  if (reader.is_key_defined("CheckpointFrequency"))
    checkpointFrequency = atoi(reader["CheckpointFrequency"].c_str());

  const char *probe = "probe.xml";
  if (reader.is_key_defined("Devices"))
    probe = reader["Devices"].c_str();
  
  if (reader.is_key_defined("ServiceName"))
    mName = reader["ServiceName"];
    
  gLogFile = "agent.log";
  if (reader.is_key_defined("LogFile"))
    gLogFile = reader["LogFile"];
  
  mAgent = new Agent(probe, bufferSize, checkpointFrequency);
  mAgent->set_listening_port(port);
  
  if (reader.is_block_defined("Adapters")) {
    const config_reader::kernel_1a &adapters = reader.block("Adapters");
    vector<string> blocks;
    adapters.get_blocks(blocks);
    
    vector<string>::iterator block;
    for (block = blocks.begin(); block != blocks.end(); ++block)
    {
      if (mAgent->getDeviceByName(*block) == 0)
      {
        throw new runtime_error("Can't locate device " + *block + " in XML configuration file.");
      }
      
      const config_reader::kernel_1a &adapter = adapters.block(*block);
      if (!adapter.is_key_defined("Host"))
      {
        throw new runtime_error("Host must be defined for " + *block);
      }
      
      const string &host = adapter["Host"];
      int port = 7878;
      if (adapter.is_key_defined("Port"))
        port = atoi(adapter["Port"].c_str());
      
      mAgent->addAdapter(*block, host, port);

      // Add additional device information
      Device *device = mAgent->getDeviceByName(*block);
      if (device != NULL)
      {
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
  }
  else
  {
    throw new runtime_error("Adapters must be defined");
  }
}
