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

#pragma once

#include "assets/asset.hpp"
#include "globals.hpp"
#include "printer.hpp"

extern "C"
{
  using xmlTextWriter = struct _xmlTextWriter;
  using xmlTextWriterPtr = xmlTextWriter *;
}

namespace mtconnect
{
  class DataItem;
  class SensorConfiguration;
  class XmlWriter;

  class XmlPrinter : public Printer
  {
  public:
    XmlPrinter(const std::string version = "", bool pretty = false);
    ~XmlPrinter() override = default;

    std::string printErrors(const unsigned int instanceId, const unsigned int bufferSize,
                            const uint64_t nextSeq, const ProtoErrorList &list) const override;

    std::string printProbe(const unsigned int instanceId, const unsigned int bufferSize,
                           const uint64_t nextSeq, const unsigned int assetBufferSize,
                           const unsigned int assetCount, const std::list<Device *> &devices,
                           const std::map<std::string, int> *count = nullptr) const override;

    std::string printSample(const unsigned int instanceId, const unsigned int bufferSize,
                            const uint64_t nextSeq, const uint64_t firstSeq, const uint64_t lastSeq,
                            ObservationPtrArray &results) const override;
    std::string printAssets(const unsigned int anInstanceId, const unsigned int bufferSize,
                            const unsigned int assetCount, const AssetList &assets) const override;
    std::string mimeType() const override { return "text/xml"; }

    void addDevicesNamespace(const std::string &urn, const std::string &location,
                             const std::string &prefix);
    void addErrorNamespace(const std::string &urn, const std::string &location,
                           const std::string &prefix);
    void addStreamsNamespace(const std::string &urn, const std::string &location,
                             const std::string &prefix);
    void addAssetsNamespace(const std::string &urn, const std::string &location,
                            const std::string &prefix);

    void setSchemaVersion(const std::string &version);
    const std::string &getSchemaVersion();

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
    void initXmlDoc(xmlTextWriterPtr writer, EDocumentType docType, const unsigned int instanceId,
                    const unsigned int bufferSize, const unsigned int assetBufferSize,
                    const unsigned int assetCount, const uint64_t nextSeq,
                    const uint64_t firstSeq = 0, const uint64_t lastSeq = 0,
                    const std::map<std::string, int> *counts = nullptr) const;

    // Helper to print individual components and details
    void printProbeHelper(xmlTextWriterPtr writer, Component *component, const char *name) const;
    void printDataItem(xmlTextWriterPtr writer, DataItem *dataItem) const;
    void printDataItemDefinition(xmlTextWriterPtr writer,
                                 const DataItemDefinition &definition) const;
    void printDataItemRelationships(xmlTextWriterPtr writer,
                                    const std::list<DataItem::Relationship> &relations) const;
    void printCellDefinitions(xmlTextWriterPtr writer,
                              const std::set<CellDefinition> &definitions) const;

    void addObservation(xmlTextWriterPtr writer, Observation *result) const;

  protected:
    std::map<std::string, SchemaNamespace> m_devicesNamespaces;
    std::map<std::string, SchemaNamespace> m_streamsNamespaces;
    std::map<std::string, SchemaNamespace> m_errorNamespaces;
    std::map<std::string, SchemaNamespace> m_assetsNamespaces;
    std::string m_schemaVersion;
    std::string m_streamsStyle;
    std::string m_devicesStyle;
    std::string m_errorStyle;
    std::string m_assetsStyle;
  };
}  // namespace mtconnect
