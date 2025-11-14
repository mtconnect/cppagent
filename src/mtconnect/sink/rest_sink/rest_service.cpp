//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <regex>

#include "error.hpp"
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
      using placeholders::_1;
      using placeholders::_2;

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
      m_server->setErrorFunction(boost::bind(&RestService::writeErrorResponse, this, _1, _2));

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
           {"deviceType", QUERY,
            "Values are 'Device' or 'Agent'. Selects only devices of that type."},
           {"assetId", PATH, "An assetId to select"},
           {"path", QUERY, "XPath to filter DataItems matched against the probe document"},
           {"at", QUERY, "Sequence number at which the observation snapshot is taken"},
           {"to", QUERY, "Sequence number at to stop reporting observations"},
           {"from", QUERY, "Sequence number at to start reporting observations"},
           {"interval", QUERY, "Time in ms between publishing data–starts streaming"},
           {"pretty", QUERY, "Instructs the result to be pretty printed"},
           {"format", QUERY, "The format of the response document: 'xml' or 'json'"},
           {"heartbeat", QUERY,
            "Time in ms between publishing a empty document when no data has changed"},
           {"id", PATH, "webservice request id"}});

      createCurrentRoutings();
      createSampleRoutings();
      createAssetRoutings();
      createProbeRoutings();
      createPutObservationRoutings();
      createFileRoutings();
      m_server->addCommands();

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
      namespace fs = std::filesystem;
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
          auto configPath = style->get_optional<string>("Path");
          if (configPath)
          {
            m_fileCache.registerFile(*location, *configPath, m_schemaVersion);
          }

          if (auto fc = m_fileCache.getFile(*location))
          {
            try
            {
              unique_ptr<char[]> buffer(new char[fc->m_size]);
              std::filebuf file;
              if (file.open(fc->m_path, std::ios::binary | std::ios::in) == nullptr)
                throw std::runtime_error("Cannot open file for reading");

              auto len = file.sgetn(buffer.get(), fc->m_size);
              file.close();
              if (len <= 0)
                throw std::runtime_error("Cannot read from file");

              string_view sv(buffer.get(), len);

              std::ofstream out(fc->m_path, std::ios::binary | std::ios_base::out);
              if (!out.is_open())
                throw std::runtime_error("Cannot open file for writing");

              std::ostream_iterator<char, char> oi(out);

              std::regex reg(
                  "(xmlns:[A-Za-z]+=\"urn:mtconnect.org:MTConnect[^:]+:)"
                  "[[:digit:]]+\\.[[:digit:]]+(\")");
              std::regex_replace(
                  oi, sv.begin(), sv.end(), reg, "$01" + m_schemaVersion + "$2",
                  std::regex_constants::match_default | std::regex_constants::match_any);
            }
            catch (std::runtime_error ec)
            {
              LOG(error) << "Cannot update sylesheet: " << ec.what() << " (" << fc->m_path << ')';
            }
            catch (...)
            {
              LOG(error) << "Cannot update sylesheet: (" << fc->m_path << ')';
            }
          }
          else
          {
            LOG(warning) << "Cannot find path for style file: " << *location;
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
            boost::system::error_code ec;
            auto addr = ip::make_address(host, ec);
            if (ec)
            {
              ip::tcp::resolver resolver(m_context);
              auto results = resolver.resolve(host, "0", ip::resolver_base::flags::v4_mapped, ec);
              if (ec)
              {
                cout << "Failed to resolve " << host << ": " << ec.message() << endl;
              }
              else
              {
                for (const auto &res : results)
                {
                  const auto &a = res.endpoint().address();
                  if (!a.is_multicast() && !a.is_unspecified())
                  {
                    m_server->allowPutFrom(a.to_string());
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

    static inline void respond(rest_sink::SessionPtr session, rest_sink::ResponsePtr &&response,
                               std::optional<std::string> id = std::nullopt)
    {
      response->m_requestId = id;
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
        request->m_request = "MTConnectDevices";

        auto device = request->parameter<string>("device");
        auto pretty = *request->parameter<bool>("pretty");
        auto deviceType = request->parameter<string>("deviceType");
        auto format = request->parameter<string>("format");
        auto printer = getPrinter(request->m_accepts, format);

        if (device && !ends_with(request->m_path, string("probe")) &&
            m_sinkContract->findDeviceByUUIDorName(*device) == nullptr)
          return false;

        if (deviceType && *deviceType != "Device" && *deviceType != "Agent")
        {
          return false;
        }

        respond(session, probeRequest(printer, device, pretty, deviceType, request->m_requestId),
                request->m_requestId);
        return true;
      };

      m_server
          ->addRouting({boost::beast::http::verb::get,
                        "/probe?pretty={bool:false}&deviceType={string}&format={string}", handler})
          .document("MTConnect probe request",
                    "Provides metadata service for the MTConnect Devices information model for all "
                    "devices.");
      m_server
          ->addRouting({boost::beast::http::verb::get,
                        "/{device}/probe?pretty={bool:false}&deviceType={string}&format={string}",
                        handler})
          .document("MTConnect probe request",
                    "Provides metadata service for the MTConnect Devices information model for "
                    "device identified by `device` matching `name` or `uuid`.")
          .command("probe");

      // Must be last
      m_server
          ->addRouting({boost::beast::http::verb::get,
                        "/?pretty={bool:false}&deviceType={string}&format={string}", handler})
          .document("MTConnect probe request",
                    "Provides metadata service for the MTConnect Devices information model for all "
                    "devices.");
      m_server
          ->addRouting({boost::beast::http::verb::get,
                        "/{device}?pretty={bool:false}&deviceType={string}&format={string}",
                        handler})
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
        auto pretty = request->parameter<bool>("pretty").value_or(false);
        auto format = request->parameter<string>("format");
        auto printer = getPrinter(request->m_accepts, format);

        request->m_request = "MTConnectAssets";

        respond(session,
                assetRequest(printer,  count, removed, request->parameter<string>("type"),
                          request->parameter<string>("device"),  pretty, request->m_requestId),
                request->m_requestId);
        return true;
      };

      auto idHandler = [&](SessionPtr session, RequestPtr request) -> bool {
        auto asset = request->parameter<string>("assetIds");
        request->m_request = "MTConnectAssets";

        if (asset)
        {
          auto format = request->parameter<string>("format");
          auto pretty = request->parameter<bool>("pretty").value_or(false);
          auto printer = getPrinter(request->m_accepts, format);

          list<string> ids;
          stringstream str(*asset);
          string id;
          while (getline(str, id, ';'))
            ids.emplace_back(id);

          respond(session, assetIdsRequest(printer, ids, pretty, request->m_requestId),
                  request->m_requestId);
        }
        else
        {
          auto error = Error::make(Error::ErrorCode::INVALID_REQUEST, "No asset given");
          throw RestError(error, request->m_accepts, rest_sink::status::bad_request);
        }
        return true;
      };

      string qp(
          "type={string}&removed={bool:false}&"
          "count={integer:100}&deviceType={string}&pretty={bool:false}&format={string}");

          m_server->addRouting({boost::beast::http::verb::get, "/asset?" + qp, handler})
            .document("MTConnect assets request", "Returns up to `count` assets");
        m_server->addRouting({boost::beast::http::verb::get, "/{device}/asset?" + qp, handler})
            .document("MTConnect assets request", "Returns up to `count` assets for deivce `device`")
            .command("asset");

        m_server->addRouting({boost::beast::http::verb::get, "/assets?" + qp, handler})
            .document("MTConnect assets request", "Returns up to `count` assets");
        m_server->addRouting({boost::beast::http::verb::get, "/{device}/assets?" + qp, handler})
            .document("MTConnect assets request", "Returns up to `count` assets for deivce `device`")
            .command("assets");

      m_server->addRouting({boost::beast::http::verb::get, "/assets/{assetIds}", idHandler})
          .document(
              "MTConnect assets request",
              "Returns a set assets identified by asset ids `asset` separated by semi-colon (;)");
      m_server->addRouting({boost::beast::http::verb::get,  "/asset/{assetIds}", idHandler})
          .document("MTConnect asset request",
                    "Returns a set of assets identified by asset ids `asset` separated by "
                    "semi-colon (;)")
          .command("assetsById");

      if (m_server->arePutsAllowed())
      {
        auto putHandler = [&](SessionPtr session, RequestPtr request) -> bool {
          auto format = request->parameter<string>("format");
          auto printer = getPrinter(request->m_accepts, format);
          respond(session,
                  putAssetRequest(printer, request->m_body, request->parameter<string>("type"),
                                  request->parameter<string>("device"),
                                  request->parameter<string>("assetId")),
                  request->m_requestId);
          return true;
        };

        auto deleteHandler = [&](SessionPtr session, RequestPtr request) -> bool {
          auto asset = request->parameter<string>("assetId");
          if (asset)
          {
            list<string> ids;
            stringstream str(*asset);
            string id;
            auto format = request->parameter<string>("format");
            auto printer = getPrinter(request->m_accepts, format);

            while (getline(str, id, ';'))
              ids.emplace_back(id);
            respond(session, deleteAssetRequest(printer, ids), request->m_requestId);
          }
          else
          {
            auto format = request->parameter<string>("format");
            auto printer = getPrinter(request->m_accepts, format);

            respond(session,
                    deleteAllAssetsRequest(printer, request->parameter<string>("device"),
                                           request->parameter<string>("type")),
                    request->m_requestId);
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
                    {t, "/" + asset + "/{assetId}?device={string}&type={string}&format={string}",
                     putHandler})
                .document("Upload an asset by identified by `assetId`",
                          "Updates or adds an asset with the asset XML in the body");
            m_server
                ->addRouting(
                    {t, "/" + asset + "?device={string}&type={string}&format={string}", putHandler})
                .document("Upload an asset by identified by `assetId`",
                          "Updates or adds an asset with the asset XML in the body");
            m_server
                ->addRouting({t, "/{device}/" + asset + "/{assetId}?type={string}&format={string}",
                              putHandler})
                .document("Upload an asset by identified by `assetId`",
                          "Updates or adds an asset with the asset XML in the body");
            m_server
                ->addRouting(
                    {t, "/{device}/" + asset + "?type={string}&format={string}", putHandler})
                .document("Upload an asset by identified by `assetId`",
                          "Updates or adds an asset with the asset XML in the body");
          }

          m_server
              ->addRouting({boost::beast::http::verb::delete_,
                            "/" + asset + "?device={string}&type={string}&format={string}",
                            deleteHandler})
              .document("Delete all assets for a device and type",
                        "Device and type are optional. If they are not given, it assumes there is "
                        "no constraint");
          m_server
              ->addRouting({boost::beast::http::verb::delete_,
                            "/" + asset + "/{assetId}?format={string}", deleteHandler})
              .document("Delete asset identified by `assetId`",
                        "Marks the asset as removed and creates an AssetRemoved event");
          m_server
              ->addRouting({boost::beast::http::verb::delete_,
                            "/{device}/" + asset + "?type={string}&format={string}", deleteHandler})
              .document("Delete all assets for a device and type",
                        "Device and type are optional. If they are not given, it assumes there is "
                        "no constraint")
              .command("asset");
          ;
        }
      }
    }

    void RestService::createCurrentRoutings()
    {
      using namespace rest_sink;
      auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
        request->m_request = "MTConnectStreams";

        auto interval = request->parameter<int32_t>("interval");
        if (interval)
        {
          auto format = request->parameter<string>("format");
          auto printer = getPrinter(request->m_accepts, format);

          streamCurrentRequest(session, printer, *interval, request->parameter<string>("device"),
                               request->parameter<string>("path"),
                               *request->parameter<bool>("pretty"),
                               request->parameter<string>("deviceType"), request->m_requestId);
        }
        else
        {
          auto format = request->parameter<string>("format");
          auto printer = getPrinter(request->m_accepts, format);

          respond(
              session,
              currentRequest(printer, request->parameter<string>("device"),
                             request->parameter<uint64_t>("at"), request->parameter<string>("path"),
                             *request->parameter<bool>("pretty"),
                             request->parameter<string>("deviceType"), request->m_requestId),
              request->m_requestId);
        }
        return true;
      };

      string qp(
          "path={string}&at={unsigned_integer}&"
          "interval={integer}&pretty={bool:false}&"
          "deviceType={string}&format={string}");
      m_server->addRouting({boost::beast::http::verb::get, "/current?" + qp, handler})
          .document("MTConnect current request",
                    "Gets a stapshot of the state of all the observations for all devices "
                    "optionally filtered by the `path`");
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/current?" + qp, handler})
          .document("MTConnect current request",
                    "Gets a stapshot of the state of all the observations for device `device` "
                    "optionally filtered by the `path`")
          .command("current");
    }

    void RestService::createSampleRoutings()
    {
      using namespace rest_sink;
      auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
        request->m_request = "MTConnectStreams";

        auto interval = request->parameter<int32_t>("interval");
        if (interval)
        {
          auto format = request->parameter<string>("format");
          auto printer = getPrinter(request->m_accepts, format);

          streamSampleRequest(
              session, printer, *interval, *request->parameter<int32_t>("heartbeat"),
              *request->parameter<int32_t>("count"), request->parameter<string>("device"),
              request->parameter<uint64_t>("from"), request->parameter<string>("path"),
              *request->parameter<bool>("pretty"), request->parameter<string>("deviceType"),
              request->m_requestId);
        }
        else
        {
          auto format = request->parameter<string>("format");
          auto printer = getPrinter(request->m_accepts, format);

          respond(session,
                  sampleRequest(
                      printer, *request->parameter<int32_t>("count"),
                      request->parameter<string>("device"), request->parameter<uint64_t>("from"),
                      request->parameter<uint64_t>("to"), request->parameter<string>("path"),
                      *request->parameter<bool>("pretty"), request->parameter<string>("deviceType"),
                      request->m_requestId),
                  request->m_requestId);
        }
        return true;
      };

      auto cancelHandler = [&](SessionPtr session, RequestPtr request) -> bool {
        if (request->m_requestId)
        {
          auto requestId = *request->m_requestId;
          auto success = session->cancelRequest(requestId);
          auto response = make_unique<Response>(
              status::ok, "{ \"success\": \""s + (success ? "true" : "false") + "\"}",
              "application/json");

          respond(session, std::move(response), request->m_requestId);
          return true;
        }
        else
        {
          return false;
        }
      };

      string qp(
          "path={string}&from={unsigned_integer}&"
          //"interval={integer}&count={integer:100}&"
          "count={integer:100}&"
          "heartbeat={integer:10000}&to={unsigned_integer}&"
          "pretty={bool:false}&"
          "deviceType={string}&format={string}");
      m_server->addRouting({boost::beast::http::verb::get, "/sample?" + qp, handler})
          .document("MTConnect sample request",
                    "Gets a time series of at maximum `count` observations for all devices "
                    "optionally filtered by the `path` and starting at `from`. By default, from is "
                    "the first available observation known to the agent");
      m_server->addRouting({boost::beast::http::verb::get, "/{device}/sample?" + qp, handler})
          .document("MTConnect sample request",
                    "Gets a time series of at maximum `count` observations for device `device` "
                    "optionally filtered by the `path` and starting at `from`. By default, from is "
                    "the first available observation known to the agent")
          .command("sample");
      m_server->addRouting({boost::beast::http::verb::get, "/cancel/id={string}", cancelHandler})
          .document("MTConnect WebServices Cancel Stream", "Cancels a streaming sample request")
          .command("cancel");
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
            auto format = request->parameter<string>("format");
            auto printer = getPrinter(request->m_accepts, format);

            respond(session, putObservationRequest(printer, *device, queries, ts),
                    request->m_requestId);
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

    bool RestService::publish(ObservationPtr &observation) { return true; }

    // -------------------------------------------
    // ReST API Requests
    // -------------------------------------------

    ResponsePtr RestService::probeRequest(const Printer *printer,
                                          const std::optional<std::string> &device, bool pretty,
                                          const std::optional<std::string> &deviceType,
                                          const std::optional<std::string> &requestId)
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
        if (deviceType)
        {
          deviceList.remove_if(
              [&deviceType](const DevicePtr &dev) { return dev->getName() != *deviceType; });
        }
      }

      auto counts = m_sinkContract->getAssetStorage()->getCountsByType();

      return make_unique<Response>(
          rest_sink::status::ok,
          printer->printProbe(m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
                              m_sinkContract->getCircularBuffer().getSequence(),
                              uint32_t(m_sinkContract->getAssetStorage()->getMaxAssets()),
                              uint32_t(m_sinkContract->getAssetStorage()->getCount()), deviceList,
                              &counts, false, pretty, requestId),
          printer->mimeType());
    }

    ResponsePtr RestService::currentRequest(const Printer *printer,
                                            const std::optional<std::string> &device,
                                            const std::optional<SequenceNumber_t> &at,
                                            const std::optional<std::string> &path, bool pretty,
                                            const std::optional<std::string> &deviceType,
                                            const std::optional<std::string> &requestId)
    {
      using namespace rest_sink;
      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }
      FilterSetOpt filter;
      if (path || device || deviceType)
      {
        filter = make_optional<FilterSet>();
        checkPath(printer, path, dev, *filter, deviceType);
      }

      // Check if there is a frequency to stream data or not
      return make_unique<Response>(rest_sink::status::ok,
                                   fetchCurrentData(printer, filter, at, pretty, requestId),
                                   printer->mimeType());
    }

    ResponsePtr RestService::sampleRequest(const Printer *printer, const int count,
                                           const std::optional<std::string> &device,
                                           const std::optional<SequenceNumber_t> &from,
                                           const std::optional<SequenceNumber_t> &to,
                                           const std::optional<std::string> &path, bool pretty,
                                           const std::optional<std::string> &deviceType,
                                           const std::optional<std::string> &requestId)
    {
      using namespace rest_sink;
      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }
      FilterSetOpt filter;
      if (path || device || deviceType)
      {
        filter = make_optional<FilterSet>();
        checkPath(printer, path, dev, *filter, deviceType);
      }

      // Check if there is a frequency to stream data or not
      SequenceNumber_t end;
      bool endOfBuffer;

      return make_unique<Response>(
          rest_sink::status::ok,
          fetchSampleData(printer, filter, count, from, to, end, endOfBuffer, pretty, requestId),
          printer->mimeType());
    }

    struct AsyncSampleResponse : public observation::AsyncObserver
    {
      AsyncSampleResponse(boost::asio::io_context::strand &strand,
                          mtconnect::buffer::CircularBuffer &buffer, FilterSet &&filter,
                          std::chrono::milliseconds interval, std::chrono::milliseconds heartbeat,
                          rest_sink::SessionPtr &session)
        : observation::AsyncObserver(strand, buffer, std::move(filter), interval, heartbeat),
          m_session(session)
      {}

      void fail(boost::beast::http::status status, const std::string &message) override
      {
        auto sink = m_sink.lock();
        if (sink && isRunning())
        {
          m_session->fail(status, message);
          cancel();
        }
        else
        {
          LOG(debug) << "Sink close when failing sample respone: " << message;
        }
      }

      bool isRunning() override
      {
        auto sink = m_sink.lock();
        Server *server {nullptr};
        if (sink)
        {
          server = dynamic_pointer_cast<RestService>(sink)->getServer();
        }

        if (!sink || !server || !server->isRunning())
        {
          return false;
        }
        else
        {
          return server->isRunning();
        }
      }

      bool cancel() override
      {
        observation::AsyncObserver::cancel();
        m_session.reset();
        return true;
      }

      std::weak_ptr<sink::Sink>
          m_sink;  //!  weak shared pointer to the sink. handles shutdown timer race
      int m_count {0};
      const Printer *m_printer {nullptr};
      bool m_logStreamData {false};
      rest_sink::SessionPtr m_session;
      ofstream m_log;
      bool m_pretty {false};
    };

    void RestService::streamSampleRequest(rest_sink::SessionPtr session, const Printer *printer,
                                          const int interval, const int heartbeatIn,
                                          const int count, const std::optional<std::string> &device,
                                          const std::optional<SequenceNumber_t> &from,
                                          const std::optional<std::string> &path, bool pretty,
                                          const std::optional<std::string> &deviceType,
                                          const std::optional<std::string> &requestId)
    {
      NAMED_SCOPE("RestService::streamSampleRequest");

      using namespace rest_sink;
      using std::placeholders::_1;
      using std::placeholders::_2;

      checkRange(printer, interval, -1, numeric_limits<int>().max(), "interval");
      checkRange(printer, heartbeatIn, 1, numeric_limits<int>().max(), "heartbeat");
      if (from)
      {
        std::lock_guard<CircularBuffer> lock(m_sinkContract->getCircularBuffer());
        auto firstSeq = m_sinkContract->getCircularBuffer().getFirstSequence();
        auto seq = m_sinkContract->getCircularBuffer().getSequence();
        checkRange(printer, *from, firstSeq - 1, seq + 1, "from");
      }

      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }

      FilterSet filter;
      checkPath(printer, path, dev, filter, deviceType);

      auto asyncResponse = make_shared<AsyncSampleResponse>(
          m_strand, m_sinkContract->getCircularBuffer(), std::move(filter),
          std::chrono::milliseconds(interval), std::chrono::milliseconds(heartbeatIn), session);
      asyncResponse->m_count = count;
      asyncResponse->m_printer = printer;
      asyncResponse->m_sink = getptr();
      asyncResponse->m_pretty = pretty;
      asyncResponse->setRequestId(requestId);
      session->addObserver(asyncResponse);

      if (m_logStreamData)
      {
        stringstream filename;
        filename << "Stream_" << getCurrentTime(LOCAL) << "_" << this_thread::get_id() << ".log";
        asyncResponse->m_log.open(filename.str().c_str());
      }

      asyncResponse->m_logStreamData = m_logStreamData;
      asyncResponse->observe(from, [this](const std::string &id) {
        return m_sinkContract->getDataItemById(id).get();
      });
      asyncResponse->m_handler = boost::bind(&RestService::streamNextSampleChunk, this, _1);

      session->beginStreaming(
          printer->mimeType(),
          asio::bind_executor(m_strand,
                              boost::bind(&AsyncObserver::handlerCompleted, asyncResponse)),
          requestId);
    }

    SequenceNumber_t RestService::streamNextSampleChunk(
        shared_ptr<observation::AsyncObserver> asyncObserver)
    {
      NAMED_SCOPE("RestService::streamNextSampleChunk");

      auto asyncResponse = std::dynamic_pointer_cast<AsyncSampleResponse>(asyncObserver);

      try
      {
        SequenceNumber_t end {0ull};

        // end and endOfBuffer are set during the fetch sample data while the
        // mutex is held. This removed the race to check if we are at the end of
        // the bufffer and setting the next start to the last sequence number
        // sent.
        std::optional<SequenceNumber_t> from;
        if (asyncResponse->getSequence() > 0)
          from.emplace(asyncResponse->getSequence());

        string content = fetchSampleData(asyncResponse->m_printer, asyncResponse->getFilter(),
                                         asyncResponse->m_count, from, nullopt, end,
                                         asyncObserver->m_endOfBuffer, asyncResponse->m_pretty,
                                         asyncResponse->getRequestId());

        if (m_logStreamData)
          asyncResponse->m_log << content << endl;

        if (asyncResponse->m_session)
        {
          asyncResponse->m_session->writeChunk(
              content,
              asio::bind_executor(m_strand,
                                  boost::bind(&AsyncObserver::handlerCompleted, asyncResponse)),
              asyncResponse->getRequestId());
        }
        return end;
      }

      catch (RestError &re)
      {
        LOG(error) << asyncResponse->m_session->getRemote().address()
                   << ": Error processing request: " << re.what();
        if (asyncResponse->m_session)
        {
          if (asyncResponse->getRequestId())
            re.setRequestId(*asyncResponse->getRequestId());
          writeErrorResponse(asyncResponse->m_session, re);
          asyncResponse->m_session->close();
        }
      }

      catch (...)
      {
        std::stringstream txt;
        txt << asyncResponse->m_session->getRemote().address() << ": Unknown Error thrown";
        LOG(error) << txt.str();
        asyncResponse->fail(boost::beast::http::status::not_found, txt.str());
      }

      return 0;
    }

    struct AsyncCurrentResponse : public AsyncResponse
    {
      AsyncCurrentResponse(rest_sink::SessionPtr session, asio::io_context &context,
                           chrono::milliseconds interval)
        : AsyncResponse(interval), m_session(session), m_timer(context)
      {}

      auto getptr() { return dynamic_pointer_cast<AsyncCurrentResponse>(shared_from_this()); }

      bool cancel() override
      {
        m_timer.cancel();
        m_session.reset();
        return true;
      }

      bool isRunning() override { return (bool)m_session; }

      std::weak_ptr<Sink> m_service;
      rest_sink::SessionPtr m_session;
      const Printer *m_printer {nullptr};
      FilterSetOpt m_filter;
      boost::asio::steady_timer m_timer;
      bool m_pretty {false};
    };

    void RestService::streamCurrentRequest(SessionPtr session, const Printer *printer,
                                           const int interval,
                                           const std::optional<std::string> &device,
                                           const std::optional<std::string> &path, bool pretty,
                                           const std::optional<std::string> &deviceType,
                                           const std::optional<std::string> &requestId)
    {
      checkRange(printer, interval, 0, numeric_limits<int>().max(), "interval");
      DevicePtr dev {nullptr};
      if (device)
      {
        dev = checkDevice(printer, *device);
      }

      auto asyncResponse =
          make_shared<AsyncCurrentResponse>(session, m_context, chrono::milliseconds {interval});
      if (path || device || deviceType)
      {
        asyncResponse->m_filter = make_optional<FilterSet>();
        checkPath(printer, path, dev, *asyncResponse->m_filter, deviceType);
      }
      asyncResponse->m_printer = printer;
      asyncResponse->m_service = getptr();
      asyncResponse->m_pretty = pretty;
      asyncResponse->setRequestId(requestId);
      session->addObserver(asyncResponse);

      asyncResponse->m_session->beginStreaming(
          printer->mimeType(),
          boost::asio::bind_executor(m_strand,
                                     [this, asyncResponse]() {
                                       streamNextCurrent(asyncResponse,
                                                         boost::system::error_code {});
                                     }),
          requestId);
    }

    void RestService::streamNextCurrent(std::shared_ptr<AsyncCurrentResponse> asyncResponse,
                                        boost::system::error_code ec)
    {
      using std::placeholders::_1;

      try
      {
        auto service = asyncResponse->m_service.lock();

        if (!service || !m_server || !m_server->isRunning())
        {
          LOG(warning) << "Trying to send chunk when service has stopped";
          if (service && asyncResponse->m_session)
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
          if (asyncResponse->m_session)
            asyncResponse->m_session->fail(boost::beast::http::status::internal_server_error,
                                           "Unexpected error streamNextCurrent, aborting");
          return;
        }

        if (asyncResponse->m_session)
        {
          asyncResponse->m_session->writeChunk(
              fetchCurrentData(asyncResponse->m_printer, asyncResponse->m_filter, nullopt,
                               asyncResponse->m_pretty, asyncResponse->getRequestId()),
              boost::asio::bind_executor(
                  m_strand,
                  [this, asyncResponse]() {
                    asyncResponse->m_timer.expires_after(asyncResponse->getInterval());
                    asyncResponse->m_timer.async_wait(boost::asio::bind_executor(
                        m_strand,
                        boost::bind(&RestService::streamNextCurrent, this, asyncResponse, _1)));
                  }),
              asyncResponse->getRequestId());
        }
      }
      catch (RestError &re)
      {
        LOG(error) << asyncResponse->m_session->getRemote().address()
                   << ": Error processing request: " << re.what();
        if (asyncResponse->m_session)
        {
          if (asyncResponse->getRequestId())
            re.setRequestId(*asyncResponse->getRequestId());
          writeErrorResponse(asyncResponse->m_session, re);
          asyncResponse->m_session->close();
        }
      }

      catch (...)
      {
        std::stringstream txt;
        txt << asyncResponse->m_session->getRemote().address() << ": Unknown Error thrown";
        LOG(error) << txt.str();
        if (asyncResponse->m_session)
        {
          asyncResponse->m_session->fail(boost::beast::http::status::not_found, txt.str());
        }
      }
    }

    ResponsePtr RestService::assetRequest(const Printer *printer, const int32_t count,
                                          const bool removed,
                                          const std::optional<std::string> &type,
                                          const std::optional<std::string> &device, bool pretty,
                                          const std::optional<std::string> &requestId)
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
              uint32_t(m_sinkContract->getAssetStorage()->getCount()), list, pretty, requestId),
          printer->mimeType());
    }

    ResponsePtr RestService::assetIdsRequest(const Printer *printer,
                                             const std::list<std::string> &ids, bool pretty,
                                             const std::optional<std::string> &requestId)
    {
      using namespace rest_sink;

      AssetList list;
      if (m_sinkContract->getAssetStorage()->getAssets(list, ids) == 0)
      {
        entity::EntityList errors;
        for (auto &id : ids)
          errors.emplace_back(AssetNotFound::make(id, "Cannot find asset: " + id));
        throw RestError(errors, printer, status::not_found, std::nullopt, requestId);
      }
      else
      {
        return make_unique<Response>(
            status::ok,
            printer->printAssets(
                m_instanceId, uint32_t(m_sinkContract->getAssetStorage()->getMaxAssets()),
                uint32_t(m_sinkContract->getAssetStorage()->getCount()), list, pretty, requestId),
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
        entity::EntityList errorList;

        if (!ap)
          errorList.emplace_back(
              Error::make(Error::ErrorCode::INVALID_REQUEST, "Could not parse Asset."));
        else
          errorList.emplace_back(
              Error::make(Error::ErrorCode::INVALID_REQUEST, "Asset parsed with errors."));
        for (auto &e : errors)
        {
          errorList.emplace_back(Error::make(Error::ErrorCode::INVALID_REQUEST, e->what()));
        }

        throw RestError(errorList, printer);
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
        entity::EntityList errors;
        for (auto &id : ids)
          errors.emplace_back(AssetNotFound::make(id, "Cannot find asset: " + id));
        throw RestError(errors, printer, status::not_found);
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
        throw RestError(AssetNotFound::make("", "No assets to delete"), printer, status::not_found);
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

      entity::EntityList errors;
      for (auto &qp : observations)
      {
        auto di = dev->getDeviceDataItem(qp.first);
        if (di == nullptr)
        {
          errors.emplace_back(
              Error::make(Error::ErrorCode::INVALID_REQUEST, "Cannot find data item: " + qp.first));
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

      if (errors.empty())
      {
        return make_unique<Response>(status::ok, "<success/>", "text/xml");
      }
      else
      {
        throw RestError(errors, printer);
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

    // -----------------------------------------------
    // Validation methods
    // -----------------------------------------------

    template <typename T>
    void RestService::checkRange(const Printer *printer, const T value, const T min, const T max,
                                 const string &param, bool notZero) const
    {
      stringstream str;
      if (value <= min)
      {
        str << '\'' << param << '\'' << " must be greater than " << min;
      }
      else if (value >= max)
      {
        str << '\'' << param << '\'' << " must be less than " << max;
      }
      else if (notZero && value == 0)
      {
        str << '\'' << param << '\'' << " must not be zero(0)";
      }
      if (str.tellp() > 0)
      {
        auto error = OutOfRange::make(param, value, min + 1, max - 1, str.str());
        throw RestError(error, printer);
      }
    }

    void RestService::checkPath(const Printer *printer, const std::optional<std::string> &path,
                                const DevicePtr device, FilterSet &filter,
                                const std::optional<std::string> &deviceType) const
    {
      try
      {
        m_sinkContract->getDataItemsForPath(device, path, filter, deviceType);
      }
      catch (exception &e)
      {
        string msg = "The path could not be parsed. Invalid syntax: "s + e.what();
        auto error = Error::make(Error::ErrorCode::INVALID_XPATH, msg);
        throw RestError(error, printer);
      }

      if (filter.empty())
      {
        string msg = "The path could not be parsed. Invalid syntax: " + *path;
        auto error = Error::make(Error::ErrorCode::INVALID_XPATH, msg);
        throw RestError(error, printer);
      }
    }

    DevicePtr RestService::checkDevice(const Printer *printer, const std::string &uuid) const
    {
      auto dev = m_sinkContract->findDeviceByUUIDorName(uuid);
      if (!dev)
      {
        string msg("Could not find the device '" + uuid + "'");
        auto error = Error::make(Error::ErrorCode::NO_DEVICE, msg);
        throw RestError(error, printer, status::not_found);
      }

      return dev;
    }

    // -------------------------------------------
    // Data Collection and Formatting
    // -------------------------------------------

    string RestService::fetchCurrentData(const Printer *printer, const FilterSetOpt &filterSet,
                                         const optional<SequenceNumber_t> &at, bool pretty,
                                         const std::optional<std::string> &requestId)
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
                                  seq, firstSeq, seq - 1, observations, pretty, requestId);
    }

    string RestService::fetchSampleData(const Printer *printer, const FilterSetOpt &filterSet,
                                        int count, const std::optional<SequenceNumber_t> &from,
                                        const std::optional<SequenceNumber_t> &to,
                                        SequenceNumber_t &end, bool &endOfBuffer, bool pretty,
                                        const std::optional<std::string> &requestId)
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
      }

      return printer->printSample(m_instanceId, m_sinkContract->getCircularBuffer().getBufferSize(),
                                  end, firstSeq, lastSeq, *observations, pretty, requestId);
    }

  }  // namespace sink::rest_sink
}  // namespace mtconnect

