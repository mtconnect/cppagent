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

#include "boost/asio/io_context.hpp"

#include "buffer/circular_buffer.hpp"
#include "request.hpp"
#include "response.hpp"
#include "server.hpp"
#include "sink/sink.hpp"
#include "source/loopback_source.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace printer {
    class XmlPrinter;
  }

  namespace sink::rest_sink {
    struct AsyncSampleResponse;
    struct AsyncCurrentResponse;

    using NamespaceFunction = void (printer::XmlPrinter::*)(const std::string &,
                                                            const std::string &,
                                                            const std::string &);
    using StyleFunction = void (printer::XmlPrinter::*)(const std::string &);

    class RestService : public Sink
    {
    public:
      RestService(boost::asio::io_context &context, SinkContractPtr &&contract,
                  const ConfigOptions &options, const boost::property_tree::ptree &config);

      ~RestService() = default;

      // Register the service with the sink factory
      static void registerFactory(SinkFactory &factory);

      std::shared_ptr<source::LoopbackSource> makeLoopbackSource(
          pipeline::PipelineContextPtr context);

      // Sink Methods
      void start() override;

      void stop() override;

      bool publish(observation::ObservationPtr &observation) override;

      bool publish(asset::AssetPtr asset) override { return false; }

      auto getServer() { return m_server.get(); }

      auto getFileCache() { return &m_fileCache; }

      // Observation management
      observation::ObservationPtr getFromBuffer(uint64_t seq) const;

      SequenceNumber_t getSequence() const;

      unsigned int getBufferSize() const;

      SequenceNumber_t getFirstSequence() const;

      // For testing...
      void setSequence(uint64_t seq);

      // MTConnect Requests
      ResponsePtr probeRequest(const printer::Printer *,
                               const std::optional<std::string> &device = std::nullopt);

      ResponsePtr currentRequest(const printer::Printer *,
                                 const std::optional<std::string> &device = std::nullopt,
                                 const std::optional<SequenceNumber_t> &at = std::nullopt,
                                 const std::optional<std::string> &path = std::nullopt);

      ResponsePtr sampleRequest(const printer::Printer *, const int count = 100,
                                const std::optional<std::string> &device = std::nullopt,
                                const std::optional<SequenceNumber_t> &from = std::nullopt,
                                const std::optional<SequenceNumber_t> &to = std::nullopt,
                                const std::optional<std::string> &path = std::nullopt);

      void streamSampleRequest(SessionPtr session, const printer::Printer *, const int interval,
                               const int heartbeat, const int count = 100,
                               const std::optional<std::string> &device = std::nullopt,
                               const std::optional<SequenceNumber_t> &from = std::nullopt,
                               const std::optional<std::string> &path = std::nullopt);

      // Async stream method
      void streamSampleWriteComplete(std::shared_ptr<AsyncSampleResponse> asyncResponse);

      void streamNextSampleChunk(std::shared_ptr<AsyncSampleResponse> asyncResponse,
                                 boost::system::error_code ec);

      void streamCurrentRequest(SessionPtr session, const printer::Printer *, const int interval,
                                const std::optional<std::string> &device = std::nullopt,
                                const std::optional<std::string> &path = std::nullopt);

      void streamNextCurrent(std::shared_ptr<AsyncCurrentResponse> asyncResponse,
                             boost::system::error_code ec);

      // Asset requests
      ResponsePtr assetRequest(const printer::Printer *, const int32_t count, const bool removed,
                               const std::optional<std::string> &type = std::nullopt,
                               const std::optional<std::string> &device = std::nullopt);

      ResponsePtr assetIdsRequest(const printer::Printer *, const std::list<std::string> &ids);

      ResponsePtr putAssetRequest(const printer::Printer *, const std::string &asset,
                                  const std::optional<std::string> &type,
                                  const std::optional<std::string> &device = std::nullopt,
                                  const std::optional<std::string> &uuid = std::nullopt);

      ResponsePtr deleteAssetRequest(const printer::Printer *, const std::list<std::string> &ids);

      ResponsePtr deleteAllAssetsRequest(const printer::Printer *,
                                         const std::optional<std::string> &device = std::nullopt,
                                         const std::optional<std::string> &type = std::nullopt);

      ResponsePtr putObservationRequest(const printer::Printer *, const std::string &device,
                                        const QueryMap observations,
                                        const std::optional<std::string> &time = std::nullopt);

      // For debugging
      void setLogStreamData(bool log);

      // Get the printer for a type
      const std::string acceptFormat(const std::string &accepts) const;

      const printer::Printer *printerForAccepts(const std::string &accepts) const;

      // Output an XML Error
      std::string printError(const printer::Printer *printer, const std::string &errorCode,
                             const std::string &text) const;

      // For testing
      auto instanceId() const { return m_instanceId; }
      void setInstanceId(uint64_t id) { m_instanceId = id; }

    protected:
      // Configuration
      void loadNamespace(const boost::property_tree::ptree &tree, const char *namespaceType,
                         printer::XmlPrinter *xmlPrinter, NamespaceFunction callback);

      void loadFiles(printer::XmlPrinter *xmlPrinter, const boost::property_tree::ptree &tree);

      void loadHttpHeaders(const boost::property_tree::ptree &tree);

      void loadStyle(const boost::property_tree::ptree &tree, const char *styleName,
                     printer::XmlPrinter *xmlPrinter, StyleFunction styleFunction);

      void loadTypes(const boost::property_tree::ptree &tree);

      void loadAllowPut();

      // HTTP Routings
      void createPutObservationRoutings();

      void createFileRoutings();

      void createProbeRoutings();

      void createSampleRoutings();

      void createCurrentRoutings();

      void createAssetRoutings();

      // Current Data Collection
      std::string fetchCurrentData(const printer::Printer *printer, const FilterSetOpt &filterSet,
                                   const std::optional<SequenceNumber_t> &at);

      // Sample data collection
      std::string fetchSampleData(const printer::Printer *printer, const FilterSetOpt &filterSet,
                                  int count, const std::optional<SequenceNumber_t> &from,
                                  const std::optional<SequenceNumber_t> &to, SequenceNumber_t &end,
                                  bool &endOfBuffer,
                                  observation::ChangeObserver *observer = nullptr);

      // Verification methods
      template <typename T>
      void checkRange(const printer::Printer *printer, const T value, const T min, const T max,
                      const std::string &param, bool notZero = false) const;

      void checkPath(const printer::Printer *printer, const std::optional<std::string> &path,
                     const DevicePtr device, FilterSet &filter) const;

      DevicePtr checkDevice(const printer::Printer *printer, const std::string &uuid) const;

    protected:
      // Loopback
      boost::asio::io_context &m_context;

      boost::asio::io_context::strand m_strand;

      std::string m_schemaVersion;

      ConfigOptions m_options;

      std::shared_ptr<source::LoopbackSource> m_loopback;

      uint64_t m_instanceId;

      std::unique_ptr<Server> m_server;

      // Buffers
      FileCache m_fileCache;

      bool m_logStreamData {false};
    };
  }  // namespace sink::rest_sink
}  // namespace mtconnect
