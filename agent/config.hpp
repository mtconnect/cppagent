#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "service.hpp"
#include <string>

class Agent;
class Device;

typedef void (NamespaceFunction)(const std::string &aUrn, const std::string &aLocation, 
                                  const std::string &aPrefix);

class AgentConfiguration : public MTConnectService {
public:
  AgentConfiguration();
  virtual ~AgentConfiguration();
  
  // For MTConnectService
  virtual void stop();
  virtual void start() ;
  virtual void initialize(int aArgc, const char *aArgv[]);

  void configureLogger();
  void loadConfig(std::istream &aFile);

  void setAgent(Agent *aAgent) { mAgent = aAgent; }
  Agent *getAgent() { return mAgent; }
  
protected:
  Device *defaultDevice();
  void loadAdapters(dlib::config_reader::kernel_1a &aReader, bool aDefaultPreserve,
                    int aLegacyTimeout, int aReconnectInterval, bool aIgnoreTimestamps);
  void loadAllowPut(dlib::config_reader::kernel_1a &aReader);
  void loadNamespace(dlib::config_reader::kernel_1a &aReader, 
                     const char *aNamespaceType, 
                     NamespaceFunction *aCallback);
  void loadFiles(dlib::config_reader::kernel_1a &aReader);
  
protected:
  Agent *mAgent;
};

#endif
