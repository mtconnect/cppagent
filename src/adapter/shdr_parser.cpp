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
      using namespace mtconnect::entity;
      
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

            // TODO: Check if the timestamp should be passed as a chrono
            // Send downstream
            auto obs = get<AssetObservation>(observation.m_observed);
            auto timestamp = date::format("%FT%TZ", observation.m_timestamp);
            if (m_assetHandler)
              m_assetHandler(observation.m_device, obs.m_body,
                             OptionallyGet<string>("assetId", observation.m_properties),
                             OptionallyGet<string>("type", observation.m_properties),
                             timestamp, errors);
            else
              g_logger << dlib::LWARN << "Asset handler was not provided";
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

            
            auto out =
            Observation2::makeObservation(obs.m_dataItem, observation.m_properties,
                                          observation.m_timestamp, errors);
            if (errors.empty())
            {
              if (obs.m_unavailable)
                out->makeUnavailable();
              
              out->setTimestamp(timestamp.m_timestamp);
              
              // Deliver
              if (m_observationHandler)
                m_observationHandler(out);
              else
                g_logger << dlib::LWARN << "Observation handler was not provided";
            }
          }
          // Continue on and try to recover... log errors first.
          for (auto &e : errors)
          {
            g_logger << dlib::LWARN << "Error while parsing tokens: " << e->what();
            for (auto it = start; it != token; it++)
              g_logger << dlib::LWARN << "    token: " << *it;
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
    
    static inline void parseCalibration(Device *device, const std::string &aLine)
    {
      istringstream toParse(aLine);
      
      // Look for name|factor|offset triples
      string name, factor, offset;
      while (getline(toParse, name, '|') && getline(toParse, factor, '|') &&
             getline(toParse, offset, '|'))
      {
        // Convert to a floating point number
        auto di = device->getDeviceDataItem(name);
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
    
    static inline bool is_true(const string &aValue)
    {
      return (aValue == "yes" || aValue == "true" || aValue == "1");
    }

    void ShdrParser::processCommand(const std::string &data, Context &context)
    {
      auto device = const_cast<Device*>(context.m_getDevice(context.m_defaultDevice));
      
      // Handle initial push of settings for uuid, serial number and manufacturer.
      // This will override the settings in the device from the xml
      if (data == "* PROBE")
      {
        //      const Printer *printer = m_agent->getPrinter("xml");
        //      string response = m_agent->handleProbe(printer, deviceName);
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
          
          static auto deviceCommands = std::map<string,std::function<void(Device*,const string&)>>({
            { "uuid", [](Device *d,const string &v) { d->setUuid(v); } },
            { "manufacturer", [](Device *d,const string &v) {d->setManufacturer(v); } },
            { "station", [](Device *d,const string &v) {d->setManufacturer(v); } },
            { "serialNumber", [](Device *d,const string &v) {d->setStation(v); } },
            { "description", [](Device *d,const string &v) {d->setDescription(v); } },
            { "nativeName", [](Device *d,const string &v) {d->setNativeName(v); } },
            { "calibration", [](Device *d,const string &v) { parseCalibration(d, v); } },
          });
          
          auto s = deviceCommands.find(key);
          if (s != deviceCommands.end())
          {
            if (device != nullptr)
            {
              auto oldName = device->getName();
              auto oldUuid = device->getUuid();

              s->second(device, value);
              if (context.m_deviceChanged)
                context.m_deviceChanged(device, oldUuid, oldName);
              else
                g_logger << LWARN << "No function registered for device changed";
            }
            else
            {
              g_logger << LWARN << "Device command '" << key << "' cannot be performed without a default device";
            }
          }
          else if (key == "conversionRequired")
            context .m_conversionRequired = is_true(value);
          else if (key == "relativeTime")
            context.m_relativeTime = is_true(value);
          else if (key == "realTime")
            context.m_realTime = is_true(value);
          // TODO: Set default device
          else if (key == "device")
          {
            auto device = const_cast<Device*>(context.m_getDevice(value));
            if (device)
            {
              g_logger << LINFO << "Device name given by the adapter " << value
              << ", has been assigned to cfg ";
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
            g_logger << LWARN << "Unknown command '" << data;
          }
        }
      }
    }
  }  // namespace adapter
}  // namespace mtconnect
