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

#pragma once

#include "connector.hpp"
#include "device_model/data_item.hpp"
#include "globals.hpp"

#include <date/tz.h>

#include <dlib/sockets.h>
#include <dlib/threads.h>

#include <chrono>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace dlib;

namespace mtconnect
{
  class Agent;
  class Device;

  namespace adapter
  {
    class Adapter;

    struct Handler
    {
      using ProcessData = std::function<void(const std::string &)>;
      using Connect = std::function<void(const Adapter &)>;

      ProcessData m_processData;
      ProcessData m_protocolCommand;

      Connect m_connecting;
      Connect m_connected;
      Connect m_disconnected;
    };

    class Adapter : public Connector, public threaded_object
    {
    public:
      // Associate adapter with a device & connect to the server & port
      Adapter(const std::string &server, const unsigned int port,
              const ConfigOptions &options);
      Adapter(const Adapter &) = delete;

      // Virtual destructor
      ~Adapter() override;

      void setHandler(std::unique_ptr<Handler> &h) { m_handler = std::move(h); }

      // Set pointer to the agent
      void setReconnectInterval(std::chrono::milliseconds interval)
      {
        m_reconnectInterval = interval;
      }
      std::chrono::milliseconds getReconnectInterval() const { return m_reconnectInterval; }

      auto &getTerminator() const { return m_terminator; }

      // Inherited method to incoming data from the server
      void processData(const std::string &data) override;
      void protocolCommand(const std::string &data) override
      {
        if (m_handler && m_handler->m_protocolCommand)
          m_handler->m_protocolCommand(data);
      }

      // Method called when connection is lost.
      void connecting() override
      {
        if (m_handler && m_handler->m_connecting)
          m_handler->m_connecting(*this);
      }
      void disconnected() override
      {
        if (m_handler && m_handler->m_disconnected)
          m_handler->m_disconnected(*this);
      }
      void connected() override
      {
        if (m_handler && m_handler->m_connected)
          m_handler->m_connected(*this);
      }

      // Agent Device methods
      const std::string &getUrl() const { return m_url; }
      const std::string &getIdentity() const { return m_identity; }

      // Stop
      void stop();

      // For the additional devices associated with this adapter
      void addDevice(std::string &device);

    protected:
      void parseCalibration(const std::string &calibString);
      void processAsset(std::istringstream &toParse, const std::string &key,
                        const std::string &value, const std::string &time);
      bool processDataItem(std::istringstream &toParse, const std::string &line,
                           const std::string &key, const std::string &value,
                           const std::string &time, double offset, bool first = false);
      std::string extractTime(const std::string &time, double &offset);

    protected:
      std::unique_ptr<Handler> m_handler;

      // Name of device associated with adapter
      std::string m_url;
      std::string m_identity;

      // If the connector has been running
      bool m_running;

      std::optional<std::string> m_terminator;
      std::stringstream m_body;

      // Timeout for reconnection attempts, given in milliseconds
      std::chrono::milliseconds m_reconnectInterval;

    private:
      // Inherited and is run as part of the threaded_object
      void thread() override;
    };
  }  // namespace adapter
}  // namespace mtconnect
