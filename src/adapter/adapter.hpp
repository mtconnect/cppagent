//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "agent.hpp"
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

    using Micros = std::chrono::microseconds;
    using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

    struct Context
    {
      using GetDevice = std::function<const Device *(const std::string &id)>;
      using GetDataItem =
          std::function<const DataItem *(const Device *device, const std::string &id)>;
      using Now = std::function<Timestamp()>;

      GetDevice m_getDevice;
      GetDataItem m_getDataItem;
      Now m_now;

      // Logging Context
      std::set<std::string> m_logOnce;

      // Time handling
      bool m_ignoreTimestamps{false};
      bool m_relativeTime{false};
      bool m_dupCheck{false};
      bool m_conversionRequired{false};
      bool m_upcaseValue{false};
      bool m_realTime{false};
      
      std::string m_defaultDevice;
      std::optional<Timestamp> m_base;
      Micros m_offset;
    };

    struct Handler
    {
      using ProcessData = std::function<void(const std::string &, Context &context)>;
      using Connect = std::function<void(const Adapter &, Context &context)>;

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
      Adapter(std::string device, const std::string &server, const unsigned int port,
              std::chrono::seconds legacyTimeout = std::chrono::seconds{600});

      // Virtual destructor
      ~Adapter() override;

      void setHandler(std::unique_ptr<Handler> &h) { m_handler = std::move(h); }

      // Set pointer to the agent
      void setAgent(Agent &agent);
      bool isDupChecking() const { return m_context.m_dupCheck; }
      void setDupCheck(bool flag) { m_context.m_dupCheck = flag; }
      Device *getDevice() const { return m_device; }
      const auto &getDeviceName() const { return m_deviceName; }

      bool isAutoAvailable() const { return m_autoAvailable; }
      void setAutoAvailable(bool flag) { m_autoAvailable = flag; }

      bool isIgnoringTimestamps() const { return m_context.m_ignoreTimestamps; }
      void setIgnoreTimestamps(bool flag) { m_context.m_ignoreTimestamps = flag; }

      void setReconnectInterval(std::chrono::milliseconds interval)
      {
        m_reconnectInterval = interval;
      }
      std::chrono::milliseconds getReconnectInterval() const { return m_reconnectInterval; }

      void setRelativeTime(bool flag) { m_context.m_relativeTime = flag; }
      bool getRelativeTime() const { return m_context.m_relativeTime; }

      void setConversionRequired(bool flag) { m_context.m_conversionRequired = flag; }
      bool conversionRequired() const { return m_context.m_conversionRequired; }

      void setUpcaseValue(bool flag) { m_context.m_upcaseValue = flag; }
      bool upcaseValue() const { return m_context.m_upcaseValue; }
      
      auto &getTerminator() const { return m_terminator; }

      // Inherited method to incoming data from the server
      void processData(const std::string &data) override;
      void protocolCommand(const std::string &data) override
      {
        if (m_handler && m_handler->m_protocolCommand)
          m_handler->m_protocolCommand(data, m_context);
      }

      // Method called when connection is lost.
      void connecting() override
      {
        if (m_handler && m_handler->m_connecting)
          m_handler->m_connecting(*this, m_context);
      }
      void disconnected() override
      {
        m_context.m_base.reset();
        if (m_handler && m_handler->m_disconnected)
          m_handler->m_disconnected(*this, m_context);
      }
      void connected() override
      {
        if (m_handler && m_handler->m_connected)
          m_handler->m_connected(*this, m_context);
      }

      // Agent Device methods
      const std::string &getUrl() const { return m_url; }
      const std::string &getIdentity() const { return m_identity; }

      bool isDuplicate(DataItem *dataItem, const std::string &value, double timeOffset) const
      {
        if (!dataItem->allowDups())
        {
          if (dataItem->hasMinimumDelta() || dataItem->hasMinimumPeriod())
            return dataItem->isFiltered(dataItem->convertValue(stringToFloat(value.c_str())),
                                        timeOffset);
          else
            return m_context.m_dupCheck && dataItem->isDuplicate(value);
        }
        else
          return false;
      }

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
      // Pointer to the agent
      Context m_context;

      Agent *m_agent;
      Device *m_device;
      std::vector<Device *> m_allDevices;

      std::unique_ptr<Handler> m_handler;

      // Name of device associated with adapter
      std::string m_deviceName;
      std::string m_url;
      std::string m_identity;

      // If the connector has been running
      bool m_running;

      // Check for dups
      bool m_autoAvailable;

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
