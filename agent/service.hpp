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

#include <string>

namespace mtconnect {
  class MTConnectService
  {
  public:
    MTConnectService();
    virtual int main(int argc, char const *argv[]);
    virtual void initialize(int argc, char const *argv[]) = 0;
    virtual void stop() = 0;
    virtual void start() = 0;
    
    void setName(std::string const &name) {
      m_name = name; }
    std::string const & name() const {
      return m_name; }
    
  protected:
    std::string m_name;
    std::string m_configFile;
    std::string m_pidFile;
    bool m_isService;
    bool m_isDebug;
    
    void install();
    void remove();
    
#ifndef _WINDOWS
    void daemonize();
#endif
    
  };
}
