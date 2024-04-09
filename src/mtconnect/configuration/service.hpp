//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#pragma once

#include <boost/optional.hpp>
#include <boost/program_options/variables_map.hpp>

#include <filesystem>
#include <string>

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  namespace configuration {
    /// @brief Abstract service supporting running as a Windows service or *NIX daemon
    class AGENT_LIB_API MTConnectService
    {
    public:
      MTConnectService() = default;
      virtual ~MTConnectService() = default;

      /// @brief command line parser and entry point for agent
      /// @param[in] argc the count of arguments
      /// @param[in] argv the arguments
      virtual int main(int argc, char const *argv[]);
      /// @brief initialize the service with the parser command line options
      virtual void initialize(const boost::program_options::variables_map &options) = 0;
      /// @brief stop the srvice
      virtual void stop() = 0;
      /// @brief start the service
      virtual void start() = 0;

      /// @brief set the name of the service
      /// @param[in] name name of the service
      void setName(std::string const &name) { m_name = name; }
      /// @brief get the name of the service
      /// @return service name
      std::string const &name() const { return m_name; }
      /// @brief set the debugging state
      /// @param debug `true` if debugging
      void setDebug(bool debug) { m_isDebug = debug; }
      /// @brief get the debugging state
      /// @return `true` if debugging
      bool getDebug() { return m_isDebug; }

      /// @brief write out usage text to standard out
      /// @param ec the exit code
      virtual void usage(int ec = 0);

      /// @brief Parse command line options
      /// @param argc number of optons
      /// @param argv option values
      /// @param command optonal command for testing
      /// @param config optional configration file for testing
      /// @return boost variable map
      boost::program_options::variables_map parseOptions(int argc, const char *argv[],
                                                         boost::optional<std::string> &command,
                                                         boost::optional<std::string> &config);

    protected:
      std::string m_name;
      std::filesystem::path m_configFile;
      std::string m_pidFile;
      bool m_isService = false;
      bool m_isDebug = false;

      void install();
      void remove();
      static bool isElevated();

#ifndef _WINDOWS
      void daemonize();
#endif
    };
  }  // namespace configuration
}  // namespace mtconnect
