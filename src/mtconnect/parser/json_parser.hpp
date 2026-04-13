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

#pragma once

#include <list>
#include <optional>
#include <string>

#include "mtconnect/config.hpp"
#include "mtconnect/device_model/device.hpp"

/// @brief MTConnect Device parser namespace
namespace mtconnect::parser {
  /// @brief parse a JSON document and create a list of devices
  class AGENT_LIB_API JsonParser
  {
  public:
    /// @brief Constructor
    /// @param version the JSON serialization version (1 or 2)
    JsonParser(uint32_t version = 1) : m_version(version) {}
    virtual ~JsonParser() = default;

    /// @brief Parses a JSON file containing an MTConnectDevices document and returns a list of
    /// devices
    /// @param[in] filePath the path to the JSON file
    /// @returns a list of device pointers
    std::list<device_model::DevicePtr> parseFile(const std::string &filePath);

    /// @brief Parses a JSON string containing an MTConnectDevices document and returns a list of
    /// devices. Navigates MTConnectDevices/Devices to find device nodes.
    /// @param[in] jsonDoc the JSON document string
    /// @returns a list of device pointers
    std::list<device_model::DevicePtr> parseDocument(const std::string &jsonDoc);

    /// @brief Parses a JSON string containing a single device and returns the device
    /// @param[in] jsonDoc the JSON document string wrapping a single Device
    /// @returns a shared device pointer if successful
    device_model::DevicePtr parseDevice(const std::string &jsonDoc)
    {
      return parseDevice(jsonDoc, m_version);
    }

    /// @brief Parses a JSON string containing a single device and returns the device
    /// @param[in] jsonDoc the JSON document string wrapping a single Device
    /// @param[in] version the JSON serialization version to use
    /// @returns a shared device pointer if successful
    device_model::DevicePtr parseDevice(const std::string &jsonDoc, uint32_t version);

    /// @brief get the schema version parsed from the document
    /// @return the version if found
    const auto &getSchemaVersion() const { return m_schemaVersion; }

  protected:
    uint32_t m_version;
    std::optional<std::string> m_schemaVersion;
  };
}  // namespace mtconnect::parser
