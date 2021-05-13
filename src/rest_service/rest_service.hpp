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

#include "sink.hpp"

#include "server.hpp"
#include "observation/observation.hpp"

namespace mtconnect 
{
  namespace rest_service
  {    
    class RestService : Sink
    {
    public:
      RestService(boost::asio::io_context &context);
      
      virtual ~RestService() {}
      
      virtual void start();
      virtual void stop();
      
      virtual receiveObservation();
      virtual receiveAsset();

      // Add component events to the sliding buffer
      observation::SequenceNumber_t addToBuffer(observation::ObservationPtr &observation);
      observation::SequenceNumber_t addToBuffer(DataItemPtr dataItem, entity::Properties props,
						std::optional<Timestamp> timestamp = std::nullopt);
      observation::SequenceNumber_t addToBuffer(DataItemPtr dataItem, const std::string &value,
						std::optional<Timestamp> timestamp = std::nullopt);


      // Asset management
      void addAsset(AssetPtr asset);
      AssetPtr addAsset(DevicePtr device, const std::string &asset,
			const std::optional<std::string> &id, const std::optional<std::string> &type,
			const std::optional<std::string> &time, entity::ErrorList &errors);
      
      bool removeAsset(DevicePtr device, const std::string &id,
		       const std::optional<Timestamp> time = std::nullopt);
      bool removeAllAssets(const std::optional<std::string> device,
			   const std::optional<std::string> type, const std::optional<Timestamp> time,
			   AssetList &list);
      
      observation::ObservationPtr getFromBuffer(uint64_t seq) const
      {
	return m_circularBuffer.getFromBuffer(seq);
      }
      observation::SequenceNumber_t getSequence() const { return m_circularBuffer.getSequence(); }
      unsigned int getBufferSize() const { return m_circularBuffer.getBufferSize(); }
      auto getMaxAssets() const { return m_assetBuffer.getMaxAssets(); }
      auto getAssetCount(bool active = true) const { return m_assetBuffer.getCount(active); }
      const auto &getAssets() const { return m_assetBuffer.getAssets(); }
      auto getFileCache() { return m_fileCache.get(); }
      
      auto getAssetCount(const std::string &type, bool active = true) const
      {
	return m_assetBuffer.getCountForType(type, active);
      }
      
      observation::SequenceNumber_t getFirstSequence() const
      {
	return m_circularBuffer.getFirstSequence();
      }
      
      // For testing...
      void setSequence(uint64_t seq) { m_circularBuffer.setSequence(seq); }

      // MTConnect Requests
      rest_service::Response probeRequest(const Printer *,
                                       const std::optional<std::string> &device = std::nullopt);
    rest_service::Response currentRequest(
        const Printer *, const std::optional<std::string> &device = std::nullopt,
        const std::optional<observation::SequenceNumber_t> &at = std::nullopt,
        const std::optional<std::string> &path = std::nullopt);
    rest_service::Response sampleRequest(
        const Printer *, const int count = 100,
        const std::optional<std::string> &device = std::nullopt,
        const std::optional<observation::SequenceNumber_t> &from = std::nullopt,
        const std::optional<observation::SequenceNumber_t> &to = std::nullopt,
        const std::optional<std::string> &path = std::nullopt);
    void streamSampleRequest(
        rest_service::SessionPtr session, const Printer *, const int interval, const int heartbeat,
        const int count = 100, const std::optional<std::string> &device = std::nullopt,
        const std::optional<observation::SequenceNumber_t> &from = std::nullopt,
        const std::optional<std::string> &path = std::nullopt);

    // Async stream method
    void streamSampleWriteComplete(std::shared_ptr<AsyncSampleResponse> asyncResponse);
    void streamNextSampleChunk(std::shared_ptr<AsyncSampleResponse> asyncResponse,
                               boost::system::error_code ec);

    void streamCurrentRequest(rest_service::SessionPtr session, const Printer *, const int interval,
                              const std::optional<std::string> &device = std::nullopt,
                              const std::optional<std::string> &path = std::nullopt);

    void streamNextCurrent(std::shared_ptr<AsyncCurrentResponse> asyncResponse,
                           boost::system::error_code ec);

    rest_service::Response assetRequest(const Printer *, const int32_t count, const bool removed,
                                       const std::optional<std::string> &type = std::nullopt,
                                       const std::optional<std::string> &device = std::nullopt);
    rest_service::Response assetIdsRequest(const Printer *, const std::list<std::string> &ids);
    rest_service::Response putAssetRequest(const Printer *, const std::string &asset,
                                          const std::optional<std::string> &type,
                                          const std::optional<std::string> &device = std::nullopt,
                                          const std::optional<std::string> &uuid = std::nullopt);
    rest_service::Response deleteAssetRequest(const Printer *, const std::list<std::string> &ids);
    rest_service::Response deleteAllAssetsRequest(
        const Printer *, const std::optional<std::string> &device = std::nullopt,
        const std::optional<std::string> &type = std::nullopt);
    rest_service::Response putObservationRequest(
        const Printer *, const std::string &device, const rest_service::QueryMap observations,
        const std::optional<std::string> &time = std::nullopt);

    // For debugging
    void setLogStreamData(bool log) { m_logStreamData = log; }

    // Get the printer for a type
    const std::string acceptFormat(const std::string &accepts) const;
    Printer *getPrinter(const std::string &aType) const
    {
      auto printer = m_printers.find(aType);
      if (printer != m_printers.end())
        return printer->second.get();
      else
        return nullptr;
    }
    const Printer *printerForAccepts(const std::string &accepts) const
    {
      return getPrinter(acceptFormat(accepts));
    }
      
    protected:
      // HTTP Routings
      void createPutObservationRoutings();
      void createFileRoutings();
      void createProbeRoutings();
      void createSampleRoutings();
      void createCurrentRoutings();
      void createAssetRoutings();

      // Current Data Collection
      std::string fetchCurrentData(const Printer *printer, const observation::FilterSetOpt &filterSet,
				   const std::optional<observation::SequenceNumber_t> &at);
      
      // Sample data collection
      std::string fetchSampleData(const Printer *printer, const observation::FilterSetOpt &filterSet,
				  int count, const std::optional<observation::SequenceNumber_t> &from,
				  const std::optional<observation::SequenceNumber_t> &to,
				  observation::SequenceNumber_t &end, bool &endOfBuffer,
				  observation::ChangeObserver *observer = nullptr);
      
      // Asset methods
      void getAssets(const Printer *printer, const std::list<std::string> &ids, AssetList &list);
      void getAssets(const Printer *printer, const int32_t count, const bool removed,
		     const std::optional<std::string> &type, const std::optional<std::string> &device,
		     AssetList &list);

      
      // Verification methods
      template <typename T>
      void checkRange(const Printer *printer, const T value, const T min, const T max,
		      const std::string &param, bool notZero = false) const;
      void checkPath(const Printer *printer, const std::optional<std::string> &path,
		     const DevicePtr device, observation::FilterSet &filter) const;
      DevicePtr checkDevice(const Printer *printer, const std::string &uuid) const;
      
    protected:
      // Loopback
      std::unique_ptr<AgentLoopbackPipeline> m_loopback;
      
      uint64_t m_instanceId;
      std::unique_ptr<Server> m_server;
      std::unique_ptr<FileCache> m_fileCache;

      // Buffers
      CircularBuffer m_circularBuffer;
      AssetBuffer m_assetBuffer;            
    };      
  }
}

