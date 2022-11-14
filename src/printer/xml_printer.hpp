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

#include <unordered_set>

#include "asset/asset.hpp"
#include "printer/printer.hpp"
#include "utilities.hpp"

extern "C"
{
  using xmlTextWriter = struct _xmlTextWriter;
  using xmlTextWriterPtr = xmlTextWriter *;
}

namespace mtconnect {
  class SensorConfiguration;

  namespace printer {
    class XmlWriter;

    class XmlPrinter : public Printer
    {
    public:
      XmlPrinter(bool pretty = false);
      ~XmlPrinter() override = default;

      std::string printErrors(const uint64_t instanceId, const unsigned int bufferSize,
                              const uint64_t nextSeq, const ProtoErrorList &list) const override;

      std::string printProbe(const uint64_t instanceId, const unsigned int bufferSize,
                             const uint64_t nextSeq, const unsigned int assetBufferSize,
                             const unsigned int assetCount, const std::list<DevicePtr> &devices,
                             const std::map<std::string, size_t> *count = nullptr) const override;

      std::string printSample(const uint64_t instanceId, const unsigned int bufferSize,
                              const uint64_t nextSeq, const uint64_t firstSeq,
                              const uint64_t lastSeq,
                              observation::ObservationList &results) const override;
      std::string printAssets(const uint64_t anInstanceId, const unsigned int bufferSize,
                              const unsigned int assetCount,
                              const asset::AssetList &asset) const override;
      std::string mimeType() const override { return "text/xml"; }

      void addDevicesNamespace(const std::string &urn, const std::string &location,
                               const std::string &prefix);
      void addErrorNamespace(const std::string &urn, const std::string &location,
                             const std::string &prefix);
      void addStreamsNamespace(const std::string &urn, const std::string &location,
                               const std::string &prefix);
      void addAssetsNamespace(const std::string &urn, const std::string &location,
                              const std::string &prefix);

      void setDevicesStyle(const std::string &style);
      void setStreamStyle(const std::string &style);
      void setAssetsStyle(const std::string &style);
      void setErrorStyle(const std::string &style);

      // For testing
      void clearDevicesNamespaces();
      void clearErrorNamespaces();
      void clearStreamsNamespaces();
      void clearAssetsNamespaces();

      std::string getDevicesUrn(const std::string &prefix);
      std::string getErrorUrn(const std::string &prefix);
      std::string getStreamsUrn(const std::string &prefix);
      std::string getAssetsUrn(const std::string &prefix);

      std::string getDevicesLocation(const std::string &prefix);
      std::string getErrorLocation(const std::string &prefix);
      std::string getStreamsLocation(const std::string &prefix);
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
                      const std::map<std::string, size_t> *counts = nullptr) const;

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
