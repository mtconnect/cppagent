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

#include "component_configuration.hpp"
#include "globals.hpp"

#include <utility>
#include <vector>

namespace mtconnect
{
  class SensorConfiguration : public ComponentConfiguration
  {
   public:
    struct Calibration
    {
      Calibration(std::string date, std::string nextDate, std::string initials)
          : m_date(std::move(date)),
            m_nextDate(std::move(nextDate)),
            m_initials(std::move(initials))
      {
      }
      Calibration(const Calibration &other)
          : m_date(other.m_date), m_nextDate(other.m_nextDate), m_initials(other.m_initials)
      {
      }
      Calibration()
      {
      }
      ~Calibration()
      {
      }

      std::string m_date;
      std::string m_nextDate;
      std::string m_initials;
    };

    class Channel
    {
     public:
      Channel(std::string calibrationDate, std::string nextCalibrationDate, std::string initials,
              const std::map<std::string, std::string> &attrs)
          : m_attributes(attrs),
            m_calibration(std::move(calibrationDate), std::move(nextCalibrationDate),
                          std::move(initials))
      {
      }
      Channel(const Channel &other)
          : m_attributes(other.m_attributes),
            m_description(other.m_description),
            m_calibration(other.m_calibration)
      {
      }
      ~Channel()
      {
      }

      void setDescription(const std::string &desc)
      {
        m_description = desc;
      }
      const std::string &getDescription() const
      {
        return m_description;
      }
      const std::map<std::string, std::string> &getAttributes() const
      {
        return m_attributes;
      }
      const Calibration &getCalibration() const
      {
        return m_calibration;
      }

     protected:
      std::map<std::string, std::string> m_attributes;
      std::string m_description;
      Calibration m_calibration;
    };

    // Sensor Configuration begins here
   public:
    SensorConfiguration(std::string firmwareVer, std::string calibrationDate,
                        std::string nextCalibrationDate, std::string initials, std::string rest)
        : m_firmwareVersion(std::move(firmwareVer)),
          m_calibration(std::move(calibrationDate), std::move(nextCalibrationDate),
                        std::move(initials)),
          m_rest(std::move(rest))
    {
    }
    virtual ~SensorConfiguration()
    {
    }

    void addChannel(const Channel &channel)
    {
      m_channels.push_back(channel);
    }

    const std::vector<Channel> &getChannels() const
    {
      return m_channels;
    }
    const Calibration &getCalibration() const
    {
      return m_calibration;
    }
    const std::string &getRest() const
    {
      return m_rest;
    }
    const std::string &getFirmwareVersion() const
    {
      return m_firmwareVersion;
    }

   protected:
    std::string m_firmwareVersion;
    Calibration m_calibration;
    std::string m_rest;
    std::vector<Channel> m_channels;
  };
}  // namespace mtconnect
