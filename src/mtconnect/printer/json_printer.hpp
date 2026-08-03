//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/asset/cutting_tool.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/printer/printer.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::printer {
  /// @brief Printer to generate JSON Documents
  class AGENT_LIB_API JsonPrinter : public Printer
  {
  public:
    JsonPrinter(uint32_t jsonVersion, bool pretty = false, bool validation = false);
    ~JsonPrinter() override = default;

    std::string printErrors(
        const uint64_t instanceId, const unsigned int bufferSize, const uint64_t nextSeq,
        const entity::EntityList &list, bool pretty = false,
        const std::optional<std::string> requestId = std::nullopt) const override;

    std::string printProbe(
        const uint64_t instanceId, const unsigned int bufferSize, const uint64_t nextSeq,
        const unsigned int assetBufferSize, const unsigned int assetCount,
        const std::list<DevicePtr> &devices, const std::map<std::string, size_t> *count = nullptr,
        bool includeHidden = false, bool pretty = false,
        const std::optional<std::string> requestId = std::nullopt) const override;

    std::string printSample(
        const uint64_t instanceId, const unsigned int bufferSize, const uint64_t nextSeq,
        const uint64_t firstSeq, const uint64_t lastSeq, observation::ObservationList &results,
        bool pretty = false,
        const std::optional<std::string> requestId = std::nullopt) const override;
    std::string printAssets(
        const uint64_t anInstanceId, const unsigned int bufferSize, const unsigned int assetCount,
        const asset::AssetList &asset, bool pretty = false,
        const std::optional<std::string> requestId = std::nullopt) const override;
    std::string mimeType() const override { return "application/mtconnect+json"; }

    uint32_t getJsonVersion() const { return m_jsonVersion; }

    /// @brief Add JSON Schema location for Devices Document
    /// @param url The url referencing the schema
    void setDevicesSchema(const std::string &url) { m_devicesSchema = url; }

    /// @brief Add JSON Schema location for Streams Document
    /// @param url The url referencing the schema
    void setStreamsSchema(const std::string &url) { m_streamsSchema = url; }

    /// @brief Add JSON Schema location for Assets Document
    /// @param url The url referencing the schema
    void setAssetsSchema(const std::string &url) { m_assetsSchema = url; }

    /// @brief Add JSON Schema location for Errors Document
    /// @param url The url referencing the schema
    /// @param location the file location of the schema file
    void setErrorSchema(const std::string &url) { m_errorSchema = url; }

    /// @brief Get the JSON Schema url for the Devices Document
    /// @returns The url if the schema is set, otherwise `std::nullopt`
    const auto &getDevicesSchema() const { return m_devicesSchema; }

    /// @brief Get the JSON Schema url for the Streams Document
    /// @returns The url if the schema is set, otherwise `std::nullopt`
    const auto &getStreamsSchema() const { return m_streamsSchema; }

    /// @brief Get the JSON Schema url for the Assets Document
    /// @returns The url if the schema is set, otherwise `std::nullopt`
    const auto &getAssetsSchema() const { return m_assetsSchema; }

    /// @brief Get the JSON Schema url for the Error Document
    /// @returns The url if the schema is set, otherwise `std::nullopt`
    const auto &getErrorSchema() const { return m_errorSchema; }

  protected:
    std::optional<std::string> m_devicesSchema;
    std::optional<std::string> m_streamsSchema;
    std::optional<std::string> m_assetsSchema;
    std::optional<std::string> m_errorSchema;

    std::string m_version;
    std::string m_hostname;
    uint32_t m_jsonVersion;
  };
}  // namespace mtconnect::printer
