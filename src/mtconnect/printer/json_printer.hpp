//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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
        const ProtoErrorList &list, bool pretty = false,
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

  protected:
    std::string m_version;
    std::string m_hostname;
    uint32_t m_jsonVersion;
  };
}  // namespace mtconnect::printer
