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
#pragma once

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

  void setAgent(Agent *aAgent) { m_agent = aAgent; }
  Agent *getAgent() { return m_agent; }

  RollingFileLogger *getLogger() { return m_loggerFile; }
  
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
  
  void monitorThread();

protected:
  Agent *m_agent;
  RollingFileLogger *m_loggerFile;
  bool m_monitorFiles;
  int m_minimumConfigReloadAge;
  std::string m_devicesFile;
  bool m_restart;
  std::string m_exePath;
};
