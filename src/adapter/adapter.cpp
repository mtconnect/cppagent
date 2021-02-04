//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#define __STDC_LIMIT_MACROS 1
#include "adapter/adapter.hpp"

#include "device_model/device.hpp"

#include <dlib/logger.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

using namespace std;
using namespace std::literals;
using namespace date::literals;

namespace mtconnect
{
  namespace adapter
  {
    static dlib::logger g_logger("input.adapter");

    // Adapter public methods
    Adapter::Adapter(const string &server, const unsigned int port, const ConfigOptions &options,
                     std::unique_ptr<AdapterPipeline> &pipeline)
      : Connector(server, port, 60s),
        m_pipeline(std::move(pipeline)),
        m_running(true),
        m_reconnectInterval{10000ms},
        m_options(options)
    {
      auto timeout = options.find("LegacyTimeout");
      if (timeout != options.end())
        m_legacyTimeout = get<Seconds>(timeout->second);

      stringstream url;
      url << "shdr://" << server << ':' << port;
      m_url = url.str();

      stringstream identity;
      identity << '_' << server << '_' << port;
      m_identity = identity.str();

      m_handler = m_pipeline->makeHandler();
      if (m_pipeline->hasContract())
        m_pipeline->build(m_options);      
    }

    Adapter::~Adapter()
    {
      if (m_running)
        stop();
    }

    void Adapter::stop()
    {
      // Will stop threaded object gracefully Adapter::thread()
      m_running = false;
      close();
      wait();
    }

    void Adapter::processData(const string &data)
    {
      if (m_terminator)
      {
        if (data == *m_terminator)
        {
          if (m_handler && m_handler->m_processData)
            m_handler->m_processData(m_body.str(), getIdentity());
          m_terminator.reset();
          m_body.str("");
        }
        else
        {
          m_body << endl << data;
        }

        return;
      }

      if (size_t multi = data.find("--multiline--"); multi != string::npos)
      {
        m_body.str("");
        m_body << data.substr(0, multi);
        m_terminator = data.substr(multi);
        return;
      }

      if (m_handler && m_handler->m_processData)
        m_handler->m_processData(data, getIdentity());
    }
    
    inline bool is_true(const std::string &value)
    {
      return value == "yes" || value == "true";
    }
    
    void Adapter::protocolCommand(const std::string &data)
    {
      static auto pattern = regex("\\*[ ]*([^:]+):[ ]*(.+)");
      smatch match;
      
      if (std::regex_match(data, match, pattern))
      {
        auto command = match[1].str();
        auto value = match[2].str();

        ConfigOptions options;
        
        if (command == "conversionRequired")
          options["ConversionRequired"] = is_true(value);
        else if (command == "relativeTime")
          options["RelativeTime"] = is_true(value);
        else if (command == "realTime")
          options["RealTime"] = is_true(value);
        else if (command == "device")
          options["Device"] = value;
        
        if (options.size() > 0)
          setOptions(options);
        else if (m_handler && m_handler->m_command)
          m_handler->m_command(data, getIdentity());
      }
    }


    // Adapter private methods
    void Adapter::thread()
    {
      while (m_running)
      {
        try
        {
          // Start the connection to the socket
          connect();

          // make sure we're closed...
          close();
        }
        catch (std::invalid_argument &err)
        {
          g_logger << LERROR << "Adapter for " << m_url
                   << "'s thread threw an argument error, stopping adapter: " << err.what();
          stop();
        }
        catch (std::exception &err)
        {
          g_logger << LERROR << "Adapter for " << m_url
                   << "'s thread threw an exceotion, stopping adapter: " << err.what();
          stop();
        }
        catch (...)
        {
          g_logger << LERROR << "Thread for adapter " << m_url
                   << "'s thread threw an unhandled exception, stopping adapter";
          stop();
        }

        if (!m_running)
          break;

        // Try to reconnect every 10 seconds
        g_logger << LINFO << "Will try to reconnect in " << m_reconnectInterval.count()
                 << " milliseconds";
        this_thread::sleep_for(m_reconnectInterval);
      }
      g_logger << LINFO << "Adapter thread stopped";
    }
  }  // namespace adapter
}  // namespace mtconnect
