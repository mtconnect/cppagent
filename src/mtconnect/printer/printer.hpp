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

#include <list>
#include <map>
#include <string>
#include <vector>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/utilities.hpp"
#include "mtconnect/version.h"

namespace mtconnect {
  namespace device_model {
    class Device;
  }
  namespace asset {
    class Asset;
    class CuttingTool;
  }  // namespace asset

  /// @brief MTConnect abstract document generation namespace
  namespace printer {
    using DevicePtr = std::shared_ptr<device_model::Device>;

    using ProtoErrorList = std::list<std::pair<std::string, std::string>>;

    /// @brief Abstract document generator interface
    class AGENT_LIB_API Printer
    {
    public:
      /// @brief construct a printer
      /// @param pretty `true` if content should be pretty printed
      Printer(bool pretty = false, bool validation = false)
        : m_pretty(pretty), m_validation(validation)
      {}
      virtual ~Printer() = default;

      /// @brief Generate an MTConnect Error document
      /// @param[in] instanceId the instance id
      /// @param[in] bufferSize the buffer size
      /// @param[in] nextSeq the next sequence
      /// @param[in] errorCode an error code
      /// @param[in] errorText the error text
      /// @return the error document
      virtual std::string printError(
          const uint64_t instanceId, const unsigned int bufferSize, const uint64_t nextSeq,
          const std::string &errorCode, const std::string &errorText, bool pretty = false,
          const std::optional<std::string> requestId = std::nullopt) const
      {
        return printErrors(instanceId, bufferSize, nextSeq, {{errorCode, errorText}});
      }
      /// @brief Generate an MTConnect Error document with a error list
      /// @param[in] instanceId the instance id
      /// @param[in] bufferSize the buffer size
      /// @param[in] nextSeq the next sequence
      /// @param[in] list the list of errors
      /// @return the MTConnect Error document
      virtual std::string printErrors(
          const uint64_t instanceId, const unsigned int bufferSize, const uint64_t nextSeq,
          const ProtoErrorList &list, bool pretty = false,
          const std::optional<std::string> requestId = std::nullopt) const = 0;
      /// @brief Generate an MTConnect Devices document
      /// @param[in] instanceId the instance id
      /// @param[in] bufferSize the buffer size
      /// @param[in] nextSeq the next sequence
      /// @param[in] assetBufferSize the asset buffer size
      /// @param[in] assetCount the asset count
      /// @param[in] devices a list of devices
      /// @param[in] count optional asset count and type association
      /// @return the MTConnect Devices document
      virtual std::string printProbe(
          const uint64_t instanceId, const unsigned int bufferSize, const uint64_t nextSeq,
          const unsigned int assetBufferSize, const unsigned int assetCount,
          const std::list<DevicePtr> &devices, const std::map<std::string, size_t> *count = nullptr,
          bool includeHidden = false, bool pretty = false,
          const std::optional<std::string> requestId = std::nullopt) const = 0;
      /// @brief Print a MTConnect Streams document
      /// @param[in] instanceId the instance id
      /// @param[in] bufferSize the buffer size
      /// @param[in] nextSeq the next sequence
      /// @param[in] firstSeq the first sequence
      /// @param[in] lastSeq the last sequnce
      /// @param[in] results a list of observations
      /// @return the MTConnect Streams document
      virtual std::string printSample(
          const uint64_t instanceId, const unsigned int bufferSize, const uint64_t nextSeq,
          const uint64_t firstSeq, const uint64_t lastSeq, observation::ObservationList &results,
          bool pretty = false, const std::optional<std::string> requestId = std::nullopt) const = 0;
      /// @brief Generate an MTConnect Assets document
      /// @param[in] anInstanceId the instance id
      /// @param[in] bufferSize the buffer size
      /// @param[in] assetCount the asset count
      /// @param[in] asset the list of assets
      /// @return the MTConnect Assets document
      virtual std::string printAssets(
          const uint64_t anInstanceId, const unsigned int bufferSize, const unsigned int assetCount,
          asset::AssetList const &asset, bool pretty = false,
          const std::optional<std::string> requestId = std::nullopt) const = 0;
      /// @brief get the mime type for the documents
      /// @return the mime type
      virtual std::string mimeType() const = 0;
      /// @brief Set the last model change time
      /// @param t the time
      void setModelChangeTime(const std::string &t) { m_modelChangeTime = t; }
      /// @brief Get the last model change time
      /// @return the time
      const std::string &getModelChangeTime() { return m_modelChangeTime; }

      /// @brief set the schema version we are generating
      /// @param s the version
      void setSchemaVersion(const std::string &s) { m_schemaVersion = s; }
      /// @brief Get the schema version
      /// @return the schema version
      const auto &getSchemaVersion() const { return m_schemaVersion; }

      /// @brief sets the sener name for the header
      /// @param name the name of the sender
      void setSenderName(const std::string &s) { m_senderName = s; }

      /// @brief gets the sender name
      /// @returns the name of the sender in the header
      const auto &getSenderName() const { return m_senderName; }

      /// @brief Use the agent version to default the schema version
      void defaultSchemaVersion() const
      {
        if (!m_schemaVersion)
        {
          std::string ver =
              std::to_string(AGENT_VERSION_MAJOR) + "." + std::to_string(AGENT_VERSION_MINOR);
          const_cast<Printer *>(this)->m_schemaVersion.emplace(ver);
        }
      }

      /// @brief Get validation header flag state
      /// @returns validation state
      bool getValidation() const { return m_validation; }

      /// @brief sets validation state
      /// @param validation the validation state
      void setValidation(bool v) { m_validation = v; }

    protected:
      bool m_pretty;      //< Turns pretty printing on
      bool m_validation;  //< Sets validation flag in header
      std::string m_modelChangeTime;
      std::optional<std::string> m_schemaVersion;
      std::string m_senderName {"localhost"};
    };
  }  // namespace printer
}  // namespace mtconnect
