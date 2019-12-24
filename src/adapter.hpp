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
#include "data_item.hpp"
#include "globals.hpp"

#include <dlib/sockets.h>
#include <dlib/threads.h>

#include <chrono>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace dlib;

namespace mtconnect
{
  class Agent;
  class Device;

  class Adapter : public Connector, public threaded_object
  {
   public:
    // Associate adapter with a device & connect to the server & port
    Adapter(std::string device, const std::string &server, const unsigned int port,
            std::chrono::seconds legacyTimeout = std::chrono::seconds{600});

    // Virtual destructor
    ~Adapter() override;

    // Set pointer to the agent
    void setAgent(Agent &agent);
    bool isDupChecking() const
    {
      return m_dupCheck;
    }
    void setDupCheck(bool flag)
    {
      m_dupCheck = flag;
    }
    Device *getDevice() const
    {
      return m_device;
    }

    bool isAutoAvailable() const
    {
      return m_autoAvailable;
    }
    void setAutoAvailable(bool flag)
    {
      m_autoAvailable = flag;
    }

    bool isIgnoringTimestamps() const
    {
      return m_ignoreTimestamps;
    }
    void setIgnoreTimestamps(bool flag)
    {
      m_ignoreTimestamps = flag;
    }

    void setReconnectInterval(std::chrono::milliseconds interval)
    {
      m_reconnectInterval = interval;
    }
    std::chrono::milliseconds getReconnectInterval() const
    {
      return m_reconnectInterval;
    }

    void setRelativeTime(bool flag)
    {
      m_relativeTime = flag;
    }
    bool getRelativeTime() const
    {
      return m_relativeTime;
    }

    void setConversionRequired(bool flag)
    {
      m_conversionRequired = flag;
    }
    bool conversionRequired() const
    {
      return m_conversionRequired;
    }

    void setUpcaseValue(bool flag)
    {
      m_upcaseValue = flag;
    }
    bool upcaseValue() const
    {
      return m_upcaseValue;
    }

    uint64_t getBaseTime() const
    {
      return m_baseTime;
    }
    uint64_t getBaseOffset() const
    {
      return m_baseOffset;
    }

    bool isParsingTime() const
    {
      return m_parseTime;
    }
    void setParseTime(bool flag)
    {
      m_parseTime = flag;
    }

    // For testing...
    void setBaseOffset(uint64_t offset)
    {
      m_baseOffset = offset;
    }
    void setBaseTime(uint64_t offset)
    {
      m_baseTime = offset;
    }
    static void getEscapedLine(std::istringstream &stream, std::string &store);

    // Inherited method to incoming data from the server
    void processData(const std::string &data) override;
    void protocolCommand(const std::string &data) override;

    // Method called when connection is lost.
    void disconnected() override;
    void connected() override;

    bool isDuplicate(DataItem *dataItem, const std::string &value, double timeOffset) const
    {
      if (!dataItem->allowDups())
      {
        if (dataItem->hasMinimumDelta() || dataItem->hasMinimumPeriod())
          return dataItem->isFiltered(dataItem->convertValue(atof(value.c_str())), timeOffset);
        else
          return m_dupCheck && dataItem->isDuplicate(value);
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
    void processAsset(std::istringstream &toParse, const std::string &key, const std::string &value,
                      const std::string &time);
    bool processDataItem(std::istringstream &toParse, const std::string &line,
                         const std::string &key, const std::string &value, const std::string &time,
                         double offset, bool first = false);
    std::string extractTime(const std::string &time, double &offset);

   protected:
    // Pointer to the agent
    Agent *m_agent;
    Device *m_device;
    std::vector<Device *> m_allDevices;

    // Name of device associated with adapter
    std::string m_deviceName;

    // If the connector has been running
    bool m_running;

    // Check for dups
    bool m_dupCheck;
    bool m_autoAvailable;
    bool m_ignoreTimestamps;
    bool m_relativeTime;
    bool m_conversionRequired;
    bool m_upcaseValue;

    // For relative times
    uint64_t m_baseTime;
    uint64_t m_baseOffset;

    bool m_parseTime;

    // For multiline asset parsing...
    bool m_gatheringAsset;
    std::string m_terminator;
    std::string m_assetId;
    std::string m_assetType;
    std::string m_time;
    std::ostringstream m_body;
    Device *m_assetDevice;
    std::set<std::string> m_logOnce;

    // Timeout for reconnection attempts, given in milliseconds
    std::chrono::milliseconds m_reconnectInterval;

   private:
    // Inherited and is run as part of the threaded_object
    void thread() override;
  };
}  // namespace mtconnect
