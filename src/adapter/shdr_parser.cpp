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

#include "shdr_parser.hpp"

#include "agent.hpp"
#include "asset_mapper.hpp"
#include "data_item_mapper.hpp"
#include "observation.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"

#include <date/date.h>

#include <optional>

using namespace std;

namespace mtconnect
{
  namespace adapter
  {
    static dlib::logger g_logger("ShdrParser");

    void ShdrParser::mapTokens(TokenList::const_iterator &token,
                               const TokenList::const_iterator &end, Context &context)
    {
      ShdrObservation timestamp;
      ExtractTimestamp(timestamp, token, end, context);
      while (token != end)
      {
        TokenList::const_iterator start = token;
        try
        {
          entity::ErrorList errors;
          ShdrObservation observation(timestamp);
          if ((*token)[0] == '@')
          {
            MapTokensToAsset(observation, token, end, context);

            // Send downstream
            auto obs = get<AssetObservation>(observation.m_observed);
          }
          else
          {
            MapTokensToDataItem(observation, token, end, context);

            // TODO: Forward observation to Agent
            // Build an observation
            Observation2Ptr outgoing;
            auto obs = get<DataItemObservation>(observation.m_observed);
            if (obs.m_dataItem == nullptr)
            {
              throw entity::EntityError("Could not find data item");
            }

            if (obs.m_dataItem->getCategory())
            {
              auto out =
                  Observation2::makeObservation(obs.m_dataItem, observation.m_properties, errors);
              if (errors.empty())
              {
                if (obs.m_unavailable)
                  out->makeUnavailable();

                out->setTimestamp(timestamp.m_timestamp);

                // Deliver
                if (m_handler)
                  m_handler(out);
              }
              else
              {
                for (auto &e : errors)
                {
                  g_logger << dlib::LWARN << "Error while parsing tokens: " << e->what();
                  for (auto it = start; it != token; it++)
                    g_logger << dlib::LWARN << "    token: " << *it;
                }
              }
            }
          }
        }
        catch (entity::EntityError &e)
        {
          g_logger << dlib::LERROR << "Could not create observation: " << e.what();
          for (auto it = start; it != token; it++)
            g_logger << dlib::LWARN << "    token: " << *it;
        }
      }
    }

    void ShdrParser::processData(const std::string &data, Context &context)
    {
      using namespace std;

      try
      {
        auto tokens = ShdrTokenizer::tokenize(data);
        if (tokens.size() > 2)
        {
          auto token = tokens.cbegin();
          auto end = tokens.end();

          mapTokens(token, end, context);
        }
        else
        {
          g_logger << dlib::LWARN << "Insufficient tokens in line: " << data;
        }
      }

      catch (std::logic_error &e)
      {
        g_logger << dlib::LWARN << "Error (" << e.what() << ") with line: " << data;
      }

      catch (...)
      {
        g_logger << dlib::LWARN << "Unknown Error on line: " << data;
      }
    }

#if 0
    
    void Adapter::protocolCommand(const std::string &data)
    {
      // Handle initial push of settings for uuid, serial number and manufacturer.
      // This will override the settings in the device from the xml
      if (data == "* PROBE")
      {
        //      const Printer *printer = m_agent->getPrinter("xml");
        //      string response = m_agent->handleProbe(printer, m_deviceName);
        //      string probe = "* PROBE LENGTH=";
        //      probe.append(intToString(response.length()));
        //      probe.append("\n");
        //      probe.append(response);
        //      probe.append("\n");
        //      m_connection->write(probe.c_str(), probe.length());
      }
      else
      {
        size_t index = data.find(':', 2);
        if (index != string::npos)
        {
          // Slice from the second character to the :, without the colon
          string key = data.substr(2, index - 2);
          trim(key);
          string value = data.substr(index + 1);
          trim(value);
          
          bool deviceChanged{false};
          std::string oldName, oldUuid;
          if (m_device)
          {
            oldName = m_device->getName();
            oldUuid = m_device->getUuid();
          }
          
          if (m_device && key == "uuid" && !m_device->m_preserveUuid)
          {
            m_device->setUuid(value);
            deviceChanged = true;
          }
          else if (m_device && key == "manufacturer")
          {
            m_device->setManufacturer(value);
            deviceChanged = true;
          }
          else if (m_device && key == "station")
          {
            m_device->setStation(value);
            deviceChanged = true;
          }
          else if (m_device && key == "serialNumber")
          {
            m_device->setSerialNumber(value);
            deviceChanged = true;
          }
          else if (m_device && key == "description")
          {
            m_device->setDescription(value);
            deviceChanged = true;
          }
          else if (m_device && key == "nativeName")
          {
            m_device->setNativeName(value);
            deviceChanged = true;
          }
          else if (m_device && key == "calibration")
          {
            parseCalibration(value);
            deviceChanged = true;
          }
          else if (key == "conversionRequired")
            m_conversionRequired = is_true(value);
          else if (key == "relativeTime")
            m_relativeTime = is_true(value);
          else if (key == "realTime")
            m_realTime = is_true(value);
          else if (key == "device")
          {
            auto device = m_agent->findDeviceByUUIDorName(value);
            if (device)
            {
              m_device = device;
              g_logger << LINFO << "Device name given by the adapter " << value
              << ", has been assigned to cfg " << m_deviceName;
              m_deviceName = value;
            }
            else
            {
              g_logger << LERROR << "Cannot find device for device command: " << value;
              throw std::invalid_argument(string("Cannot find device for device name or uuid: ") +
                                          value);
            }
          }
          else
          {
            g_logger << LWARN << "Unknown command '" << data << "' for device '" << m_deviceName;
          }
          
          if (deviceChanged)
          {
            m_agent->deviceChanged(m_device, oldUuid, oldName);
          }
        }
      }
    }
    
    void Adapter::parseCalibration(const std::string &aLine)
    {
      istringstream toParse(aLine);
      
      // Look for name|factor|offset triples
      string name, factor, offset;
      while (getline(toParse, name, '|') && getline(toParse, factor, '|') &&
             getline(toParse, offset, '|'))
      {
        // Convert to a floating point number
        auto di = m_device->getDeviceDataItem(name);
        if (!di)
          g_logger << LWARN << "Cannot find data item to calibrate for " << name;
        else
        {
          double fact_value = strtod(factor.c_str(), nullptr);
          double off_value = strtod(offset.c_str(), nullptr);
          di->setConversionFactor(fact_value, off_value);
        }
      }
    }
    
    void Adapter::disconnected()
    {
      m_baseTime = 0;
      m_agent->disconnected(this, m_allDevices);
    }
    
    void Adapter::connecting() { m_agent->connecting(this); }
    
    void Adapter::connected() { m_agent->connected(this, m_allDevices); }

#endif
  }  // namespace adapter
}  // namespace mtconnect
