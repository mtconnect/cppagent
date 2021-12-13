//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "circular_buffer.hpp"
#include "loopback_source.hpp"
#include "request.hpp"
#include "response.hpp"
#include "server.hpp"
#include "sink.hpp"
#include "utilities.hpp"

namespace mtconnect {
  class XmlPrinter;

  namespace rest_sink {
    struct AsyncSampleResponse;
    struct AsyncCurrentResponse;

    using NamespaceFunction = void (XmlPrinter::*)(const std::string &, const std::string &,
                                                   const std::string &);
    using StyleFunction = void (XmlPrinter::*)(const std::string &);

    class RestService : public Sink
    {
    public:
      RestService(boost::asio::io_context &context, SinkContractPtr &&contract,
                  const ConfigOptions &options, const boost::property_tree::ptree &config);

      ~RestService() = default;

      // Register the service with the sink factory
      static void registerFactory(SinkFactory &factory)
      {
        factory.registerFactory(
            "RestService",
            [](const std::string &name, boost::asio::io_context &io, SinkContractPtr &&contract,
               const ConfigOptions &options, const boost::property_tree::ptree &block) -> SinkPtr {
              auto sink = std::make_shared<RestService>(io, std::move(contract), options, block);
              return sink;
            });
      }

      auto makeLoopbackSource(pipeline::PipelineContextPtr context)
      {
        m_loopback = std::make_shared<LoopbackSource>("RestSource", m_strand, context, m_options);
        m_sinkContract->addSource(m_loopback);
        return m_loopback;
      }

      // Sink Methods
      void start() override;
      void stop() override;

      uint64_t publish(observation::ObservationPtr &observation) override;
      bool publish(asset::AssetPtr asset) override { return false; }

      auto getServer() { return m_server.get(); }
      auto getFileCache() { return &m_fileCache; }

      // Observation management
      observation::ObservationPtr getFromBuffer(uint64_t seq) const
      {
        return m_circularBuffer.getFromBuffer(seq);
      }
      SequenceNumber_t getSequence() const { return m_circularBuffer.getSequence(); }
      unsigned int getBufferSize() const { return m_circularBuffer.getBufferSize(); }

      SequenceNumber_t getFirstSequence() const { return m_circularBuffer.getFirstSequence(); }

      // For testing...
      void setSequence(uint64_t seq) { m_circularBuffer.setSequence(seq); }

      // MTConnect Requests
      ResponsePtr probeRequest(const Printer *,
                               const std::optional<std::string> &device = std::nullopt);
      ResponsePtr currentRequest(const Printer *,
                                 const std::optional<std::string> &device = std::nullopt,
                                 const std::optional<SequenceNumber_t> &at = std::nullopt,
                                 const std::optional<std::string> &path = std::nullopt);
      ResponsePtr sampleRequest(const Printer *, const int count = 100,
                                const std::optional<std::string> &device = std::nullopt,
                                const std::optional<SequenceNumber_t> &from = std::nullopt,
                                const std::optional<SequenceNumber_t> &to = std::nullopt,
                                const std::optional<std::string> &path = std::nullopt);
      void streamSampleRequest(SessionPtr session, const Printer *, const int interval,
                               const int heartbeat, const int count = 100,
                               const std::optional<std::string> &device = std::nullopt,
                               const std::optional<SequenceNumber_t> &from = std::nullopt,
                               const std::optional<std::string> &path = std::nullopt);

      // Async stream method
      void streamSampleWriteComplete(std::shared_ptr<AsyncSampleResponse> asyncResponse);
      void streamNextSampleChunk(std::shared_ptr<AsyncSampleResponse> asyncResponse,
                                 boost::system::error_code ec);

      void streamCurrentRequest(SessionPtr session, const Printer *, const int interval,
                                const std::optional<std::string> &device = std::nullopt,
                                const std::optional<std::string> &path = std::nullopt);

      void streamNextCurrent(std::shared_ptr<AsyncCurrentResponse> asyncResponse,
                             boost::system::error_code ec);

      // Asset requests
      ResponsePtr assetRequest(const Printer *, const int32_t count, const bool removed,
                               const std::optional<std::string> &type = std::nullopt,
                               const std::optional<std::string> &device = std::nullopt);
      ResponsePtr assetIdsRequest(const Printer *, const std::list<std::string> &ids);
      ResponsePtr putAssetRequest(const Printer *, const std::string &asset,
                                  const std::optional<std::string> &type,
                                  const std::optional<std::string> &device = std::nullopt,
                                  const std::optional<std::string> &uuid = std::nullopt);
      ResponsePtr deleteAssetRequest(const Printer *, const std::list<std::string> &ids);
      ResponsePtr deleteAllAssetsRequest(const Printer *,
                                         const std::optional<std::string> &device = std::nullopt,
                                         const std::optional<std::string> &type = std::nullopt);
      ResponsePtr putObservationRequest(const Printer *, const std::string &device,
                                        const QueryMap observations,
                                        const std::optional<std::string> &time = std::nullopt);

      // For debugging
      void setLogStreamData(bool log) { m_logStreamData = log; }

      // Get the printer for a type
      const std::string acceptFormat(const std::string &accepts) const
      {
        std::stringstream list(accepts);
        std::string accept;
        while (std::getline(list, accept, ','))
        {
          for (const auto &p : m_sinkContract->getPrinters())
          {
            if (ends_with(accept, p.first))
              return p.first;
          }
        }

        return "xml";
      }

      const Printer *printerForAccepts(const std::string &accepts) const
      {
        return m_sinkContract->getPrinter(acceptFormat(accepts));
      }

      // Output an XML Error
      std::string printError(const Printer *printer, const std::string &errorCode,
                             const std::string &text) const;

    protected:
      // Configuration
      void loadNamespace(const boost::property_tree::ptree &tree, const char *namespaceType,
                         XmlPrinter *xmlPrinter, NamespaceFunction callback);
      void loadFiles(XmlPrinter *xmlPrinter, const boost::property_tree::ptree &tree);
      void loadHttpHeaders(const boost::property_tree::ptree &tree);
      void loadStyle(const boost::property_tree::ptree &tree, const char *styleName,
                     XmlPrinter *xmlPrinter, StyleFunction styleFunction);
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
      std::string fetchCurrentData(const Printer *printer, const FilterSetOpt &filterSet,
                                   const std::optional<SequenceNumber_t> &at);

      // Sample data collection
      std::string fetchSampleData(const Printer *printer, const FilterSetOpt &filterSet, int count,
                                  const std::optional<SequenceNumber_t> &from,
                                  const std::optional<SequenceNumber_t> &to, SequenceNumber_t &end,
                                  bool &endOfBuffer,
                                  observation::ChangeObserver *observer = nullptr);

      // Verification methods
      template <typename T>
      void checkRange(const Printer *printer, const T value, const T min, const T max,
                      const std::string &param, bool notZero = false) const;
      void checkPath(const Printer *printer, const std::optional<std::string> &path,
                     const DevicePtr device, FilterSet &filter) const;
      DevicePtr checkDevice(const Printer *printer, const std::string &uuid) const;

    protected:
      // Loopback
      boost::asio::io_context &m_context;
      boost::asio::io_context::strand m_strand;
      std::string m_version;

      ConfigOptions m_options;
      std::shared_ptr<LoopbackSource> m_loopback;

      uint64_t m_instanceId;
      std::unique_ptr<Server> m_server;

      // Buffers
      FileCache m_fileCache;
      CircularBuffer m_circularBuffer;

      bool m_logStreamData {false};
    };
  }  // namespace rest_sink
}  // namespace mtconnect
