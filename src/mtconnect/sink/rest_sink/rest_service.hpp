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

#include <boost/asio/io_context.hpp>

#include "mtconnect/buffer/circular_buffer.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/sink/sink.hpp"
#include "mtconnect/source/loopback_source.hpp"
#include "mtconnect/utilities.hpp"
#include "request.hpp"
#include "response.hpp"
#include "server.hpp"

namespace mtconnect {
  namespace printer {
    class XmlPrinter;
  }

  /// @brief MTConnect REST normative implemention namespace
  namespace sink::rest_sink {
    struct AsyncSampleResponse;
    struct AsyncCurrentResponse;

    /// @brief Callback fundtion for setting namespaces
    using NamespaceFunction = void (printer::XmlPrinter::*)(const std::string &,
                                                            const std::string &,
                                                            const std::string &);
    /// @brief Callback fundtion for setting stylesheet
    using StyleFunction = void (printer::XmlPrinter::*)(const std::string &);

    /// @brief The Sink for the MTConnect normative REST Service
    class AGENT_LIB_API RestService : public Sink
    {
    public:
      /// @brief Create a Rest Service sink
      /// @param context the boost asio io_context
      /// @param contract the Sink Contract from the agent
      /// @param options configuration options
      /// @param config additional configuration options if specified directly as a sink
      RestService(boost::asio::io_context &context, SinkContractPtr &&contract,
                  const ConfigOptions &options, const boost::property_tree::ptree &config);

      ~RestService() = default;

      /// @brief Register the Sink factory to create this sink
      /// @param factory
      static void registerFactory(SinkFactory &factory);

      /// @brief Make a loopback source to handle PUT, POST, and DELETE
      /// @param context the pipeline context
      /// @return shared pointer to a loopback source
      std::shared_ptr<source::LoopbackSource> makeLoopbackSource(
          pipeline::PipelineContextPtr context);

      /// @name Interface methods
      ///@{
      void start() override;

      void stop() override;

      bool publish(observation::ObservationPtr &observation) override;

      bool publish(asset::AssetPtr asset) override { return false; }
      ///@}

      /// @brief Get the HTTP server
      /// @return pointer to the HTTP server
      auto getServer() { return m_server.get(); }
      /// @brief Get the file cache
      /// @return pointer to the file cache
      auto getFileCache() { return &m_fileCache; }

      /// @name MTConnect Request Handlers
      ///@{

      /// @brief Handler for a probe request
      /// @param  p printer for doc generation
      /// @param device optional device name or uuid
      /// @return MTConnect Devices response
      ResponsePtr probeRequest(const printer::Printer *p,
                               const std::optional<std::string> &device = std::nullopt);

      /// @brief Handler for a current request
      /// @param  p printer for doc generation
      /// @param device optional device name or uuid
      /// @param at optional sequence number to take the snapshot
      /// @param path an xpath to filter
      /// @return MTConnect Streams response
      ResponsePtr currentRequest(const printer::Printer *p,
                                 const std::optional<std::string> &device = std::nullopt,
                                 const std::optional<SequenceNumber_t> &at = std::nullopt,
                                 const std::optional<std::string> &path = std::nullopt);

      /// @brief Handler for a sample request
      /// @param p printer for doc generation
      /// @param count maximum number of observations
      /// @param device optional device name or uuid
      /// @param from optional starting sequence number
      /// @param to optional ending sequence number
      /// @param path an xpath for filtering
      /// @return MTConnect Streams response
      ResponsePtr sampleRequest(const printer::Printer *p, const int count = 100,
                                const std::optional<std::string> &device = std::nullopt,
                                const std::optional<SequenceNumber_t> &from = std::nullopt,
                                const std::optional<SequenceNumber_t> &to = std::nullopt,
                                const std::optional<std::string> &path = std::nullopt);
      /// @brief Handler for a streaming sample
      /// @param session session to stream data to
      /// @param p printer for doc generation
      /// @param interval the minimum interval between sending documents in ms
      /// @param heartbeat how often to send an empty document if no activity in ms
      /// @param count the maxumum number of observations
      /// @param device optional device name or uuid
      /// @param from optional starting sequence number
      /// @param path optional path for filtering
      void streamSampleRequest(SessionPtr session, const printer::Printer *p, const int interval,
                               const int heartbeat, const int count = 100,
                               const std::optional<std::string> &device = std::nullopt,
                               const std::optional<SequenceNumber_t> &from = std::nullopt,
                               const std::optional<std::string> &path = std::nullopt);

      /// @brief Handler for a streaming current
      /// @param session session to stream data to
      /// @param p printer for doc generation
      /// @param interval the minimum interval between sending documents in ms
      /// @param device optional device name or uuid
      /// @param path optional path for filtering
      void streamCurrentRequest(SessionPtr session, const printer::Printer *p, const int interval,
                                const std::optional<std::string> &device = std::nullopt,
                                const std::optional<std::string> &path = std::nullopt);
      /// @brief Handler for put/post observation
      /// @param p printer for response generation
      /// @param device device
      /// @param observations key/value pairs for the observations
      /// @param time optional timestamp
      /// @return `<success/>` if succeeds
      ResponsePtr putObservationRequest(const printer::Printer *p, const std::string &device,
                                        const QueryMap observations,
                                        const std::optional<std::string> &time = std::nullopt);

      ///@}

      /// @name Async stream method
      ///@{

      /// @brief Callback when the async write completes
      /// @param asyncResponse shared pointer to response referencing the session
      void streamSampleWriteComplete(std::shared_ptr<AsyncSampleResponse> asyncResponse);

      /// @brief After the write complete, send the next chunk of data
      /// @param asyncResponse shared pointer to async response referencing the session
      /// @param ec an async error code
      void streamNextSampleChunk(std::shared_ptr<AsyncSampleResponse> asyncResponse,
                                 boost::system::error_code ec);

      /// @brief Callback to stream another current chunk
      /// @param asyncResponse shared pointer to async response referencing the session
      /// @param ec an async error code
      void streamNextCurrent(std::shared_ptr<AsyncCurrentResponse> asyncResponse,
                             boost::system::error_code ec);
      ///@}

      /// @name Asset Request Handler
      ///@{

      /// @brief Asset request handler for assets by type or device
      /// @param p printer for the response document
      /// @param count maximum number of assets to return
      /// @param removed `true` if response should include removed assets
      /// @param type optional type of asset to filter
      /// @param device optional device name or uuid
      /// @return MTConnect Assets response document
      ResponsePtr assetRequest(const printer::Printer *p, const int32_t count, const bool removed,
                               const std::optional<std::string> &type = std::nullopt,
                               const std::optional<std::string> &device = std::nullopt);

      /// @brief Asset request handler using a list of asset ids
      /// @param p printer for the response document
      /// @param ids list of asset ids
      /// @return MTConnect Assets response document
      ResponsePtr assetIdsRequest(const printer::Printer *p, const std::list<std::string> &ids);

      /// @brief Asset request handler to update an asset
      /// @param p printer for the response document
      /// @param asset the asset body
      /// @param type optional type, if not given will derive from `asset`
      /// @param device option device, if not given will derive from `asset`
      /// @param uuid optional asset id, if not given will derive from the `asset`
      /// @return MTConnect Assets response document
      ResponsePtr putAssetRequest(const printer::Printer *p, const std::string &asset,
                                  const std::optional<std::string> &type,
                                  const std::optional<std::string> &device = std::nullopt,
                                  const std::optional<std::string> &uuid = std::nullopt);
      /// @brief Asset request handler to delete a a list of asset ids
      /// @param p printer for the response document
      /// @param ids the list of ids
      /// @return MTConnect Assets response document
      ResponsePtr deleteAssetRequest(const printer::Printer *p, const std::list<std::string> &ids);
      /// @brief Asset request handler to delete all assets by device and/or type
      /// @param p printer for the response document
      /// @param device optional device
      /// @param type optonal type
      /// @return number of assets removed as response
      ResponsePtr deleteAllAssetsRequest(const printer::Printer *p,
                                         const std::optional<std::string> &device = std::nullopt,
                                         const std::optional<std::string> &type = std::nullopt);
      ///@}

      /// @brief For debugging: turn on stream data logging
      /// @note This is only for debuging
      void setLogStreamData(bool log);

      /// @brief Check the accepts header for a matching printer key
      /// @param accepts the accepts header
      /// @return printer key or `xml` if one is not found
      const std::string acceptFormat(const std::string &accepts) const;
      /// @brief get a printer given a list of formats from the Accepts header
      /// @param accepts the accepts header
      /// @return pointer to a printer
      const printer::Printer *printerForAccepts(const std::string &accepts) const;

      /// @brief Generate an MTConnect Error document
      /// @param printer printer to generate error
      /// @param errorCode an error code
      /// @param text descriptive error text
      /// @return MTConnect Error document
      std::string printError(const printer::Printer *printer, const std::string &errorCode,
                             const std::string &text) const;

      /// @name For testing only
      ///@{
      auto instanceId() const { return m_instanceId; }
      void setInstanceId(uint64_t id) { m_instanceId = id; }
      ///@}

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
