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

#include <date/tz.h>
#include <optional>

using namespace std;

namespace mtconnect
{
  namespace adapter
  {

    static dlib::logger g_logger("ShdrParser");

    const auto EXP = "^(" R"RE("(([^"\\\|]*(\\\|)?)+)")RE" "|"
                          R"RE(([^|]*))RE" R"RE()(\||$))RE";
    std::regex ShdrTokenizer::m_pattern(EXP,
                                        std::regex::optimize |
                                        std::regex::ECMAScript);

    inline static std::pair<std::string,std::optional<std::string>> splitKey(const std::string &key)
    {
      auto c = key.find(':');
      if (c != string::npos)
        return { key.substr(c + 1, string::npos), key.substr(0, c - 1) };
      else
        return {key, nullopt};
    }
    
    inline optional<double> getDuration(std::string &timestamp)
    {
      optional<double> duration;

      auto pos = timestamp.find('@');
      if (pos != string::npos)
      {
        auto read = pos + 1;;
        duration = std::stod(timestamp, &read);
        if (read == pos + 1)
          duration.reset();
        timestamp = timestamp.erase(pos);
      }
      
      return duration;
    }
    
    // --------------------------------------
    // Mapping to data items
    
    entity::Requirements DataItemMapper::m_condition {
      { "level", true },
      { "nativeCode", false },
      { "nativeSeverity", false },
      { "qualifier", false },
      { "value", false }
    };
    entity::Requirements DataItemMapper::m_timeseries {
      { "count", entity::INTEGER, true },
      { "frequency", entity::DOUBLE, true },
      { "value", entity::VECTOR, true }
    };
    entity::Requirements DataItemMapper::m_message {
      { "nativeCode", false },
      { "value", false }
    };
    entity::Requirements DataItemMapper::m_asset {
      { "assetId", false },
      { "type", false }
    };
    entity::Requirements DataItemMapper::m_sample {
      { "value", entity::DOUBLE, false },
    };
    entity::Requirements DataItemMapper::m_event {
      { "value", false },
    };
    
    void TimeStampExtractor::extractTimestamp(ShdrObservation &obs,
                                              TokenList::const_iterator &token,
                                              const TokenList::const_iterator &end,
                                              Context &context)
    {
      using namespace date;
      
      const string &timestamp = *token++;
      Timestamp now = context.m_now ? context.m_now() : chrono::system_clock::now();
      if (context.m_ignoreTimestamps || timestamp.empty())
      {
        obs.m_timestamp = now;
        return;
      }
      
      Timestamp ts;
      bool has_t{ timestamp.find('T') != string::npos };
      if (has_t)
      {
        istringstream in(timestamp);
        in >> std::setw(6) >> parse("%FT%T", ts);
        if (!context.m_relativeTime)
        {
          obs.m_timestamp = ts;
          return;
        }
      }
      
      // Handle double offset
      double offset;
      if (!has_t)
      {
        offset = stod(timestamp);
      }
      
      if (!context.m_base)
      {
        context.m_base = now;
        if (has_t)
          context.m_offset = now - ts;
        else
          context.m_offset = Micros(int64_t(offset * 1000.0));
        obs.m_timestamp = now;
      }
      else
      {
        if (has_t)
        {
          obs.m_timestamp = ts + context.m_offset;
        }
        else
        {
          obs.m_timestamp = *context.m_base +
            Micros(int64_t(offset * 1000.0)) - context.m_offset;
        }
      }
    }

    
    void DataItemMapper::mapTokensToAsset(ShdrObservation &obs,
                                       TokenList::const_iterator &token,
                                       const TokenList::const_iterator &end)
    {
      
    }
    
    inline void zipProperties(entity::Properties &properties,
                              const entity::Requirements &reqs,
                              TokenList::const_iterator &token,
                              const TokenList::const_iterator &end)
    {
      auto req = reqs.begin();
      while (token != end && req != reqs.end())
      {
        entity::Value value(*token);
        try
        {
          if (req->convertType(value))
          {
            properties.insert_or_assign(*token, value);
          }
          else
          {
            g_logger << dlib::LWARN << "Cannot convert value for: " << *token;
          }
        }
        catch (entity::PropertyError &e)
        {
          g_logger << dlib::LWARN << "Cannot convert value for: " << *token
                    << " - " << e.what();
        }
        
        token++;
        req++;
      }
    }
    
    void DataItemMapper::mapTokensToDataItems(ShdrObservation &obs,
                                         TokenList::const_iterator &token,
                                         const TokenList::const_iterator &end,
                                         Context &context)
    {
      // Asset maping
      if ((*token)[0] == '@')
      {
        mapTokensToAsset(obs, token, end);
        return;
      }

      ShdrDataItemObservation observation{obs};
      
      while (token != end)
      {
        string deviceId;
        auto dataItemKey = splitKey(*token++);
        observation.m_device = context.m_getDevice(dataItemKey.second.value_or(""));
        observation.m_dataItem = context.m_getDataItem(observation.m_device, dataItemKey.first);
        
        if (observation.m_dataItem == nullptr)
        {
          // resync to next item
          if (context.m_logOnce.count(dataItemKey.first) > 0)
            g_logger << LTRACE << "(" << observation.m_device->getName()
                     << ") Could not find data item: " << dataItemKey.first;
          else
          {
            g_logger << LWARN << "(" << observation.m_device->getName()
                     << ") Could not find data item: " << dataItemKey.first;
            context.m_logOnce.insert(dataItemKey.first);
          }

          continue;
        }
        
        entity::Requirements *reqs { nullptr };
        
        // Extract the remaining tokens
        if (observation.m_dataItem->getCategory() == DataItem::SAMPLE)
        {
          if (observation.m_dataItem->getRepresentation() == DataItem::TIME_SERIES)
          {
            reqs = &m_timeseries;
          }
          else
          {
            reqs = &m_sample;
          }
        }
        else if (observation.m_dataItem->getCategory() == DataItem::EVENT)
        {
          if (observation.m_dataItem->getType() == "MESSAGE")
          {
            reqs = &m_message;
          }
          else
          {
            reqs = &m_event;
          }
        }
        else if (observation.m_dataItem->getCategory() == DataItem::CONDITION)
        {
          reqs = &m_condition;
        }
        
        if (reqs != nullptr)
          zipProperties(observation.m_properties, *reqs, token, end);
        else
          g_logger << LWARN << "Cannot find requirements for " << dataItemKey.first;
      }
    }
    
#if 0
    
    
    inline string Adapter::extractTime(const string &time, double &anOffset)
    {
      // Check how to handle time. If the time is relative, then we need to compute the first
      // offsets, otherwise, if this function is being used as an API, add the current time.
      string result;
      if (m_relativeTime)
      {
        uint64_t offset;
        
        if (!m_baseTime)
        {
          m_baseTime = getCurrentTimeInMicros();
          
          if (time.find('T') != string::npos)
          {
            m_parseTime = true;
            m_baseOffset = parseTimeMicro(time);
          }
          else
            m_baseOffset = (uint64_t)(atof(time.c_str()) * 1000.0);
          
          offset = 0;
        }
        else if (m_parseTime)
          offset = parseTimeMicro(time) - m_baseOffset;
        else
          offset = ((uint64_t)(atof(time.c_str()) * 1000.0)) - m_baseOffset;
        
        // convert microseconds to seconds
        anOffset = offset / 1000000.0;
        result = getRelativeTimeString(m_baseTime + offset);
      }
      else if (m_ignoreTimestamps || time.empty())
      {
        anOffset = getCurrentTimeInSec();
        result = getCurrentTime(GMT_UV_SEC);
      }
      else
      {
        anOffset = parseTimeMicro(time) / 1000000.0;
        result = time;
      }
      
      return result;
    }
        
    bool Adapter::processDataItem(istringstream &toParse, const string &line, const string &inputKey,
                                  const string &inputValue, const string &time, double anOffset,
                                  bool first)
    {
      string dev, key = inputKey;
      Device *device(nullptr);
      DataItem *dataItem(nullptr);
      bool more = true;
      
      if (splitKey(key, dev))
        device = m_agent->getDeviceByName(dev);
      else
      {
        dev = m_deviceName;
        device = m_device;
      }
      
      if (device)
      {
        dataItem = device->getDeviceDataItem(key);
        
        if (!dataItem)
        {
          if (m_logOnce.count(key) > 0)
            g_logger << LTRACE << "(" << device->getName() << ") Could not find data item: " << key;
          else
          {
            g_logger << LWARN << "(" << device->getName() << ") Could not find data item: " << key
            << " from line '" << line << "'";
            m_logOnce.insert(key);
          }
        }
        else if (dataItem->hasConstantValue())
        {
          if (!m_logOnce.count(key))
          {
            g_logger << LDEBUG << "(" << device->getName() << ") Ignoring value for: " << key
            << ", constant value";
            m_logOnce.insert(key);
          }
        }
        else
        {
          string rest, value;
          if (first && (dataItem->isCondition() || dataItem->isAlarm() || dataItem->isMessage() ||
                        dataItem->isTimeSeries()))
          {
            getline(toParse, rest);
            value = inputValue + "|" + rest;
            if (!rest.empty())
              value = inputValue + "|" + rest;
            else
              value = inputValue;
            more = false;
          }
          else
          {
            if (m_upcaseValue && !dataItem->isDataSet())
            {
              value.resize(inputValue.length());
              transform(inputValue.begin(), inputValue.end(), value.begin(), ::toupper);
            }
            else
              value = inputValue;
          }
          
          dataItem->setDataSource(this);
          
          trim(value);
          string check = value;
          
          if (dataItem->hasResetTrigger())
          {
            auto found = value.find_first_of(':');
            if (found != string::npos)
              check.erase(found);
          }
          
          if (!isDuplicate(dataItem, check, anOffset))
            m_agent->addToBuffer(dataItem, value, time);
          else if (m_dupCheck)
            g_logger << LTRACE << "Dropping duplicate value for " << key << " of " << value;
        }
      }
      else
      {
        g_logger << LDEBUG << "Could not find device: " << dev;
        // Continue on processing the rest of the fields. Assume key/value pairs...
      }
      
      return more;
    }
    
    void Adapter::processAsset(istringstream &toParse, const string &inputKey, const string &value,
                               const string &time)
    {
      string key, dev;
      try
      {
        Device *device(nullptr);
        key = inputKey;
        if (splitKey(key, dev))
          device = m_agent->getDeviceByName(dev);
        else
        {
          device = m_device;
          dev = device->getUuid();
        }
        
        if (device == nullptr)
        {
          g_logger << LERROR << "Cannot find device: " << dev << " for asset request";
          return;
        }
        
        string assetId;
        if (value[0] == '@')
          assetId = device->getUuid() + value.substr(1);
        else
          assetId = value;
        
        if (key == "@ASSET@")
        {
          string type, rest;
          getline(toParse, type, '|');
          getline(toParse, rest);
          
          // Chck for an update and parse key value pairs. If only a type
          // is presented, then assume the remainder is a complete doc.
          
          // if the rest of the line begins with --multiline--... then
          // set multiline and accumulate until a completed document is found
          if (rest.find("--multiline--") != rest.npos)
          {
            m_assetDevice = device;
            m_gatheringAsset = true;
            m_terminator = rest;
            m_time = time;
            m_assetType = type;
            m_assetId = assetId;
            m_body.str("");
            m_body.clear();
          }
          else
          {
            entity::ErrorList errors;
            m_agent->addAsset(device, rest, assetId, type, time, errors);
          }
        }
        else if (key == "@UPDATE_ASSET@")
        {
          string assetKey, assetValue;
          AssetChangeList list;
          getline(toParse, assetKey, '|');
          if (assetKey[0] == '<')
          {
            do
            {
              pair<string, string> kv("xml", assetKey);
              list.emplace_back(kv);
            } while (getline(toParse, assetKey, '|'));
          }
          else
          {
            while (getline(toParse, assetValue, '|'))
            {
              pair<string, string> kv(assetKey, assetValue);
              list.emplace_back(kv);
              
              if (!getline(toParse, assetKey, '|'))
                break;
            }
          }
          
          // m_agent->updateAsset(device, assetId, list, time);
        }
        else if (key == "@REMOVE_ASSET@")
          m_agent->removeAsset(device, assetId, time);
        else if (key == "@REMOVE_ALL_ASSETS@")
        {
          AssetList list;
          m_agent->removeAllAssets(device->getUuid(), value, time, list);
        }
      }
      catch (std::logic_error &e)
      {
        g_logger << LERROR << "Asset request: " << key << " failed: " << e.what();
      }
    }
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
  }
}
