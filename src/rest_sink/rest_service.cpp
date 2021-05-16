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

#include "rest_service.hpp"

#include "server.hpp"
#include "entity/xml_parser.hpp"

namespace asio = boost::asio;
using namespace std;

namespace mtconnect
{
  using namespace configuration;
  using namespace observation;
  using namespace asset;

  
  namespace rest_sink
  {
    RestService::RestService(asio::io_context &context,
                             SinkContractPtr &&contract,
                             const ConfigOptions &options)
      : Sink(move(contract)), m_context(context), m_strand(context), m_options(options),
        m_circularBuffer(GetOption<int>(options, BufferSize).value_or(17),
                         GetOption<int>(options, CheckpointFrequency).value_or(1000)),
	m_logStreamData(GetOption<bool>(options, LogStreams).value_or(false))
    {
      // Unique id number for agent instance
      m_instanceId = getCurrentTimeInSec();
      
      m_server->setErrorFunction(
          [this](SessionPtr session, rest_sink::status st, const string &msg) {
            auto printer = m_sinkContract->getPrinter("xml");
            auto doc = printError(printer, "INVALID_REQUEST", msg);
            session->writeResponse({st, doc, printer->mimeType()});
          });
    }
    
    // -----------------------------------------------------------
    // Request Routing
    // -----------------------------------------------------------

    static inline void respond(rest_sink::SessionPtr session, rest_sink::Response response)
    {
      session->writeResponse(response);
    }

    void RestService::createFileRoutings()
    {
      using namespace rest_sink;
      auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto f = m_fileCache.getFile(request->m_path);
        if (f)
        {
          Response response(rest_sink::status::ok, f->m_buffer, f->m_mimeType);
          session->writeResponse(response);
        }
        return bool(f);
      };
      m_server->addRouting({boost::beast::http::verb::get, regex("/.+"), handler});
    }

    void RestService::createProbeRoutings()
    {
      using namespace rest_sink;
      // Probe
      auto handler = [&](SessionPtr session, const RequestPtr request) -> bool {
        auto device = request->parameter<string>("device");
        auto printer = printerForAccepts(request->m_accepts);

        if (device && !ends_with(request->m_path, string("probe")) &&
            m_sinkContract->findDeviceByUUIDorName(*device) == nullptr)
          return false;

        respond(session, probeRequest(printer, device));
        return true;
      };

      m_server->addRouting({boost::beast::http::verb::get, "/probe", handler});
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/probe", handler});
      // Must be last
      m_server->addRouting({boost::beast::http::verb::get, "/", handler});
      m_server->addRouting({boost::beast::http::verb::get, "/{device}", handler});
    }

    void RestService::createAssetRoutings()
    {
      using namespace rest_sink;
      auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto removed = *request->parameter<string>("removed") == "true";
        auto count = *request->parameter<int32_t>("count");
        auto printer = printerForAccepts(request->m_accepts);

        respond(session, assetRequest(printer, count, removed, request->parameter<string>("type"),
                                      request->parameter<string>("device")));
        return true;
      };

      auto idHandler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto asset = request->parameter<string>("asset");
        if (asset)
        {
          auto printer = m_sinkContract->getPrinter(acceptFormat(request->m_accepts));

          list<string> ids;
          stringstream str(*asset);
          string id;
          while (getline(str, id, ';'))
            ids.emplace_back(id);
          respond(session, assetIdsRequest(printer, ids));
        }
        else
        {
          auto printer = printerForAccepts(request->m_accepts);
          auto error = printError(printer, "INVALID_REQUEST", "No asset given");
          respond(session, {rest_sink::status::bad_request, error, printer->mimeType()});
        }
        return true;
      };

      string qp("type={string}&removed={string:false}&count={integer:100}&device={string}");
      m_server->addRouting({boost::beast::http::verb::get, "/asset?" + qp, handler});
      m_server->addRouting({boost::beast::http::verb::get, "/asset?" + qp, handler});
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/asset?" + qp, handler});
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/asset?" + qp, handler});

      m_server->addRouting({boost::beast::http::verb::get, "/asset/{asset}", idHandler});
      m_server->addRouting({boost::beast::http::verb::get, "/asset/{asset}", idHandler});

      if (m_server->arePutsAllowed())
      {
        auto putHandler = [&](SessionPtr session, RequestPtr request) -> bool {
          auto printer = printerForAccepts(request->m_accepts);
          respond(session,
                  putAssetRequest(printer, request->m_body, request->parameter<string>("type"),
                                  request->parameter<string>("device"),
                                  request->parameter<string>("uuid")));
          return true;
        };

        auto deleteHandler = [&](SessionPtr session, RequestPtr request) -> bool {
          auto asset = request->parameter<string>("asset");
          if (asset)
          {
            list<string> ids;
            stringstream str(*asset);
            string id;
            auto printer = printerForAccepts(request->m_accepts);

            while (getline(str, id, ';'))
              ids.emplace_back(id);
            respond(session, deleteAssetRequest(printer, ids));
          }
          else
          {
            respond(session, deleteAllAssetsRequest(printerForAccepts(request->m_accepts),
                                                    request->parameter<string>("device"),
                                                    request->parameter<string>("type")));
          }
          return true;
        };

        for (const auto &t : list<boost::beast::http::verb> {boost::beast::http::verb::put,
                                                             boost::beast::http::verb::post})
        {
          m_server->addRouting({t, "/asset/{uuid}?device={string}&type={string}", putHandler});
          m_server->addRouting({t, "/asset?device={string}&type={string}", putHandler});
          m_server->addRouting({t, "/{device}/asset/{uuid}?type={string}", putHandler});
          m_server->addRouting({t, "/{device}/asset?type={string}", putHandler});
        }

        m_server->addRouting({boost::beast::http::verb::delete_,
                              "/asset?&device={string}&type={string}", deleteHandler});
        m_server->addRouting({boost::beast::http::verb::delete_,
                              "/asset?&device={string}&type={string}", deleteHandler});
        m_server->addRouting({boost::beast::http::verb::delete_, "/asset/{asset}", deleteHandler});
        m_server->addRouting({boost::beast::http::verb::delete_, "/asset/{asset}", deleteHandler});
        m_server->addRouting(
            {boost::beast::http::verb::delete_, "/{device}/asset?type={string}", deleteHandler});
        m_server->addRouting(
            {boost::beast::http::verb::delete_, "/{device}/asset?type={string}", deleteHandler});
      }
    }

    void RestService::createCurrentRoutings()
    {
      using namespace rest_sink;
      auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto interval = request->parameter<int32_t>("interval");
        if (interval)
        {
          streamCurrentRequest(session, printerForAccepts(request->m_accepts), *interval,
                               request->parameter<string>("device"),
                               request->parameter<string>("path"));
        }
        else
        {
          respond(session, currentRequest(printerForAccepts(request->m_accepts),
                                          request->parameter<string>("device"),
                                          request->parameter<uint64_t>("at"),
                                          request->parameter<string>("path")));
        }
        return true;
      };

      string qp("path={string}&at={unsigned_integer}&interval={integer}");
      m_server->addRouting({boost::beast::http::verb::get, "/current?" + qp, handler});
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/current?" + qp, handler});
    }

    void RestService::createSampleRoutings()
    {
      using namespace rest_sink;
      auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto interval = request->parameter<int32_t>("interval");
        if (interval)
        {
          streamSampleRequest(
              session, printerForAccepts(request->m_accepts), *interval,
              *request->parameter<int32_t>("heartbeat"), *request->parameter<int32_t>("count"),
              request->parameter<string>("device"), request->parameter<uint64_t>("from"),
              request->parameter<string>("path"));
        }
        else
        {
          respond(session,
                  sampleRequest(
                      printerForAccepts(request->m_accepts), *request->parameter<int32_t>("count"),
                      request->parameter<string>("device"), request->parameter<uint64_t>("from"),
                      request->parameter<uint64_t>("to"), request->parameter<string>("path")));
        }
        return true;
      };

      string qp(
          "path={string}&from={unsigned_integer}&"
          "interval={integer}&count={integer:100}&"
          "heartbeat={integer:10000}&to={unsigned_integer}");
      m_server->addRouting({boost::beast::http::verb::get, "/sample?" + qp, handler});
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/sample?" + qp, handler});
    }

    void RestService::createPutObservationRoutings()
    {
      using namespace rest_sink;

      if (m_server->arePutsAllowed())
      {
        auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
          if (!request->m_query.empty())
          {
            auto queries = request->m_query;
            auto ts = request->parameter<string>("time");
            if (ts)
              queries.erase("time");
            auto device = request->parameter<string>("device");

            respond(session, putObservationRequest(printerForAccepts(request->m_accepts), *device,
                                                   queries, ts));
            return true;
          }
          else
          {
            return true;
          }
        };

        m_server->addRouting({boost::beast::http::verb::put, "/{device}?time={string}", handler});
        m_server->addRouting({boost::beast::http::verb::post, "/{device}?time={string}", handler});
      }
    }

    // ----------------------------------------------------
    // Observation Add Method
    // ----------------------------------------------------

    SequenceNumber_t RestService::publish(ObservationPtr &observation)
    {
      std::lock_guard<CircularBuffer> lock(m_circularBuffer);

      auto dataItem = observation->getDataItem();
      if (!dataItem->isDiscrete())
      {
        if (!observation->isUnavailable() && dataItem->isDataSet() &&
            !m_circularBuffer.getLatest().dataSetDifference(observation))
        {
          return 0;
        }
      }

      auto seqNum = m_circularBuffer.addToBuffer(observation);
      dataItem->signalObservers(seqNum);
      return seqNum;
    }

    // -------------------------------------------
    // ReST API Requests
    // -------------------------------------------

    rest_sink::Response RestService::probeRequest(const Printer *printer,
                                               const std::optional<std::string> &device)
    {
      NAMED_SCOPE("RestService::probeRequest");

      list<DevicePtr> deviceList;

      if (device)
      {
        auto dev = checkDevice(printer, *device);
        deviceList.emplace_back(dev);
      }
      else
      {
        deviceList = m_sinkContract->getDevices();
      }

      auto counts = m_sinkContract->getAssetStorage()->getCountsByType();
      return {rest_sink::status::ok,
              printer->printProbe(m_instanceId, m_circularBuffer.getBufferSize(),
                                  m_circularBuffer.getSequence(), m_sinkContract->getAssetStorage()->getMaxAssets(),
                                  m_sinkContract->getAssetStorage()->getCount(), deviceList, &counts),
              printer->mimeType()};
    }

    rest_sink::Response RestService::currentRequest(const Printer *printer,
                                                 const std::optional<std::string> &device,
                                                 const std::optional<SequenceNumber_t> &at,
                                                 const std::optional<std::string> &path)
    {
      using namespace rest_sink;
      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }
      FilterSetOpt filter;
      if (path || device)
      {
        filter = make_optional<FilterSet>();
        checkPath(printer, path, dev, *filter);
      }

      // Check if there is a frequency to stream data or not
      return {rest_sink::status::ok, fetchCurrentData(printer, filter, at), printer->mimeType()};
    }

    rest_sink::Response RestService::sampleRequest(const Printer *printer, const int count,
                                                const std::optional<std::string> &device,
                                                const std::optional<SequenceNumber_t> &from,
                                                const std::optional<SequenceNumber_t> &to,
                                                const std::optional<std::string> &path)
    {
      using namespace rest_sink;
      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }
      FilterSetOpt filter;
      if (path || device)
      {
        filter = make_optional<FilterSet>();
        checkPath(printer, path, dev, *filter);
      }

      // Check if there is a frequency to stream data or not
      SequenceNumber_t end;
      bool endOfBuffer;

      return {rest_sink::status::ok,
              fetchSampleData(printer, filter, count, from, to, end, endOfBuffer),
              printer->mimeType()};
    }

    struct AsyncSampleResponse
    {
      AsyncSampleResponse(rest_sink::SessionPtr &session, boost::asio::io_context &context)
        : m_session(session), m_observer(context), m_last(chrono::system_clock::now())
      {
      }

      rest_sink::SessionPtr m_session;
      ofstream m_log;
      SequenceNumber_t m_sequence {0};
      chrono::milliseconds m_interval;
      chrono::milliseconds m_heartbeat;
      int m_count {0};
      bool m_logStreamData {false};
      bool m_endOfBuffer {false};
      const Printer *m_printer {nullptr};
      FilterSet m_filter;
      ChangeObserver m_observer;
      chrono::system_clock::time_point m_last;
    };

    void RestService::streamSampleRequest(rest_sink::SessionPtr session, const Printer *printer,
                                    const int interval, const int heartbeatIn, const int count,
                                    const std::optional<std::string> &device,
                                    const std::optional<SequenceNumber_t> &from,
                                    const std::optional<std::string> &path)
    {
      NAMED_SCOPE("RestService::streamSampleRequest");

      using namespace rest_sink;
      using boost::placeholders::_1;
      using boost::placeholders::_2;

      checkRange(printer, interval, -1, numeric_limits<int>().max(), "interval");
      checkRange(printer, heartbeatIn, 1, numeric_limits<int>().max(), "heartbeat");
      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }

      auto asyncResponse = make_shared<AsyncSampleResponse>(session, m_context);
      asyncResponse->m_count = count;
      asyncResponse->m_printer = printer;
      asyncResponse->m_heartbeat = std::chrono::milliseconds(heartbeatIn);

      checkPath(asyncResponse->m_printer, path, dev, asyncResponse->m_filter);

      if (m_logStreamData)
      {
        stringstream filename;
        filename << "Stream_" << getCurrentTime(LOCAL) << "_" << this_thread::get_id() << ".log";
        asyncResponse->m_log.open(filename.str().c_str());
      }

      // This object will automatically clean up all the observer from the
      // signalers in an exception proof manor.
      // Add observers
      for (const auto &item : asyncResponse->m_filter)
        m_sinkContract->getDataItemById(item)->addObserver(&asyncResponse->m_observer);

      chrono::milliseconds interMilli {interval};
      SequenceNumber_t firstSeq = getFirstSequence();
      if (!from || *from < firstSeq)
        asyncResponse->m_sequence = firstSeq;
      else
        asyncResponse->m_sequence = *from;

      if (from >= m_circularBuffer.getSequence())
        asyncResponse->m_endOfBuffer = true;

      asyncResponse->m_interval = chrono::milliseconds(interval);
      asyncResponse->m_logStreamData = m_logStreamData;

      session->beginStreaming(
          printer->mimeType(),
          asio::bind_executor(m_strand,
                             boost::bind(&RestService::streamSampleWriteComplete, this, asyncResponse)));
    }

    void RestService::streamSampleWriteComplete(shared_ptr<AsyncSampleResponse> asyncResponse)
    {
      NAMED_SCOPE("RestService::streamSampleWriteComplete");

      if (asyncResponse->m_endOfBuffer)
      {
        using boost::placeholders::_1;
        using boost::placeholders::_2;

        asyncResponse->m_observer.wait(
            asyncResponse->m_heartbeat,
            asio::bind_executor(m_strand,
                               boost::bind(&RestService::streamNextSampleChunk, this, asyncResponse, _1)));
      }
      else
      {
        streamNextSampleChunk(asyncResponse, boost::system::error_code {});
      }
    }

    void RestService::streamNextSampleChunk(shared_ptr<AsyncSampleResponse> asyncResponse,
                                      boost::system::error_code ec)
    {
      NAMED_SCOPE("RestService::streamNextSampleChunk");
      using boost::placeholders::_1;
      using boost::placeholders::_2;

      if (ec && ec != boost::asio::error::operation_aborted)
      {
        LOG(warning) << "Unexpected error streamNextSampleChunk, aborting";
        LOG(warning) << ec.category().message(ec.value()) << ": " << ec.message();
        return;
      }

      if (!asyncResponse->m_endOfBuffer)
      {
        // Wait to make sure the signal was actually signaled. We have observed that
        // a signal can occur in rare conditions where there are multiple threads listening
        // on separate condition variables and this pops out too soon. This will make sure
        // observer was actually signaled and instead of throwing an error will wait again
        // for the remaining hartbeat interval.
        auto delta = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() -
                                                                 asyncResponse->m_last);
        if (delta < asyncResponse->m_interval)
        {
          asyncResponse->m_observer.wait(
              asyncResponse->m_interval - delta,
              asio::bind_executor(
                  m_strand, boost::bind(&RestService::streamNextSampleChunk, this, asyncResponse, _1)));
          return;
        }
      }

      {
        std::lock_guard<CircularBuffer> lock(m_circularBuffer);

        // See if the observer was signaled!
        if (!asyncResponse->m_observer.wasSignaled())
        {
          // If nothing came out during the last wait, we may have still have advanced
          // the sequence number. We should reset the start to something closer to the
          // current sequence. If we lock the sequence lock, we can check if the observer
          // was signaled between the time the wait timed out and the mutex was locked.
          // Otherwise, nothing has arrived and we set to the next sequence number to
          // the next sequence number to be allocated and continue.
          asyncResponse->m_sequence = m_circularBuffer.getSequence();
        }
        else
        {
          // Get the sequence # signaled in the observer when the earliest event arrived.
          // This will allow the next set of data to be pulled. Any later events will have
          // greater sequence numbers, so this should not cause a problem. Also, signaled
          // sequence numbers can only decrease, never increase.
          asyncResponse->m_sequence = asyncResponse->m_observer.getSequence();
          asyncResponse->m_observer.reset();
        }

        // Fetch sample data now resets the observer while holding the sequence
        // mutex to make sure that a new event will be recorded in the observer
        // when it returns.
        uint64_t end(0ull);
        string content;
        asyncResponse->m_endOfBuffer = true;
        // Check if we're falling too far behind. If we are, generate an
        // MTConnectError and return.
        if (asyncResponse->m_sequence < getFirstSequence())
        {
          LOG(warning) << "Client fell too far behind, disconnecting";
          return;
        }

        // end and endOfBuffer are set during the fetch sample data while the
        // mutex is held. This removed the race to check if we are at the end of
        // the bufffer and setting the next start to the last sequence number
        // sent.
        content = fetchSampleData(asyncResponse->m_printer, asyncResponse->m_filter,
                                  asyncResponse->m_count, asyncResponse->m_sequence, nullopt, end,
                                  asyncResponse->m_endOfBuffer, &asyncResponse->m_observer);

        // Even if we are at the end of the buffer, or within range. If we are filtering,
        // we will need to make sure we are not spinning when there are no valid events
        // to be reported. we will waste cycles spinning on the end of the buffer when
        // we should be in a heartbeat wait as well.
        if (!asyncResponse->m_endOfBuffer)
        {
          // If we're not at the end of the buffer, move to the end of the previous set and
          // begin filtering from where we left off.
          asyncResponse->m_sequence = end;
        }
        if (m_logStreamData)
          asyncResponse->m_log << content << endl;
        asyncResponse->m_session->writeChunk(
            content, asio::bind_executor(m_strand, boost::bind(&RestService::streamSampleWriteComplete, this,
                                                              asyncResponse)));
      }
    }

    struct AsyncCurrentResponse
    {
      AsyncCurrentResponse(rest_sink::SessionPtr session, asio::io_context &context)
        : m_session(session), m_timer(context)
      {
      }

      rest_sink::SessionPtr m_session;
      chrono::milliseconds m_interval;
      const Printer *m_printer {nullptr};
      FilterSetOpt m_filter;
      boost::asio::steady_timer m_timer;
    };

    void RestService::streamCurrentRequest(SessionPtr session, const Printer *printer, const int interval,
                                     const std::optional<std::string> &device,
                                     const std::optional<std::string> &path)
    {
      checkRange(printer, interval, 0, numeric_limits<int>().max(), "interval");
      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }

      auto asyncResponse = make_shared<AsyncCurrentResponse>(session, m_context);
      if (path || device)
      {
        asyncResponse->m_filter = make_optional<FilterSet>();
        checkPath(printer, path, dev, *asyncResponse->m_filter);
      }
      asyncResponse->m_interval = chrono::milliseconds {interval};
      asyncResponse->m_printer = printer;

      asyncResponse->m_session->beginStreaming(
          printer->mimeType(), boost::asio::bind_executor(m_strand, [this, asyncResponse]() {
            streamNextCurrent(asyncResponse, boost::system::error_code {});
          }));
    }

    void RestService::streamNextCurrent(std::shared_ptr<AsyncCurrentResponse> asyncResponse,
                                  boost::system::error_code ec)
    {
      using boost::placeholders::_1;

      asyncResponse->m_session->writeChunk(
          fetchCurrentData(asyncResponse->m_printer, asyncResponse->m_filter, nullopt),
          boost::asio::bind_executor(m_strand, [this, asyncResponse]() {
            asyncResponse->m_timer.expires_from_now(asyncResponse->m_interval);
            asyncResponse->m_timer.async_wait(boost::asio::bind_executor(
                m_strand, boost::bind(&RestService::streamNextCurrent, this, asyncResponse, _1)));
          }));
    }

    rest_sink::Response RestService::assetRequest(const Printer *printer, const int32_t count,
                                               const bool removed,
                                               const std::optional<std::string> &type,
                                               const std::optional<std::string> &device)
    {
      using namespace rest_sink;
      
      AssetList list;
      if (m_sinkContract->getAssetStorage()->getAssets(list, count, removed,
                                                       device, type) == 0)
      {
        return {status::not_found, printError(printer, "ASSET_NOT_FOUND",
                                              "Cannot find asseets"),
          printer->mimeType()};
      }
      else
      {
        return {status::ok,
          printer->printAssets(m_instanceId, m_sinkContract->getAssetStorage()->getMaxAssets(),
                               m_sinkContract->getAssetStorage()->getCount(), list),
          printer->mimeType()};
      }
    }

    Response RestService::assetIdsRequest(const Printer *printer,
                                                  const std::list<std::string> &ids)
    {
      using namespace rest_sink;

      AssetList list;
      if (m_sinkContract->getAssetStorage()->getAssets(list, ids) == 0)
      {
        return {status::not_found, printError(printer, "ASSET_NOT_FOUND",
                                              "Cannot find asseets"),
          printer->mimeType()};
      }
      else
      {
        return {status::ok,
          printer->printAssets(m_instanceId, m_sinkContract->getAssetStorage()->getMaxAssets(),
                               m_sinkContract->getAssetStorage()->getCount(), list),
          printer->mimeType()};
      }
    }

    Response RestService::putAssetRequest(const Printer *printer, const std::string &asset,
                                                  const std::optional<std::string> &type,
                                                  const std::optional<std::string> &device,
                                                  const std::optional<std::string> &uuid)
    {
      using namespace rest_sink;

      entity::ErrorList errors;
      auto dev = checkDevice(printer, *device);
      auto ap = m_loopback->receiveAsset(dev, asset, uuid, type, nullopt, errors);
      if (!ap || errors.size() > 0 || (type && ap->getType() != *type))
      {
        ProtoErrorList errorResp;
        if (!ap)
          errorResp.emplace_back("INVALID_REQUEST", "Could not parse Asset.");
        else
          errorResp.emplace_back("INVALID_REQUEST", "Asset parsed with errors.");
        for (auto &e : errors)
        {
          errorResp.emplace_back("INVALID_REQUEST", e->what());
        }
        return {status::bad_request,
                printer->printErrors(m_instanceId, m_circularBuffer.getBufferSize(),
                                     m_circularBuffer.getSequence(), errorResp),
                printer->mimeType()};
      }
      
      AssetList list {ap};
      return {status::ok,
              printer->printAssets(m_instanceId, m_sinkContract->getAssetStorage()->getMaxAssets(),
                                   m_sinkContract->getAssetStorage()->getCount(), list),
              printer->mimeType()};
    }

    rest_sink::Response RestService::deleteAssetRequest(const Printer *printer,
                                                     const std::list<std::string> &ids)
    {
      using namespace rest_sink;
      AssetList list;
      if (m_sinkContract->getAssetStorage()->getAssets(list, ids) > 0)
      {

        for (auto asset : list)
        {
          m_loopback->removeAsset(asset->getAssetId());
        }
        
        return {status::ok,
          printer->printAssets(m_instanceId, m_sinkContract->getAssetStorage()->getMaxAssets(),
                               m_sinkContract->getAssetStorage()->getCount(), list),
          printer->mimeType()};
      }
      else
      {
        return {status::not_found, printError(printer, "ASSET_NOT_FOUND",
                                              "Cannot find asseets"),
          printer->mimeType()};
      }
    }

    rest_sink::Response RestService::deleteAllAssetsRequest(const Printer *printer,
                                                         const std::optional<std::string> &device,
                                                         const std::optional<std::string> &type)
    {
      AssetList list;
      if (m_sinkContract->getAssetStorage()->getAssets(list, std::numeric_limits<size_t>().max(), false,
                                                       device, type) == 0)
      {
        return {status::not_found, printError(printer, "ASSET_NOT_FOUND",
                                              "Cannot find asseets"),
          printer->mimeType()};
      }
      else
      {
        for (auto asset : list)
        {
          m_loopback->removeAsset(asset->getAssetId());
        }

        stringstream str;
        str << "Removed " << list.size() << " assets";
        return {status::ok, str.str(), "text/plain"};
      }
    }

    rest_sink::Response RestService::putObservationRequest(const Printer *printer,
                                                        const std::string &device,
                                                        const rest_sink::QueryMap observations,
                                                        const std::optional<std::string> &time)
    {
      using namespace rest_sink;

      Timestamp ts;
      if (time)
      {
        istringstream in(*time);
        in >> std::setw(6) >> date::parse("%FT%T", ts);
        if (!in.good())
        {
          ts = chrono::system_clock::now();
        }
      }
      else
      {
        ts = chrono::system_clock::now();
      }

      auto dev = checkDevice(printer, device);

      ProtoErrorList errorResp;
      for (auto &qp : observations)
      {
        auto di = dev->getDeviceDataItem(qp.first);
        if (di == nullptr)
        {
          errorResp.emplace_back("BAD_REQUEST", "Cannot find data item: " + qp.first);
        }
        else
        {
          m_loopback->receive(di, qp.second, ts);
        }
      }

      if (errorResp.empty())
      {
        return {status::ok, "<success/>", "text/xml"};
      }
      else
      {
        return {status::not_found,
                printer->printErrors(m_instanceId, m_circularBuffer.getBufferSize(),
                                     m_circularBuffer.getSequence(), errorResp),
                printer->mimeType()};
      }
    }

      string RestService::printError(const Printer *printer, const string &errorCode,
                               const string &text) const
      {
        LOG(debug) << "Returning error " << errorCode << ": " << text;
        if (printer)
          return printer->printError(m_instanceId, m_circularBuffer.getBufferSize(),
                                     m_circularBuffer.getSequence(), errorCode, text);
        else
          return errorCode + ": " + text;
      }


  }  // namespace rest_sink

}  // namespace mtconnect
