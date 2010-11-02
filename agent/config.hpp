#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "service.hpp"
#include <string>

class Agent;
class Device;

class AgentConfiguration : public MTConnectService {
public:
  AgentConfiguration();
  ~AgentConfiguration();
  
  // For MTConnectService
  virtual void stop();
  virtual void start() ;
  virtual void initialize(int aArgc, const char *aArgv[]) ;
  
protected:
  void loadConfig();
  Device *defaultDevice();
  
protected:
  Agent *mAgent;
};

#endif
