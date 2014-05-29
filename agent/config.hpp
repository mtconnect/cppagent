#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "service.hpp"
#include <string>

class Agent;
class Device;
class log_level;
class RollingFileLogger;

typedef void (NamespaceFunction)(const std::string &aUrn, const std::string &aLocation, 
                                  const std::string &aPrefix);
typedef void (StyleFunction)(const std::string &aLocation);

class AgentConfiguration : public MTConnectService {
public:
  AgentConfiguration();
  virtual ~AgentConfiguration();
  
  // For MTConnectService
  virtual void stop();
  virtual void start() ;
  virtual void initialize(int aArgc, const char *aArgv[]);

  void configureLogger(dlib::config_reader::kernel_1a &aReader);
  void loadConfig(std::istream &aFile);

  void setAgent(Agent *aAgent) { mAgent = aAgent; }
  Agent *getAgent() { return mAgent; }

  RollingFileLogger *getLogger() { return mLoggerFile; }
  
protected:
  Device *defaultDevice();
  void loadAdapters(dlib::config_reader::kernel_1a &aReader, bool aDefaultPreserve,
                    int aLegacyTimeout, int aReconnectInterval, bool aIgnoreTimestamps,
                    bool aConversionRequired, bool aUpcaseValue);
  void loadAllowPut(dlib::config_reader::kernel_1a &aReader);
  void loadNamespace(dlib::config_reader::kernel_1a &aReader, 
                     const char *aNamespaceType, 
                     NamespaceFunction *aCallback);
  void loadFiles(dlib::config_reader::kernel_1a &aReader);
  void loadStyle(dlib::config_reader::kernel_1a &aReader, const char *aDoc, StyleFunction *aFunction);
  void loadTypes(dlib::config_reader::kernel_1a &aReader);
  
  void LoggerHook(const std::string& aLoggerName,
                  const dlib::log_level& l,
                  const dlib::uint64 aThreadId,
                  const char* aMessage);

protected:
  Agent *mAgent;
  RollingFileLogger *mLoggerFile;
};

#endif
