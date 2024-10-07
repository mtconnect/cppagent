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

#include <unordered_set>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/printer/printer.hpp"
#include "mtconnect/utilities.hpp"

extern "C"
{
  using xmlTextWriter = struct _xmlTextWriter;
  using xmlTextWriterPtr = xmlTextWriter *;
}

namespace mtconnect {
  class SensorConfiguration;

  namespace printer {
    class XmlWriter;

    /// @brief Printer to generate XML Documents
    class AGENT_LIB_API XmlPrinter : public Printer
    {
    public:
      XmlPrinter(bool pretty = false, bool validation = false);
      ~XmlPrinter() override = default;

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
      std::string mimeType() const override { return "text/xml"; }

      /// @brief Print a single device in XML
      /// @param device A device poiinter
      /// @return The devices XML
      std::string printDevice(DevicePtr device, bool pretty = false);

      /// @brief Add a Devices XML device namespace
      /// @param urn the namespace URN
      /// @param location the location of the schema file
      /// @param prefix the namespace prefix
      void addDevicesNamespace(const std::string &urn, const std::string &location,
                               const std::string &prefix);
      /// @brief Add a Error XML device namespace
      /// @param urn the namespace URN
      /// @param location the location of the schema file
      /// @param prefix the namespace prefix
      void addErrorNamespace(const std::string &urn, const std::string &location,
                             const std::string &prefix);
      /// @brief Add a Streams XML device namespace
      /// @param urn the namespace URN
      /// @param location the location of the schema file
      /// @param prefix the namespace prefix
      void addStreamsNamespace(const std::string &urn, const std::string &location,
                               const std::string &prefix);
      /// @brief Add a Assets XML device namespace
      /// @param urn the namespace URN
      /// @param location the location of the schema file
      /// @param prefix the namespace prefix
      void addAssetsNamespace(const std::string &urn, const std::string &location,
                              const std::string &prefix);

      /// @brief Set the Devices style sheet to add as a processing instruction
      /// @param style the stype sheet
      void setDevicesStyle(const std::string &style);
      /// @brief Set the Streams style sheet to add as a processing instruction
      /// @param style the stype sheet
      void setStreamStyle(const std::string &style);
      /// @brief Set the Assets style sheet to add as a processing instruction
      /// @param style the stype sheet
      void setAssetsStyle(const std::string &style);
      /// @brief Set the Error style sheet to add as a processing instruction
      /// @param style the stype sheet
      void setErrorStyle(const std::string &style);

      /// @name For testing
      ///@{

      /// @brief remove all Devices namespaces
      void clearDevicesNamespaces();
      /// @brief remove all Error namespaces
      void clearErrorNamespaces();
      /// @brief remove all Streams namespaces
      void clearStreamsNamespaces();
      /// @brief remove all Assets namespaces
      void clearAssetsNamespaces();
      ///@}

      /// @brief Get the Devices URN for a prefix
      /// @param[in] prefix the prefix
      /// @return the URN
      std::string getDevicesUrn(const std::string &prefix);
      /// @brief Get the Error URN for a prefix
      /// @param[in] prefix the prefix
      /// @return the URN
      std::string getErrorUrn(const std::string &prefix);
      /// @brief Get the Streams URN for a prefix
      /// @param[in] prefix the prefix
      /// @return the URN
      std::string getStreamsUrn(const std::string &prefix);
      /// @brief Get the Assets URN for a prefix
      /// @param[in] prefix the prefix
      /// @return the URN
      std::string getAssetsUrn(const std::string &prefix);

      /// @brief Get the Devices location for a prefix
      /// @param[in] prefix the prefix
      /// @return the location
      std::string getDevicesLocation(const std::string &prefix);
      /// @brief Get the Error location for a prefix
      /// @param[in] prefix the prefix
      /// @return the location
      std::string getErrorLocation(const std::string &prefix);
      /// @brief Get the Streams location for a prefix
      /// @param[in] prefix the prefix
      /// @return the location
      std::string getStreamsLocation(const std::string &prefix);
      /// @brief Get the Assets location for a prefix
      /// @param[in] prefix the prefix
      /// @return the location
      std::string getAssetsLocation(const std::string &prefix);

    protected:
      enum EDocumentType
      {
        eERROR,
        eSTREAMS,
        eDEVICES,
        eASSETS
      };

      struct SchemaNamespace
      {
        std::string mUrn;
        std::string mSchemaLocation;
      };

      // Initiate all documents
      void initXmlDoc(xmlTextWriterPtr writer, EDocumentType docType, const uint64_t instanceId,
                      const unsigned int bufferSize, const unsigned int assetBufferSize,
                      const unsigned int assetCount, const uint64_t nextSeq,
                      const uint64_t firstSeq = 0, const uint64_t lastSeq = 0,
                      const std::map<std::string, size_t> *counts = nullptr,
                      const std::optional<std::string> requestId = std::nullopt) const;

      // Helper to print individual components and details
      void printProbeHelper(xmlTextWriterPtr writer, device_model::ComponentPtr component,
                            const char *name) const;
      void printDataItem(xmlTextWriterPtr writer, DataItemPtr dataItem) const;
      void addObservation(xmlTextWriterPtr writer, observation::ObservationPtr result) const;

    protected:
      std::map<std::string, SchemaNamespace> m_devicesNamespaces;
      std::map<std::string, SchemaNamespace> m_streamsNamespaces;
      std::map<std::string, SchemaNamespace> m_errorNamespaces;
      std::map<std::string, SchemaNamespace> m_assetNamespaces;

      std::unordered_set<std::string> m_deviceNsSet;
      std::unordered_set<std::string> m_streamsNsSet;
      std::unordered_set<std::string> m_errorNsSet;
      std::unordered_set<std::string> m_assetNsSet;

      std::string m_streamsStyle;
      std::string m_devicesStyle;
      std::string m_errorStyle;
      std::string m_assetStyle;
    };
  }  // namespace printer
}  // namespace mtconnect
