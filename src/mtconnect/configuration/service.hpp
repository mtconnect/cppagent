//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "utilities.hpp"

namespace mtconnect {
  namespace configuration {
    class MTConnectService
    {
    public:
      MTConnectService() = default;
      virtual ~MTConnectService() = default;

      virtual int main(int argc, char const *argv[]);
      virtual void initialize(const boost::program_options::variables_map &options) = 0;
      virtual void stop() = 0;
      virtual void start() = 0;

      void setName(std::string const &name) { m_name = name; }
      std::string const &name() const { return m_name; }
      void setDebug(bool debug) { m_isDebug = debug; }
      bool getDebug() { return m_isDebug; }

      virtual void usage(int ec = 0);

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
