//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology ("AMT")
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

#include "json_parser.hpp"

#include <fstream>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "mtconnect/device_model/device.hpp"
#include "mtconnect/entity/json_parser.hpp"
#include "mtconnect/logging.hpp"

using namespace std;
namespace rj = ::rapidjson;

namespace mtconnect::parser {
  using namespace device_model;

  list<DevicePtr> JsonParser::parseFile(const string &filePath)
  {
    NAMED_SCOPE("json.parser");

    ifstream ifs(filePath);
    if (!ifs.is_open())
    {
      LOG(fatal) << "Cannot open JSON file: " << filePath;
      throw FatalException("Cannot open JSON file: " + filePath);
    }

    stringstream buffer;
    buffer << ifs.rdbuf();
    return parseDocument(buffer.str());
  }

  list<DevicePtr> JsonParser::parseDocument(const string &jsonDoc)
  {
    NAMED_SCOPE("json.parser");

    list<DevicePtr> deviceList;

    rj::Document doc;
    doc.Parse(jsonDoc.c_str());

    if (doc.HasParseError())
    {
      LOG(fatal) << "JSON parse error: " << rj::GetParseError_En(doc.GetParseError()) << " at offset "
                 << doc.GetErrorOffset();
      throw FatalException("Cannot parse JSON document");
    }

    if (!doc.IsObject())
    {
      LOG(fatal) << "JSON document root is not an object";
      throw FatalException("JSON document root is not an object");
    }

    // Navigate to MTConnectDevices
    auto mtcIt = doc.FindMember("MTConnectDevices");
    if (mtcIt == doc.MemberEnd() || !mtcIt->value.IsObject())
    {
      LOG(fatal) << "JSON document does not contain MTConnectDevices";
      throw FatalException("JSON document does not contain MTConnectDevices");
    }

    const auto &mtcDevices = mtcIt->value;

    // Use document jsonVersion if present, otherwise fall back to the default
    uint32_t version = m_version;
    auto jsonVersionIt = mtcDevices.FindMember("jsonVersion");
    if (jsonVersionIt != mtcDevices.MemberEnd() && jsonVersionIt->value.IsUint())
    {
      version = jsonVersionIt->value.GetUint();
    }

    // Extract schema version if present
    auto schemaIt = mtcDevices.FindMember("schemaVersion");
    if (schemaIt != mtcDevices.MemberEnd() && schemaIt->value.IsString())
    {
      m_schemaVersion.emplace(schemaIt->value.GetString(), schemaIt->value.GetStringLength());
    }

    // Navigate to Devices
    auto devicesIt = mtcDevices.FindMember("Devices");
    if (devicesIt == mtcDevices.MemberEnd())
    {
      LOG(warning) << "No Devices in JSON document – expecting dynamic configuration";
      return deviceList;
    }

    const auto &devices = devicesIt->value;

    if (version == 1)
    {
      // V1: Devices is an array of objects like [{"Device": {...}}, ...]
      if (!devices.IsArray())
      {
        LOG(fatal) << "V1 Devices is not an array";
        throw FatalException("V1 Devices is not an array");
      }

      for (rj::SizeType i = 0; i < devices.Size(); ++i)
      {
        const auto &item = devices[i];
        if (item.IsObject() && item.MemberCount() > 0)
        {
          // Re-serialize the single device wrapper object for the entity parser
          rj::StringBuffer sb;
          rj::Writer<rj::StringBuffer> writer(sb);
          item.Accept(writer);

          auto device = parseDevice(string(sb.GetString(), sb.GetLength()), version);
          if (device)
            deviceList.emplace_back(device);
          else
            LOG(error) << "Failed to parse device from JSON array element " << i;
        }
      }
    }
    else if (version == 2)
    {
      // V2: Devices is an object like {"Device": [{...}, ...]}
      if (!devices.IsObject())
      {
        LOG(fatal) << "V2 Devices is not an object";
        throw FatalException("V2 Devices is not an object");
      }

      auto deviceArrayIt = devices.FindMember("Device");
      if (deviceArrayIt != devices.MemberEnd() && deviceArrayIt->value.IsArray())
      {
        for (rj::SizeType i = 0; i < deviceArrayIt->value.Size(); ++i)
        {
          const auto &deviceObj = deviceArrayIt->value[i];
          // Wrap as {"Device": {...}} for the entity parser
          rj::StringBuffer sb;
          rj::Writer<rj::StringBuffer> writer(sb);
          writer.StartObject();
          writer.Key("Device");
          deviceObj.Accept(writer);
          writer.EndObject();

          auto device = parseDevice(string(sb.GetString(), sb.GetLength()), version);
          if (device)
            deviceList.emplace_back(device);
          else
            LOG(error) << "Failed to parse device from JSON v2 array element " << i;
        }
      }
    }

    return deviceList;
  }

  DevicePtr JsonParser::parseDevice(const std::string &jsonDoc, uint32_t version)
  {
    NAMED_SCOPE("json.parser");

    DevicePtr device;

    try
    {
      entity::ErrorList errors;
      entity::JsonParser parser(version);
      auto entity = parser.parse(Device::getRoot(), jsonDoc, "2.3", errors);

      if (!errors.empty())
      {
        for (auto &e : errors)
        {
          LOG(warning) << "Error parsing JSON Device: " << e->what();
        }
      }

      if (entity)
      {
        device = dynamic_pointer_cast<Device>(entity);
      }
      else
      {
        LOG(error) << "Failed to parse JSON device document";
      }
    }
    catch (const exception &e)
    {
      LOG(fatal) << "Cannot parse JSON document: " << e.what();
      throw FatalException(e.what());
    }

    return device;
  }
}  // namespace mtconnect::parser
