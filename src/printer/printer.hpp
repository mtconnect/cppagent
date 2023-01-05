//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include <map>
#include <string>
#include <vector>

#include "asset/asset.hpp"
#include "observation/observation.hpp"
#include "utilities.hpp"
#include "version.h"

namespace mtconnect {
  namespace device_model {
    class Device;
  }
  namespace asset {
    class Asset;
    class CuttingTool;
  }  // namespace asset

  namespace printer {
    using DevicePtr = std::shared_ptr<device_model::Device>;

    using ProtoErrorList = std::list<std::pair<std::string, std::string>>;

    class Printer
    {
    public:
      Printer(bool pretty = false) : m_pretty(pretty) {}
      virtual ~Printer() = default;

      virtual std::string printError(const uint64_t instanceId, const unsigned int bufferSize,
                                     const uint64_t nextSeq, const std::string &errorCode,
                                     const std::string &errorText) const
      {
        return printErrors(instanceId, bufferSize, nextSeq, {{errorCode, errorText}});
      }
      virtual std::string printErrors(const uint64_t instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const ProtoErrorList &list) const = 0;

      virtual std::string printProbe(
          const uint64_t instanceId, const unsigned int bufferSize, const uint64_t nextSeq,
          const unsigned int assetBufferSize, const unsigned int assetCount,
          const std::list<DevicePtr> &devices,
          const std::map<std::string, size_t> *count = nullptr) const = 0;

      virtual std::string printSample(const uint64_t instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const uint64_t firstSeq,
                                      const uint64_t lastSeq,
                                      observation::ObservationList &results) const = 0;
      virtual std::string printAssets(const uint64_t anInstanceId, const unsigned int bufferSize,
                                      const unsigned int assetCount,
                                      asset::AssetList const &asset) const = 0;
      virtual std::string mimeType() const = 0;

      void setModelChangeTime(const std::string &t) { m_modelChangeTime = t; }
      const std::string &getModelChangeTime() { return m_modelChangeTime; }

      void setSchemaVersion(const std::string &s) { m_schemaVersion = s; }
      const auto &getSchemaVersion() const { return m_schemaVersion; }

      void defaultSchemaVersion() const
      {
        if (!m_schemaVersion)
        {
          std::string ver =
              std::to_string(AGENT_VERSION_MAJOR) + "." + std::to_string(AGENT_VERSION_MINOR);
          const_cast<Printer *>(this)->m_schemaVersion.emplace(ver);
        }
      }

    protected:
      bool m_pretty;
      std::string m_modelChangeTime;
      std::optional<std::string> m_schemaVersion;
    };
  }  // namespace printer
}  // namespace mtconnect
