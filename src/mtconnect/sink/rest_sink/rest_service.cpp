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

#include "rest_service.hpp"

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/pipeline/shdr_token_mapper.hpp"
#include "mtconnect/pipeline/shdr_tokenizer.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "mtconnect/printer/xml_printer.hpp"
#include "server.hpp"

namespace asio = boost::asio;
using namespace std;
namespace config = ::mtconnect::configuration;

using ptree = boost::property_tree::ptree;

namespace mtconnect {
  using namespace observation;
  using namespace asset;
  using namespace device_model;
  using namespace printer;
  using namespace buffer;

  namespace sink::rest_sink {
    RestService::RestService(asio::io_context &context, SinkContractPtr &&contract,
                             const ConfigOptions &options, const ptree &config)
      : Sink("RestService", std::move(contract)),
        m_context(context),
        m_strand(context),
        m_schemaVersion(GetOption<string>(options, config::SchemaVersion).value_or("x.y")),
        m_options(options),
        m_logStreamData(GetOption<bool>(options, config::LogStreams).value_or(false))
    {
      auto maxSize =
          ConvertFileSize(options, mtconnect::configuration::MaxCachedFileSize, 20 * 1024);
      auto compressSize =
          ConvertFileSize(options, mtconnect::configuration::MinCompressFileSize, 100 * 1024);

      m_fileCache.setMaxCachedFileSize(maxSize);
      m_fileCache.setMinCompressedFileSize(compressSize);

      // Unique id number for agent instance
      m_instanceId = getCurrentTimeInSec();

      // Get the HTTP Headers
      loadHttpHeaders(config);

      m_server = make_unique<Server>(context, m_options);
      m_server->setErrorFunction(
          [this](SessionPtr session, rest_sink::status st, const string &msg) {
            auto printer = m_sinkContract->getPrinter("xml");
            auto doc = printError(printer, "INVALID_REQUEST", msg);
            ResponsePtr resp = std::make_unique<Response>(st, doc, printer->mimeType());
            session->writeFailureResponse(std::move(resp));
          });

      auto xmlPrinter = dynamic_cast<XmlPrinter *>(m_sinkContract->getPrinter("xml"));

      // Files served by the Agent... allows schema files to be served by
      // agent.
      loadFiles(xmlPrinter, config);

      // Load namespaces, allow for local file system serving as well.
      loadNamespace(config, "DevicesNamespaces", xmlPrinter, &XmlPrinter::addDevicesNamespace);
      loadNamespace(config, "StreamsNamespaces", xmlPrinter, &XmlPrinter::addStreamsNamespace);
      loadNamespace(config, "AssetsNamespaces", xmlPrinter, &XmlPrinter::addAssetsNamespace);
      loadNamespace(config, "ErrorNamespaces", xmlPrinter, &XmlPrinter::addErrorNamespace);

      loadStyle(config, "DevicesStyle", xmlPrinter, &XmlPrinter::setDevicesStyle);
      loadStyle(config, "StreamsStyle", xmlPrinter, &XmlPrinter::setStreamStyle);
      loadStyle(config, "AssetsStyle", xmlPrinter, &XmlPrinter::setAssetsStyle);
      loadStyle(config, "ErrorStyle", xmlPrinter, &XmlPrinter::setErrorStyle);

      loadTypes(config);
      loadAllowPut();

      m_server->addParameterDocumentation(
          {{"device", PATH, "Device UUID or name"},
           {"device", QUERY, "Device UUID or name"},
           {"assetIds", PATH, "Semi-colon (;) separated list of assetIds"},
           {"removed", QUERY, "Boolean indicating if removed assets are included in results"},
           {"type", QUERY, "Only include assets of type `type` in the results"},
           {"count", QUERY, "Maximum number of entities to include in results"},
           {"assetId", QUERY, "An assetId to select"},
           {"assetId", PATH, "An assetId to select"},
           {"path", QUERY, "XPath to filter DataItems matched against the probe document"},
           {"at", QUERY, "Sequence number at which the observation snapshot is taken"},
           {"to", QUERY, "Sequence number at to stop reporting observations"},
           {"from", QUERY, "Sequence number at to start reporting observations"},
           {"interval", QUERY, "Time in ms between publishing data–starts streaming"},
           {"pretty", QUERY, "Instructs the result to be pretty printed"},
           {"heartbeat", QUERY,
            "Time in ms between publishing a empty document when no data has changed"}});

      createProbeRoutings();
      createCurrentRoutings();
      createSampleRoutings();
      createAssetRoutings();
      createPutObservationRoutings();
      createFileRoutings();

      makeLoopbackSource(m_sinkContract->m_pipelineContext);
    }

    // Register the service with the sink factory
    void RestService::registerFactory(SinkFactory &factory)
    {
      factory.registerFactory(
          "RestService",
          [](const std::string &name, boost::asio::io_context &io, SinkContractPtr &&contract,
             const ConfigOptions &options, const boost::property_tree::ptree &block) -> SinkPtr {
            auto sink = std::make_shared<RestService>(io, std::move(contract), options, block);
            return sink;
          });
    }

    void RestService::start() { m_server->start(); }

    void RestService::stop() { m_server->stop(); }

    // Configuration
    void RestService::loadNamespace(const ptree &tree, const char *namespaceType,
                                    XmlPrinter *xmlPrinter, NamespaceFunction callback)
    {
      // Load namespaces, allow for local file system serving as well.
      auto ns = tree.get_child_optional(namespaceType);
      if (ns)
      {
        for (const auto &block : *ns)
        {
          auto urn = block.second.get_optional<string>("Urn");
          if (block.first != "m" && !urn)
          {
            LOG(error) << "Name space must have a Urn: " << block.first;
          }
          else
          {
            auto location = block.second.get_optional<string>("Location").value_or("");
            (xmlPrinter->*callback)(urn.value_or(""), location, block.first);
            auto path = block.second.get_optional<string>("Path");
            if (path && !location.empty())
            {
              auto xns = m_fileCache.registerFile(location, *path, m_schemaVersion);
              if (!xns)
              {
                LOG(debug) << "Cannot register " << urn << " at " << location << " and path "
                           << *path;
              }
            }
          }
        }
      }
    }

    void RestService::loadFiles(XmlPrinter *xmlPrinter, const ptree &tree)
    {
      auto files = tree.get_child_optional("Files");
      if (files)
      {
        for (const auto &file : *files)
        {
          auto location = file.second.get_optional<string>("Location");
          auto path = file.second.get_optional<string>("Path");
          if (!location || !path)
          {
            LOG(error) << "Name space must have a Location (uri) or Directory and Path: "
                       << file.first;
          }
          else
          {
            auto resolved = m_sinkContract->m_findDataFile(*path);
            if (!resolved)
            {
              LOG(error) << "RestService loading Files: Cannot resolve path: " << *path
                         << " in data path";
            }
            else
            {
              auto namespaces = m_fileCache.registerFiles(*location, *resolved, m_schemaVersion);
              for (auto &ns : namespaces)
              {
                if (ns.first.find("Devices") != string::npos)
                {
                  xmlPrinter->addDevicesNamespace(ns.first, ns.second, "m");
                }
                else if (ns.first.find("Streams") != string::npos)
                {
                  xmlPrinter->addStreamsNamespace(ns.first, ns.second, "m");
                }
                else if (ns.first.find("Assets") != string::npos)
                {
                  xmlPrinter->addAssetsNamespace(ns.first, ns.second, "m");
                }
                else if (ns.first.find("Error") != string::npos)
                {
                  xmlPrinter->addErrorNamespace(ns.first, ns.second, "m");
                }
              }
            }
          }
        }
      }

      auto dirs = tree.get_child_optional("Directories");
      if (dirs)
      {
        for (const auto &dir : *dirs)
        {
          auto location = dir.second.get_optional<string>("Location");
          auto path = dir.second.get_optional<string>("Path");
          auto index = dir.second.get_optional<string>("Default");
          if (!location || !path)
          {
            LOG(error) << "Name space must have a Location (uri) or Directory and Path: "
                       << dir.first;
          }
          else
          {
            auto resolved = m_sinkContract->m_findDataFile(*path);
            if (!resolved)
            {
              LOG(error) << "RestService loading Directories: Cannot resolve path: " << *path
                         << " in data path";
            }
            else
            {
              string ind {index ? *index : "index.html"};
              m_fileCache.addDirectory(*location, resolved->string(), ind);
            }
          }
        }
      }
    }

    void RestService::loadHttpHeaders(const ptree &tree)
    {
      auto headers = tree.get_child_optional(config::HttpHeaders);
      if (headers)
      {
        StringList fields;
        for (auto &f : *headers)
        {
          fields.emplace_back(f.first + ": " + f.second.data());
        }

        m_options[config::HttpHeaders] = fields;
      }
    }

    void RestService::loadStyle(const ptree &tree, const char *styleName, XmlPrinter *xmlPrinter,
                                StyleFunction styleFunction)
    {
      auto style = tree.get_child_optional(styleName);
      if (style)
      {
        auto location = style->get_optional<string>("Location");
        if (!location)
        {
          LOG(error) << "A style must have a Location: " << styleName;
        }
        else
        {
          (xmlPrinter->*styleFunction)(*location);
          auto path = style->get_optional<string>("Path");
          if (path)
          {
            m_fileCache.registerFile(*location, *path, m_schemaVersion);
          }
        }
      }
    }

    void RestService::loadTypes(const ptree &tree)
    {
      auto types = tree.get_child_optional("MimeTypes");
      if (types)
      {
        for (const auto &type : *types)
        {
          m_fileCache.addMimeType(type.first, type.second.data());
        }
      }
    }

    void RestService::loadAllowPut()
    {
      namespace asio = boost::asio;
      namespace ip = asio::ip;

      m_server->allowPuts(get<bool>(m_options[config::AllowPut]));
      auto hosts = GetOption<string>(m_options, config::AllowPutFrom);
      if (hosts && !hosts->empty())
      {
        istringstream line(*hosts);
        do
        {
          string host;
          getline(line, host, ',');
          host = trim(host);
          if (!host.empty())
          {
            // Check if it is a simple numeric address
            using br = ip::resolver_base;
            boost::system::error_code ec;
            auto addr = ip::make_address(host, ec);
            if (ec)
            {
              ip::tcp::resolver resolver(m_context);
              ip::tcp::resolver::query query(host, "0", br::v4_mapped);

              auto it = resolver.resolve(query, ec);
              if (ec)
              {
                cout << "Failed to resolve " << host << ": " << ec.message() << endl;
              }
              else
              {
                ip::tcp::resolver::iterator end;
                for (; it != end; it++)
                {
                  const auto &addr = it->endpoint().address();
                  if (!addr.is_multicast() && !addr.is_unspecified())
                  {
                    m_server->allowPutFrom(addr.to_string());
                  }
                }
              }
            }
            else
            {
              m_server->allowPutFrom(addr.to_string());
            }
          }
        } while (!line.eof());
      }
    }

    std::shared_ptr<source::LoopbackSource> RestService::makeLoopbackSource(
        pipeline::PipelineContextPtr context)
    {
      using namespace pipeline;
      using namespace source;

      m_loopback = make_shared<LoopbackSource>("RestSource", m_strand, context, m_options);
      auto pipeline = m_loopback->getPipeline();

      auto tokenizer = make_shared<ShdrTokenizer>();
      if (!pipeline->spliceBefore("UpcaseValue", tokenizer))
        pipeline->spliceBefore("DuplicateFilter", tokenizer);

      pipeline->spliceAfter("ShdrTokenizer", make_shared<ExtractTimestamp>(false));

      auto mapper = make_shared<ShdrTokenMapper>(context, "", 2);
      pipeline->spliceAfter("ExtractTimestamp", mapper);
      mapper->bind(make_shared<NullTransform>(TypeGuard<Observations>(RUN)));

      // Reattach the first in the chain to catch non-data entities
      auto next = mapper->getNext().front();
      pipeline->lastAfter("Start", next);

      m_sinkContract->addSource(m_loopback);
      return m_loopback;
    }

    // -----------------------------------------------------------
    // Request Routings
    // -----------------------------------------------------------

    static inline void respond(rest_sink::SessionPtr session, rest_sink::ResponsePtr &&response)
    {
      session->writeResponse(std::move(response));
    }

    void RestService::createFileRoutings()
    {
      using namespace rest_sink;
      auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto file = m_fileCache.getFile(request->m_path, request->m_acceptsEncoding, &m_context);
        if (file)
        {
          if (file->m_redirect)
          {
            ResponsePtr response = make_unique<Response>(rest_sink::status::permanent_redirect,
                                                         file->m_buffer, file->m_mimeType);
            response->m_location = *file->m_redirect;
            session->writeResponse(std::move(response));
          }
          else
          {
            ResponsePtr response = make_unique<Response>(rest_sink::status::ok, file);
            session->writeResponse(std::move(response));
          }
        }
        return bool(file);
      };
      m_server->addRouting({boost::beast::http::verb::get, regex("/.+"), handler});
    }

    void RestService::createProbeRoutings()
    {
      using namespace rest_sink;
      // Probe
      auto handler = [&](SessionPtr session, const RequestPtr request) -> bool {
        auto device = request->parameter<string>("device");
        auto pretty = *request->parameter<bool>("pretty");

        auto printer = printerForAccepts(request->m_accepts);

        if (device && !ends_with(request->m_path, string("probe")) &&
            m_sinkContract->findDeviceByUUIDorName(*device) == nullptr)
          return false;

        respond(session, probeRequest(printer, device, pretty));
        return true;
      };

      m_server->addRouting({boost::beast::http::verb::get, "/probe?pretty={bool:false}", handler})
          .document("MTConnect probe request",
                    "Provides metadata service for the MTConnect Devices information model for all "
                    "devices.");
      m_server
          ->addRouting(
              {boost::beast::http::verb::get, "/{device}/probe?pretty={bool:false}", handler})
          .document("MTConnect probe request",
                    "Provides metadata service for the MTConnect Devices information model for "
                    "device identified by `device` matching `name` or `uuid`.");

      // Must be last
      m_server->addRouting({boost::beast::http::verb::get, "/?pretty={bool:false}", handler})
          .document("MTConnect probe request",
                    "Provides metadata service for the MTConnect Devices information model for all "
                    "devices.");
      m_server
          ->addRouting({boost::beast::http::verb::get, "/{device}?pretty={bool:false}", handler})
          .document("MTConnect probe request",
                    "Provides metadata service for the MTConnect Devices information model for "
                    "device identified by `device` matching `name` or `uuid`.");
    }

    void RestService::createAssetRoutings()
    {
      using namespace rest_sink;
      auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto removed = *request->parameter<bool>("removed");
        auto count = *request->parameter<int32_t>("count");
        auto printer = printerForAccepts(request->m_accepts);

        respond(session, assetRequest(printer, count, removed, request->parameter<string>("type"),
                                      request->parameter<string>("device")));
        return true;
      };

      auto idHandler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto asset = request->parameter<string>("assetIds");
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
          respond(session, make_unique<Response>(rest_sink::status::bad_request, error,
                                                 printer->mimeType()));
        }
        return true;
      };

      string qp(
          "type={string}&removed={bool:false}&"
          "count={integer:100}&device={string}&pretty={bool:false}");
      m_server->addRouting({boost::beast::http::verb::get, "/assets?" + qp, handler})
          .document("MTConnect assets request", "Returns up to `count` assets");
      m_server->addRouting({boost::beast::http::verb::get, "/asset?" + qp, handler})
          .document("MTConnect asset request", "Returns up to `count` assets");
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/assets?" + qp, handler})
          .document("MTConnect assets request", "Returns up to `count` assets for deivce `device`");
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/asset?" + qp, handler})
          .document("MTConnect asset request", "Returns up to `count` assets for deivce `device`");
      m_server->addRouting({boost::beast::http::verb::get, "/assets/{assetIds}", idHandler})
          .document(
              "MTConnect assets request",
              "Returns a set assets identified by asset ids `asset` separated by semi-colon (;)");
      m_server->addRouting({boost::beast::http::verb::get, "/asset/{assetIds}", idHandler})
          .document("MTConnect asset request",
                    "Returns a set of assets identified by asset ids `asset` separated by "
                    "semi-colon (;)");

      if (m_server->arePutsAllowed())
      {
        auto putHandler = [&](SessionPtr session, RequestPtr request) -> bool {
          auto printer = printerForAccepts(request->m_accepts);
          respond(session,
                  putAssetRequest(printer, request->m_body, request->parameter<string>("type"),
                                  request->parameter<string>("device"),
                                  request->parameter<string>("assetId")));
          return true;
        };

        auto deleteHandler = [&](SessionPtr session, RequestPtr request) -> bool {
          auto asset = request->parameter<string>("assetId");
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

        for (const auto &asset : list<string> {"asset", "assets"})
        {
          for (const auto &t : list<boost::beast::http::verb> {boost::beast::http::verb::put,
                                                               boost::beast::http::verb::post})
          {
            m_server
                ->addRouting(
                    {t, "/" + asset + "/{assetId}?device={string}&type={string}", putHandler})
                .document("Upload an asset by identified by `assetId`",
                          "Updates or adds an asset with the asset XML in the body");
            m_server->addRouting({t, "/" + asset + "?device={string}&type={string}", putHandler})
                .document("Upload an asset by identified by `assetId`",
                          "Updates or adds an asset with the asset XML in the body");
            m_server->addRouting({t, "/{device}/" + asset + "/{assetId}?type={string}", putHandler})
                .document("Upload an asset by identified by `assetId`",
                          "Updates or adds an asset with the asset XML in the body");
            m_server->addRouting({t, "/{device}/" + asset + "?type={string}", putHandler})
                .document("Upload an asset by identified by `assetId`",
                          "Updates or adds an asset with the asset XML in the body");
          }

          m_server
              ->addRouting({boost::beast::http::verb::delete_,
                            "/" + asset + "?device={string}&type={string}", deleteHandler})
              .document("Delete all assets for a device and type",
                        "Device and type are optional. If they are not given, it assumes there is "
                        "no constraint");
          m_server
              ->addRouting(
                  {boost::beast::http::verb::delete_, "/" + asset + "/{assetId}", deleteHandler})
              .document("Delete asset identified by `assetId`",
                        "Marks the asset as removed and creates an AssetRemoved event");
          m_server
              ->addRouting({boost::beast::http::verb::delete_,
                            "/{device}/" + asset + "?type={string}", deleteHandler})
              .document("Delete all assets for a device and type",
                        "Device and type are optional. If they are not given, it assumes there is "
                        "no constraint");
        }
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
                               request->parameter<string>("path"),
                               *request->parameter<bool>("pretty"));
        }
        else
        {
          respond(session, currentRequest(printerForAccepts(request->m_accepts),
                                          request->parameter<string>("device"),
                                          request->parameter<uint64_t>("at"),
                                          request->parameter<string>("path"),
                                          *request->parameter<bool>("pretty")));
        }
        return true;
      };

      string qp(
          "path={string}&at={unsigned_integer}&"
          "interval={integer}&pretty={bool:false}");
      m_server->addRouting({boost::beast::http::verb::get, "/current?" + qp, handler})
          .document("MTConnect current request",
                    "Gets a stapshot of the state of all the observations for all devices "
                    "optionally filtered by the `path`");
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/current?" + qp, handler})
          .document("MTConnect current request",
                    "Gets a stapshot of the state of all the observations for device `device` "
                    "optionally filtered by the `path`");
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
              request->parameter<string>("path"), *request->parameter<bool>("pretty"));
        }
        else
        {
          respond(session,
                  sampleRequest(
                      printerForAccepts(request->m_accepts), *request->parameter<int32_t>("count"),
                      request->parameter<string>("device"), request->parameter<uint64_t>("from"),
                      request->parameter<uint64_t>("to"), request->parameter<string>("path"),
                      *request->parameter<bool>("pretty")));
        }
        return true;
      };

      string qp(
          "path={string}&from={unsigned_integer}&"
          "interval={integer}&count={integer:100}&"
          "heartbeat={integer:10000}&to={unsigned_integer}&"
          "pretty={bool:false}");
      m_server->addRouting({boost::beast::http::verb::get, "/sample?" + qp, handler})
          .document("MTConnect sample request",
                    "Gets a time series of at maximum `count` observations for all devices "
                    "optionally filtered by the `path` and starting at `from`. By default, from is "
                    "the first available observation known to the agent");
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/sample?" + qp, handler})
          .document("MTConnect sample request",
                    "Gets a time series of at maximum `count` observations for device `device` "
                    "optionally filtered by the `path` and starting at `from`. By default, from is "
                    "the first available observation known to the agent");
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

        m_server->addRouting({boost::beast::http::verb::put, "/{device}?time={string}", handler})
            .document("Non-normative PUT to update a value in the agent",
                      "The data of the PUT contains the dataItem=value observation data");
        m_server->addRouting({boost::beast::http::verb::post, "/{device}?time={string}", handler})
            .document("Non-normative POST to update a value in the agent",
                      "The data of the POST contains the dataItem=value observation data");
      }
    }

    // ----------------------------------------------------
    // Observation Add Method
    // ----------------------------------------------------

    bool RestService::publish(ObservationPtr &observation)
    {
      if (observation->isOrphan())
        return false;

      auto dataItem = observation->getDataItem();
      auto seqNum = observation->getSequence();
      dataItem->signalObservers(seqNum);
      return true;
    }

    // -------------------------------------------
    // ReST API Requests
    // -------------------------------------------

    ResponsePtr RestService::probeRequest(const Printer *printer,
                                          const std::optional<std::string> &device, bool pretty)
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

      return make_unique<Response>(
          rest_sink::status::ok,
          printer->printProbe(m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
                              m_sinkContract->getCircularBuffer().getSequence(),
                              uint32_t(m_sinkContract->getAssetStorage()->getMaxAssets()),
                              uint32_t(m_sinkContract->getAssetStorage()->getCount()), deviceList,
                              &counts, false, pretty),
          printer->mimeType());
    }

    ResponsePtr RestService::currentRequest(const Printer *printer,
                                            const std::optional<std::string> &device,
                                            const std::optional<SequenceNumber_t> &at,
                                            const std::optional<std::string> &path, bool pretty)
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
      return make_unique<Response>(rest_sink::status::ok,
                                   fetchCurrentData(printer, filter, at, pretty),
                                   printer->mimeType());
    }

    ResponsePtr RestService::sampleRequest(const Printer *printer, const int count,
                                           const std::optional<std::string> &device,
                                           const std::optional<SequenceNumber_t> &from,
                                           const std::optional<SequenceNumber_t> &to,
                                           const std::optional<std::string> &path, bool pretty)
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

      return make_unique<Response>(
          rest_sink::status::ok,
          fetchSampleData(printer, filter, count, from, to, end, endOfBuffer, nullptr, pretty),
          printer->mimeType());
    }

    struct AsyncSampleResponse
    {
      AsyncSampleResponse(rest_sink::SessionPtr &session, boost::asio::io_context::strand &strand)
        : m_session(session),
          m_observer(strand),
          m_last(chrono::system_clock::now()),
          m_timer(strand.context())
      {}

      std::weak_ptr<Sink> m_service;
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
      boost::asio::steady_timer m_timer;
      bool m_pretty {false};
    };

    void RestService::streamSampleRequest(rest_sink::SessionPtr session, const Printer *printer,
                                          const int interval, const int heartbeatIn,
                                          const int count, const std::optional<std::string> &device,
                                          const std::optional<SequenceNumber_t> &from,
                                          const std::optional<std::string> &path, bool pretty)
    {
      NAMED_SCOPE("RestService::streamSampleRequest");

      using namespace rest_sink;
      using std::placeholders::_1;
      using std::placeholders::_2;

      checkRange(printer, interval, -1, numeric_limits<int>().max(), "interval");
      checkRange(printer, heartbeatIn, 1, numeric_limits<int>().max(), "heartbeat");
      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }

      auto asyncResponse = make_shared<AsyncSampleResponse>(session, m_strand);
      asyncResponse->m_count = count;
      asyncResponse->m_printer = printer;
      asyncResponse->m_heartbeat = std::chrono::milliseconds(heartbeatIn);
      asyncResponse->m_service = getptr();
      asyncResponse->m_pretty = pretty;

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
      {
        auto di = m_sinkContract->getDataItemById(item);
        if (di)
          di->addObserver(&asyncResponse->m_observer);
      }

      chrono::milliseconds interMilli {interval};
      SequenceNumber_t firstSeq = m_sinkContract->getCircularBuffer().getFirstSequence();

      if (!from || *from < firstSeq)
        asyncResponse->m_sequence = firstSeq;
      else
        asyncResponse->m_sequence = *from;

      asyncResponse->m_endOfBuffer = from >= m_sinkContract->getCircularBuffer().getSequence();

      asyncResponse->m_interval = chrono::milliseconds(interval);
      asyncResponse->m_logStreamData = m_logStreamData;

      session->beginStreaming(
          printer->mimeType(),
          asio::bind_executor(
              m_strand, boost::bind(&RestService::streamSampleWriteComplete, this, asyncResponse)));
    }

    void RestService::streamSampleWriteComplete(shared_ptr<AsyncSampleResponse> asyncResponse)
    {
      NAMED_SCOPE("RestService::streamSampleWriteComplete");

      asyncResponse->m_last = chrono::system_clock::now();
      if (asyncResponse->m_endOfBuffer)
      {
        using std::placeholders::_1;
        using std::placeholders::_2;

        asyncResponse->m_observer.wait(
            asyncResponse->m_heartbeat,
            asio::bind_executor(m_strand, boost::bind(&RestService::streamNextSampleChunk, this,
                                                      asyncResponse, _1)));
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
      using std::placeholders::_1;
      using std::placeholders::_2;

      auto service = asyncResponse->m_service.lock();

      if (!service || !m_server || !m_server->isRunning())
      {
        LOG(warning) << "Trying to send chunk when service has stopped";
        if (service)
        {
          asyncResponse->m_session->fail(boost::beast::http::status::internal_server_error,
                                         "Agent shutting down, aborting stream");
        }
        return;
      }

      if (ec && ec != boost::asio::error::operation_aborted)
      {
        LOG(warning) << "Unexpected error streamNextSampleChunk, aborting";
        LOG(warning) << ec.category().message(ec.value()) << ": " << ec.message();
        asyncResponse->m_session->fail(boost::beast::http::status::internal_server_error,
                                       "Unexpected error streamNextSampleChunk, aborting");
        return;
      }

      {
        std::lock_guard<CircularBuffer> lock(m_sinkContract->getCircularBuffer());

        if (!asyncResponse->m_endOfBuffer)
        {
          // Check if we are streaming chunks rapidly to catch up to the end of
          // buffer. We will not delay between chunks in this case and write as
          // rapidly as possible
        }
        else if (!asyncResponse->m_observer.wasSignaled())
        {
          // If nothing came out during the last wait, we may have still have advanced
          // the sequence number. We should reset the start to something closer to the
          // current sequence. If we lock the sequence lock, we can check if the observer
          // was signaled between the time the wait timed out and the mutex was locked.
          // Otherwise, nothing has arrived and we set to the next sequence number to
          // the next sequence number to be allocated and continue.
          asyncResponse->m_sequence = m_sinkContract->getCircularBuffer().getSequence();
        }
        else
        {
          // The observer can be signaled before the interval has expired. If this occurs, then
          // Wait the remaining duration of the interval.
          auto delta = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() -
                                                                   asyncResponse->m_last);
          if (delta < asyncResponse->m_interval)
          {
            asyncResponse->m_timer.expires_from_now(asyncResponse->m_interval - delta);
            asyncResponse->m_timer.async_wait(asio::bind_executor(
                m_strand,
                boost::bind(&RestService::streamNextSampleChunk, this, asyncResponse, _1)));
            return;
          }

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
        if (asyncResponse->m_sequence < m_sinkContract->getCircularBuffer().getFirstSequence())
        {
          LOG(warning) << "Client fell too far behind, disconnecting";
          asyncResponse->m_session->fail(boost::beast::http::status::not_found,
                                         "Client fell too far behind, disconnecting");
          return;
        }

        // end and endOfBuffer are set during the fetch sample data while the
        // mutex is held. This removed the race to check if we are at the end of
        // the bufffer and setting the next start to the last sequence number
        // sent.
        content = fetchSampleData(asyncResponse->m_printer, asyncResponse->m_filter,
                                  asyncResponse->m_count, asyncResponse->m_sequence, nullopt, end,
                                  asyncResponse->m_endOfBuffer, &asyncResponse->m_observer,
                                  asyncResponse->m_pretty);

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
            content,
            asio::bind_executor(m_strand, boost::bind(&RestService::streamSampleWriteComplete, this,
                                                      asyncResponse)));
      }
    }

    struct AsyncCurrentResponse
    {
      AsyncCurrentResponse(rest_sink::SessionPtr session, asio::io_context &context)
        : m_session(session), m_timer(context)
      {}

      std::weak_ptr<Sink> m_service;
      rest_sink::SessionPtr m_session;
      chrono::milliseconds m_interval;
      const Printer *m_printer {nullptr};
      FilterSetOpt m_filter;
      boost::asio::steady_timer m_timer;
      bool m_pretty {false};
    };

    void RestService::streamCurrentRequest(SessionPtr session, const Printer *printer,
                                           const int interval,
                                           const std::optional<std::string> &device,
                                           const std::optional<std::string> &path, bool pretty)
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
      asyncResponse->m_service = getptr();
      asyncResponse->m_pretty = pretty;

      asyncResponse->m_session->beginStreaming(
          printer->mimeType(), boost::asio::bind_executor(m_strand, [this, asyncResponse]() {
            streamNextCurrent(asyncResponse, boost::system::error_code {});
          }));
    }

    void RestService::streamNextCurrent(std::shared_ptr<AsyncCurrentResponse> asyncResponse,
                                        boost::system::error_code ec)
    {
      using std::placeholders::_1;

      auto service = asyncResponse->m_service.lock();

      if (!service || !m_server || !m_server->isRunning())
      {
        LOG(warning) << "Trying to send chunk when service has stopped";
        if (service)
        {
          asyncResponse->m_session->fail(boost::beast::http::status::internal_server_error,
                                         "Agent shutting down, aborting stream");
        }
        return;
      }

      if (ec && ec != boost::asio::error::operation_aborted)
      {
        LOG(warning) << "Unexpected error streamNextCurrent, aborting";
        LOG(warning) << ec.category().message(ec.value()) << ": " << ec.message();
        asyncResponse->m_session->fail(boost::beast::http::status::internal_server_error,
                                       "Unexpected error streamNextCurrent, aborting");
        return;
      }

      asyncResponse->m_session->writeChunk(
          fetchCurrentData(asyncResponse->m_printer, asyncResponse->m_filter, nullopt,
                           asyncResponse->m_pretty),
          boost::asio::bind_executor(m_strand, [this, asyncResponse]() {
            asyncResponse->m_timer.expires_from_now(asyncResponse->m_interval);
            asyncResponse->m_timer.async_wait(boost::asio::bind_executor(
                m_strand, boost::bind(&RestService::streamNextCurrent, this, asyncResponse, _1)));
          }));
    }

    ResponsePtr RestService::assetRequest(const Printer *printer, const int32_t count,
                                          const bool removed,
                                          const std::optional<std::string> &type,
                                          const std::optional<std::string> &device, bool pretty)
    {
      using namespace rest_sink;

      AssetList list;
      optional<string> uuid;
      if (device)
      {
        auto d = m_sinkContract->findDeviceByUUIDorName(*device);
        if (d)
          uuid = d->getUuid();
      }

      m_sinkContract->getAssetStorage()->getAssets(list, count, !removed, uuid, type);
      return make_unique<Response>(
          status::ok,
          printer->printAssets(
              m_instanceId, uint32_t(m_sinkContract->getAssetStorage()->getMaxAssets()),
              uint32_t(m_sinkContract->getAssetStorage()->getCount()), list, pretty),
          printer->mimeType());
    }

    ResponsePtr RestService::assetIdsRequest(const Printer *printer,
                                             const std::list<std::string> &ids, bool pretty)
    {
      using namespace rest_sink;

      AssetList list;
      if (m_sinkContract->getAssetStorage()->getAssets(list, ids) == 0)
      {
        stringstream str;
        str << "Cannot find asset for asset Ids: ";
        for (auto &id : ids)
          str << id << ", ";

        auto message = str.str().substr(0, str.str().size() - 2);
        return make_unique<Response>(status::not_found,
                                     printError(printer, "ASSET_NOT_FOUND", message, pretty),
                                     printer->mimeType());
      }
      else
      {
        return make_unique<Response>(
            status::ok,
            printer->printAssets(
                m_instanceId, uint32_t(m_sinkContract->getAssetStorage()->getMaxAssets()),
                uint32_t(m_sinkContract->getAssetStorage()->getCount()), list, pretty),
            printer->mimeType());
      }
    }

    ResponsePtr RestService::putAssetRequest(const Printer *printer, const std::string &asset,
                                             const std::optional<std::string> &type,
                                             const std::optional<std::string> &device,
                                             const std::optional<std::string> &uuid)
    {
      using namespace rest_sink;

      entity::ErrorList errors;
      DevicePtr dev;
      if (device)
        dev = checkDevice(printer, *device);
      else
        dev = m_sinkContract->getDefaultDevice();
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
        return make_unique<Response>(
            status::bad_request,
            printer->printErrors(m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
                                 m_sinkContract->getCircularBuffer().getSequence(), errorResp),
            printer->mimeType());
      }

      AssetList list {ap};
      return make_unique<Response>(
          status::ok,
          printer->printAssets(m_instanceId,
                               uint32_t(m_sinkContract->getAssetStorage()->getMaxAssets()),
                               uint32_t(m_sinkContract->getAssetStorage()->getCount()), list),
          printer->mimeType());
    }

    ResponsePtr RestService::deleteAssetRequest(const Printer *printer,
                                                const std::list<std::string> &ids)
    {
      using namespace rest_sink;
      AssetList list;
      if (m_sinkContract->getAssetStorage()->getAssets(list, ids) > 0)
      {
        for (auto asset : list)
        {
          m_loopback->removeAsset(asset->getDeviceUuid(), asset->getAssetId());
        }

        return make_unique<Response>(
            status::ok,
            printer->printAssets(m_instanceId,
                                 uint32_t(m_sinkContract->getAssetStorage()->getMaxAssets()),
                                 uint32_t(m_sinkContract->getAssetStorage()->getCount()), list),
            printer->mimeType());
      }
      else
      {
        return make_unique<Response>(status::not_found,
                                     printError(printer, "ASSET_NOT_FOUND", "Cannot find assets"),
                                     printer->mimeType());
      }
    }

    ResponsePtr RestService::deleteAllAssetsRequest(const Printer *printer,
                                                    const std::optional<std::string> &device,
                                                    const std::optional<std::string> &type)
    {
      AssetList list;
      if (m_sinkContract->getAssetStorage()->getAssets(list, std::numeric_limits<size_t>().max(),
                                                       true, device, type) == 0)
      {
        return make_unique<Response>(status::not_found,
                                     printError(printer, "ASSET_NOT_FOUND", "Cannot find assets"),
                                     printer->mimeType());
      }
      else
      {
        for (auto asset : list)
        {
          m_loopback->removeAsset(asset->getDeviceUuid(), asset->getAssetId());
        }

        stringstream str;
        str << "Removed " << list.size() << " assets";
        return make_unique<Response>(status::ok, str.str(), "text/plain");
      }
    }

    ResponsePtr RestService::putObservationRequest(const Printer *printer,
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
          if (qp.second.find_first_of('|') != string::npos)
          {
            stringstream ss;
            if (time)
              ss << *time;
            ss << '|' << di->getId() << '|' << qp.second;
            m_loopback->receive(ss.str());
          }
          else
          {
            m_loopback->receive(di, qp.second, ts);
          }
        }
      }

      if (errorResp.empty())
      {
        return make_unique<Response>(status::ok, "<success/>", "text/xml");
      }
      else
      {
        return make_unique<Response>(
            status::not_found,
            printer->printErrors(m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
                                 m_sinkContract->getCircularBuffer().getSequence(), errorResp),
            printer->mimeType());
      }
    }

    // For debugging
    void RestService::setLogStreamData(bool log) { m_logStreamData = log; }

    // Get the printer for a type
    const std::string RestService::acceptFormat(const std::string &accepts) const
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

    const Printer *RestService::printerForAccepts(const std::string &accepts) const
    {
      return m_sinkContract->getPrinter(acceptFormat(accepts));
    }

    string RestService::printError(const Printer *printer, const string &errorCode,
                                   const string &text, bool pretty) const
    {
      LOG(debug) << "Returning error " << errorCode << ": " << text;
      if (printer)
        return printer->printError(
            m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
            m_sinkContract->getCircularBuffer().getSequence(), errorCode, text, pretty);
      else
        return errorCode + ": " + text;
    }

    // -----------------------------------------------
    // Validation methods
    // -----------------------------------------------

    template <typename T>
    void RestService::checkRange(const Printer *printer, const T value, const T min, const T max,
                                 const string &param, bool notZero) const
    {
      if (value <= min)
      {
        stringstream str;
        str << '\'' << param << '\'' << " must be greater than " << min;
        throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                           printer->mimeType(), status::bad_request);
      }
      if (value >= max)
      {
        stringstream str;
        str << '\'' << param << '\'' << " must be less than " << max;
        throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                           printer->mimeType(), status::bad_request);
      }
      if (notZero && value == 0)
      {
        stringstream str;
        str << '\'' << param << '\'' << " must not be zero(0)";
        throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                           printer->mimeType(), status::bad_request);
      }
    }

    void RestService::checkPath(const Printer *printer, const std::optional<std::string> &path,
                                const DevicePtr device, FilterSet &filter) const
    {
      try
      {
        m_sinkContract->getDataItemsForPath(device, path, filter);
      }
      catch (exception &e)
      {
        throw RequestError(e.what(), printError(printer, "INVALID_XPATH", e.what()),
                           printer->mimeType(), status::bad_request);
      }

      if (filter.empty())
      {
        string msg = "The path could not be parsed. Invalid syntax: " + *path;
        throw RequestError(msg.c_str(), printError(printer, "INVALID_XPATH", msg),
                           printer->mimeType(), status::bad_request);
      }
    }

    DevicePtr RestService::checkDevice(const Printer *printer, const std::string &uuid) const
    {
      auto dev = m_sinkContract->findDeviceByUUIDorName(uuid);
      if (!dev)
      {
        string msg("Could not find the device '" + uuid + "'");
        throw RequestError(msg.c_str(), printError(printer, "NO_DEVICE", msg), printer->mimeType(),
                           status::not_found);
      }

      return dev;
    }

    // -------------------------------------------
    // Data Collection and Formatting
    // -------------------------------------------

    string RestService::fetchCurrentData(const Printer *printer, const FilterSetOpt &filterSet,
                                         const optional<SequenceNumber_t> &at, bool pretty)
    {
      ObservationList observations;
      SequenceNumber_t firstSeq, seq;

      {
        std::lock_guard<CircularBuffer> lock(m_sinkContract->getCircularBuffer());

        firstSeq = m_sinkContract->getCircularBuffer().getFirstSequence();
        seq = m_sinkContract->getCircularBuffer().getSequence();
        if (at)
        {
          checkRange(printer, *at, firstSeq - 1, seq, "at");

          auto check = m_sinkContract->getCircularBuffer().getCheckpointAt(*at, filterSet);
          check->getObservations(observations);
        }
        else
        {
          m_sinkContract->getCircularBuffer().getLatest().getObservations(observations, filterSet);
        }
      }

      return printer->printSample(m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
                                  seq, firstSeq, seq - 1, observations, pretty);
    }

    string RestService::fetchSampleData(const Printer *printer, const FilterSetOpt &filterSet,
                                        int count, const std::optional<SequenceNumber_t> &from,
                                        const std::optional<SequenceNumber_t> &to,
                                        SequenceNumber_t &end, bool &endOfBuffer,
                                        ChangeObserver *observer, bool pretty)
    {
      std::unique_ptr<ObservationList> observations;
      SequenceNumber_t firstSeq, lastSeq;

      {
        std::lock_guard<CircularBuffer> lock(m_sinkContract->getCircularBuffer());
        firstSeq = m_sinkContract->getCircularBuffer().getFirstSequence();
        auto seq = m_sinkContract->getCircularBuffer().getSequence();
        lastSeq = seq - 1;
        int upperCountLimit = m_sinkContract->getCircularBuffer().getBufferSize() + 1;
        int lowerCountLimit = -upperCountLimit;

        if (from)
        {
          checkRange(printer, *from, firstSeq - 1, seq + 1, "from");
        }
        if (to)
        {
          auto lower = from ? *from : firstSeq;
          checkRange(printer, *to, lower, seq + 1, "to");
          lowerCountLimit = 0;
        }
        checkRange(printer, count, lowerCountLimit, upperCountLimit, "count", true);

        observations = m_sinkContract->getCircularBuffer().getObservations(
            count, filterSet, from, to, end, firstSeq, endOfBuffer);

        if (observer)
          observer->reset();
      }

      return printer->printSample(m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
                                  end, firstSeq, lastSeq, *observations, pretty);
    }

  }  // namespace sink::rest_sink
}  // namespace mtconnect
