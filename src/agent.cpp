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

#include "agent.hpp"

#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>

#include "assets/asset.hpp"
#include "assets/cutting_tool.hpp"
#include "assets/file_asset.hpp"
#include "device_model/agent_device.hpp"
#include "entity/xml_parser.hpp"
#include "http_server/file_cache.hpp"
#include "http_server/session.hpp"
#include "json_printer.hpp"
#include "logging.hpp"
#include "observation/observation.hpp"
#include "xml_printer.hpp"

using namespace std;
using namespace mtconnect::observation;

namespace mtconnect
{
  using namespace device_model;
  using namespace data_item;
  using namespace entity;
  using namespace http_server;
  namespace net = boost::asio;

  static const string g_unavailable("UNAVAILABLE");
  static const string g_available("AVAILABLE");

  // Agent public methods
  Agent::Agent(boost::asio::io_context &context, std::unique_ptr<http_server::Server> &server,
               std::unique_ptr<http_server::FileCache> &cache, const string &configXmlPath,
               int bufferSize, int maxAssets, const std::string &version, int checkpointFreq,
               bool pretty)
    : m_context(context),
      m_strand(m_context),
      m_server(move(server)),
      m_fileCache(move(cache)),
      m_xmlParser(make_unique<mtconnect::XmlParser>()),
      m_circularBuffer(bufferSize, checkpointFreq),
      m_assetBuffer(maxAssets),
      m_version(version),
      m_configXmlPath(configXmlPath),
      m_logStreamData(false),
      m_pretty(pretty)
  {
    CuttingToolArchetype::registerAsset();
    CuttingTool::registerAsset();
    FileArchetypeAsset::registerAsset();
    FileAsset::registerAsset();

    // Create the Printers
    m_printers["xml"] = unique_ptr<Printer>(new XmlPrinter(version, m_pretty));
    m_printers["json"] = unique_ptr<Printer>(new JsonPrinter(version, m_pretty));

    // Unique id number for agent instance
    m_instanceId = getCurrentTimeInSec();
  }

  void Agent::initialize(pipeline::PipelineContextPtr context, const ConfigOptions &options)
  {
    NAMED_SCOPE("agent");
    m_loopback = std::make_unique<AgentLoopbackPipeline>(context);
    m_loopback->build(options);

    int major, minor;
    char c;
    stringstream vstr(m_version);
    vstr >> major >> c >> minor;
    if (major > 1 || (major == 1 && minor >= 7))
    {
      createAgentDevice();
    }
    loadXMLDeviceFile(m_configXmlPath);
    loadCachedProbe();

    createProbeRoutings();
    createCurrentRoutings();
    createSampleRoutings();
    createAssetRoutings();
    createPutObservationRoutings();
    createFileRoutings();

    m_server->setErrorFunction(
        [this](SessionPtr session, http_server::status st, const string &msg) {
          auto printer = getPrinter("xml");
          auto doc = printError(printer, "INVALID_REQUEST", msg);
          session->writeResponse({st, doc, printer->mimeType()});
        });

    m_initialized = true;
  }

  Agent::~Agent()
  {
    for (auto adp : m_adapters)
      delete adp;
    m_xmlParser.reset();
    m_agentDevice = nullptr;
  }

  void Agent::start()
  {
    NAMED_SCOPE("Agent::start");
    try
    {
      // Start all the adapters
      for (const auto adapter : m_adapters)
        adapter->start();

      // Start the server. This blocks until the server stops.
      m_server->start();
    }
    catch (std::runtime_error &e)
    {
      LOG(fatal) << "Cannot start server: " << e.what();
      std::exit(1);
    }
  }

  void Agent::stop()
  {
    NAMED_SCOPE("Agent::stop");

    // Stop all adapter threads...
    LOG(info) << "Shutting down adapters";
    // Deletes adapter and waits for it to exit.
    for (const auto adapter : m_adapters)
      adapter->stop();

    LOG(info) << "Shutting down server";
    m_server->stop();

    LOG(info) << "Shutting down adapters";
    for (const auto adapter : m_adapters)
      delete adapter;

    m_adapters.clear();
    LOG(info) << "Shutting down completed";
  }

  // ---------------------------------------
  // Agent Device
  // ---------------------------------------

  void Agent::createAgentDevice()
  {
    NAMED_SCOPE("Agent::createAgentDevice");

    // Create the Agent Device
    ErrorList errors;
    Properties ps {{"uuid", "0b49a3a0-18ca-0139-8748-2cde48001122"s},
                   {"id", "agent_2cde48001122"s},
                   {"name", "Agent"s},
                   {"mtconnectVersion", "1.7"s}};
    m_agentDevice =
        dynamic_pointer_cast<AgentDevice>(AgentDevice::getFactory()->make("Agent", ps, errors));
    if (!errors.empty())
    {
      for (auto &e : errors)
        LOG(fatal) << "Error creating the agent device: " << e->what();
      throw EntityError("Cannot create AgentDevice");
    }
    addDevice(m_agentDevice);
  }

  // ----------------------------------------------
  // Device management and Initialization
  // ----------------------------------------------

  void Agent::loadXMLDeviceFile(const std::string &configXmlPath)
  {
    NAMED_SCOPE("Agent::loadXMLDeviceFile");

    try
    {
      // Load the configuration for the Agent
      auto devices = m_xmlParser->parseFile(configXmlPath,
                                            dynamic_cast<XmlPrinter *>(m_printers["xml"].get()));

      // Fir the DeviceAdded event for each device
      for (auto device : devices)
        addDevice(device);
    }
    catch (runtime_error &e)
    {
      LOG(fatal) << "Error loading xml configuration: " + configXmlPath;
      LOG(fatal) << "Error detail: " << e.what();
      cerr << e.what() << endl;
      throw e;
    }
    catch (exception &f)
    {
      LOG(fatal) << "Error loading xml configuration: " + configXmlPath;
      LOG(fatal) << "Error detail: " << f.what();
      cerr << f.what() << endl;
      throw f;
    }
  }

  void Agent::verifyDevice(DevicePtr device)
  {
    NAMED_SCOPE("Agent::verifyDevice");

    auto xmlPrinter = dynamic_cast<XmlPrinter *>(m_printers["xml"].get());
    const auto &schemaVersion = xmlPrinter->getSchemaVersion();

    // Add the devices to the device map and create availability and
    // asset changed events if they don't exist
    // Make sure we have two device level data items:
    // 1. Availability
    // 2. AssetChanged
    if (!device->getAvailability())
    {
      // Create availability data item and add it to the device.
      entity::ErrorList errors;
      auto di = DataItem::make(
          {{"type", "AVAILABILITY"s}, {"id", device->getId() + "_avail"}, {"category", "EVENT"s}},
          errors);
      device->addDataItem(di, errors);
    }

    int major, minor;
    char c;
    stringstream ss(schemaVersion);
    ss >> major >> c >> minor;
    if (!device->getAssetChanged() && (major > 1 || (major == 1 && minor >= 2)))
    {
      entity::ErrorList errors;
      // Create asset change data item and add it to the device.
      auto di = DataItem::make({{"type", "ASSET_CHANGED"s},
                                {"id", device->getId() + "_asset_chg"},
                                {"category", "EVENT"s}},
                               errors);
      device->addDataItem(di, errors);
    }

    if (device->getAssetChanged() && (major > 1 || (major == 1 && minor >= 5)))
    {
      auto di = device->getAssetChanged();
      di->makeDiscrete();
    }

    if (!device->getAssetRemoved() && (major > 1 || (major == 1 && minor >= 3)))
    {
      // Create asset removed data item and add it to the device.
      entity::ErrorList errors;
      auto di = DataItem::make({{"type", "ASSET_REMOVED"s},
                                {"id", device->getId() + "_asset_rem"},
                                {"category", "EVENT"s}},
                               errors);
      device->addDataItem(di, errors);
    }
  }

  void Agent::initializeDataItems(DevicePtr device)
  {
    NAMED_SCOPE("Agent::initializeDataItems");

    // Grab data from configuration
    string time = getCurrentTime(GMT_UV_SEC);

    // Initialize the id mapping for the devices and set all data items to UNAVAILABLE
    for (auto item : device->getDeviceDataItems())
    {
      auto d = item.second.lock();
      if (m_dataItemMap.count(d->getId()) > 0)
      {
        if (m_dataItemMap[d->getId()] != d)
        {
          LOG(fatal) << "Duplicate DataItem id " << d->getId()
                     << " for device: " << *device->getComponentName()
                     << " and data item name: " << d->getId();
          std::exit(1);
        }
      }
      else
      {
        // Check for single valued constrained data items.
        const string *value = &g_unavailable;
        if (d->isCondition())
          value = &g_unavailable;
        else if (d->getConstantValue())
          value = &d->getConstantValue().value();

        addToBuffer(d, *value);
        m_dataItemMap[d->getId()] = d;
      }
    }
  }

  // Add the a device from a configuration file
  void Agent::addDevice(DevicePtr device)
  {
    NAMED_SCOPE("Agent::addDevice");

    // Check if device already exists
    string uuid = *device->getUuid();
    auto old = m_deviceUuidMap.find(uuid);
    if (old != m_deviceUuidMap.end())
    {
      // Update existing device
      LOG(fatal) << "Device " << *device->getUuid() << " already exists. "
                 << " Update not supported yet";
      std::exit(1);
    }
    else
    {
      // Check if we are already initialized. If so, the device will need to be
      // verified, additional data items added, and initial values set.
      if (!m_initialized)
      {
        m_devices.push_back(device);
        m_deviceNameMap[device->get<string>("name")] = device;
        m_deviceUuidMap[uuid] = device;

        // TODO: Redo Resolve Reference  with entity
        // device->resolveReferences();
        verifyDevice(device);
        initializeDataItems(device);

        // Check for single valued constrained data items.
        if (m_agentDevice && device != m_agentDevice)
        {
          auto d = m_agentDevice->getDeviceDataItem("device_added");
          addToBuffer(d, uuid);
        }
      }
      else
        LOG(warning) << "Adding device " << uuid << " after initialialization not supported yet";
    }
  }

  void Agent::deviceChanged(DevicePtr device, const std::string &oldUuid,
                            const std::string &oldName)
  {
    NAMED_SCOPE("Agent::deviceChanged");

    string uuid = *device->getUuid();
    if (uuid != oldUuid)
    {
      if (m_agentDevice)
      {
        auto d = m_agentDevice->getDeviceDataItem("device_removed");
        addToBuffer(d, oldUuid);
      }
      m_deviceUuidMap.erase(oldUuid);
      m_deviceUuidMap[uuid] = device;
    }

    if (*device->getComponentName() != oldName)
    {
      m_deviceNameMap.erase(oldName);
      m_deviceNameMap[device->get<string>("name")] = device;
    }

    loadCachedProbe();

    if (m_agentDevice)
    {
      if (device->getUuid() != oldUuid)
      {
        auto d = m_agentDevice->getDeviceDataItem("device_added");
        addToBuffer(d, uuid);
      }
      else
      {
        auto d = m_agentDevice->getDeviceDataItem("device_changed");
        addToBuffer(d, uuid);
      }
    }
  }

  void Agent::loadCachedProbe()
  {
    NAMED_SCOPE("Agent::loadCachedProbe");

    // Reload the document for path resolution
    auto xmlPrinter = dynamic_cast<XmlPrinter *>(m_printers["xml"].get());
    m_xmlParser->loadDocument(xmlPrinter->printProbe(m_instanceId, m_circularBuffer.getBufferSize(),
                                                     getMaxAssets(), m_assetBuffer.getCount(),
                                                     m_circularBuffer.getSequence(), m_devices));
  }

  // -----------------------------------------------------------
  // Request Routing
  // -----------------------------------------------------------

  static inline void respond(http_server::SessionPtr session, http_server::Response response)
  {
    session->writeResponse(response);
  }

  void Agent::createFileRoutings()
  {
    using namespace http_server;
    auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
      auto f = m_fileCache->getFile(request->m_path);
      if (f)
      {
        Response response(http_server::status::ok, f->m_buffer.get(), f->m_mimeType);
        session->writeResponse(response);
      }
      return bool(f);
    };
    m_server->addRouting({boost::beast::http::verb::get, regex("/.+"), handler});
  }

  void Agent::createProbeRoutings()
  {
    using namespace http_server;
    // Probe
    auto handler = [&](SessionPtr session, const RequestPtr request) -> bool {
      auto device = request->parameter<string>("device");
      auto printer = printerForAccepts(request->m_accepts);

      if (device && !ends_with(request->m_path, string("probe")) &&
          findDeviceByUUIDorName(*device) == nullptr)
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

  void Agent::createAssetRoutings()
  {
    using namespace http_server;
    auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
      auto removed = *request->parameter<string>("removed") == "true";
      auto count = *request->parameter<int32_t>("count");
      auto printer = printerForAccepts(request->m_accepts);

      respond(session, assetRequest(printer, count, removed, request->parameter<string>("type"),
                                    request->parameter<string>("device")));
      return true;
    };

    auto idHandler = [&](SessionPtr session, RequestPtr request) -> bool {
      auto assets = request->parameter<string>("assets");
      if (assets)
      {
        auto printer = getPrinter(acceptFormat(request->m_accepts));

        list<string> ids;
        stringstream str(*assets);
        string id;
        while (getline(str, id, ';'))
          ids.emplace_back(id);
        respond(session, assetIdsRequest(printer, ids));
      }
      else
      {
        auto printer = printerForAccepts(request->m_accepts);
        auto error = printError(printer, "INVALID_REQUEST", "No assets given");
        respond(session, {http_server::status::bad_request, error, printer->mimeType()});
      }
      return true;
    };

    string qp("type={string}&removed={string:false}&count={integer:100}&device={string}");
    m_server->addRouting({boost::beast::http::verb::get, "/assets?" + qp, handler});
    m_server->addRouting({boost::beast::http::verb::get, "/asset?" + qp, handler});
    m_server->addRouting({boost::beast::http::verb::get, "/{device}/assets?" + qp, handler});
    m_server->addRouting({boost::beast::http::verb::get, "/{device}/asset?" + qp, handler});

    m_server->addRouting({boost::beast::http::verb::get, "/asset/{assets}", idHandler});
    m_server->addRouting({boost::beast::http::verb::get, "/assets/{assets}", idHandler});

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
        auto assets = request->parameter<string>("assets");
        if (assets)
        {
          list<string> ids;
          stringstream str(*assets);
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
                            "/assets?&device={string}&type={string}", deleteHandler});
      m_server->addRouting({boost::beast::http::verb::delete_,
                            "/asset?&device={string}&type={string}", deleteHandler});
      m_server->addRouting({boost::beast::http::verb::delete_, "/assets/{assets}", deleteHandler});
      m_server->addRouting({boost::beast::http::verb::delete_, "/asset/{assets}", deleteHandler});
      m_server->addRouting(
          {boost::beast::http::verb::delete_, "/{device}/assets?type={string}", deleteHandler});
      m_server->addRouting(
          {boost::beast::http::verb::delete_, "/{device}/asset?type={string}", deleteHandler});
    }
  }

  void Agent::createCurrentRoutings()
  {
    using namespace http_server;
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

  void Agent::createSampleRoutings()
  {
    using namespace http_server;
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

  void Agent::createPutObservationRoutings()
  {
    using namespace http_server;

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
  // Helper Methods
  // ----------------------------------------------------

  DevicePtr Agent::getDeviceByName(const std::string &name) const
  {
    if (name.empty())
      return defaultDevice();

    auto devPos = m_deviceNameMap.find(name);
    if (devPos != m_deviceNameMap.end())
      return devPos->second;

    return nullptr;
  }

  DevicePtr Agent::getDeviceByName(const std::string &name)
  {
    if (name.empty())
      return defaultDevice();

    auto devPos = m_deviceNameMap.find(name);
    if (devPos != m_deviceNameMap.end())
      return devPos->second;

    return nullptr;
  }

  DevicePtr Agent::findDeviceByUUIDorName(const std::string &idOrName) const
  {
    if (idOrName.empty())
      return defaultDevice();

    auto di = m_deviceUuidMap.find(idOrName);
    if (di == m_deviceUuidMap.end())
    {
      di = m_deviceNameMap.find(idOrName);
      if (di != m_deviceNameMap.end())
        return di->second;
    }
    else
    {
      return di->second;
    }

    return nullptr;
  }

  const string Agent::acceptFormat(const std::string &accepts) const
  {
    std::stringstream list(accepts);
    std::string accept;
    while (std::getline(list, accept, ','))
    {
      for (const auto &p : m_printers)
      {
        if (ends_with(accept, p.first))
          return p.first;
      }
    }

    return "xml";
  }
  // ----------------------------------------------------
  // Adapter Methods
  // ----------------------------------------------------

  void Agent::addAdapter(adapter::Adapter *adapter, bool start)
  {
    m_adapters.emplace_back(adapter);

    if (start)
      adapter->start();

    if (m_agentDevice)
    {
      m_agentDevice->addAdapter(adapter);
      initializeDataItems(m_agentDevice);
      // Reload the document for path resolution
      if (m_initialized)
      {
        loadCachedProbe();
      }
    }
  }

  void AgentPipelineContract::deliverConnectStatus(entity::EntityPtr entity,
                                                   const StringList &devices, bool autoAvailable)
  {
    auto value = entity->getValue<string>();
    if (value == "CONNECTING")
    {
      m_agent->connecting(entity->get<string>("source"));
    }
    else if (value == "CONNECTED")
    {
      m_agent->connected(entity->get<string>("source"), devices, autoAvailable);
    }
    else if (value == "DISCONNECTED")
    {
      m_agent->disconnected(entity->get<string>("source"), devices, autoAvailable);
    }
    else
    {
      LOG(error) << "Unexpected connection status received: " << value;
    }
  }

  void AgentPipelineContract::deliverCommand(entity::EntityPtr entity)
  {
    static auto pattern = regex("\\*[ ]*([^:]+):[ ]*(.+)");
    auto value = entity->getValue<string>();
    smatch match;

    if (std::regex_match(value, match, pattern))
    {
      auto device = entity->maybeGet<string>("device");
      auto command = match[1].str();
      auto param = match[2].str();
      auto source = entity->maybeGet<string>("source");

      if (!device || !source)
      {
        LOG(error) << "Invalid command: " << command << ", device or source not specified";
      }
      else
      {
        LOG(debug) << "Processing command: " << command << ": " << value;
        m_agent->receiveCommand(*device, command, param, *source);
      }
    }
    else
    {
      LOG(warning) << "Cannot parse command: " << value;
    }
  }

  void Agent::connecting(const std::string &adapter)
  {
    if (m_agentDevice)
    {
      auto di = m_agentDevice->getConnectionStatus(adapter);
      addToBuffer(di, "LISTENING");
    }
  }

  // Add values for related data items UNAVAILABLE
  void Agent::disconnected(const std::string &adapter, const StringList &devices,
                           bool autoAvailable)
  {
    LOG(debug) << "Disconnected from adapter, setting all values to UNAVAILABLE";

    if (m_agentDevice)
    {
      auto di = m_agentDevice->getConnectionStatus(adapter);
      addToBuffer(di, "CLOSED");
    }

    for (auto &name : devices)
    {
      DevicePtr device = findDeviceByUUIDorName(name);
      if (device == nullptr)
      {
        LOG(warning) << "Cannot find device " << name << " when adapter " << adapter
                     << "disconnected";
        continue;
      }

      const auto &dataItems = device->getDeviceDataItems();
      for (const auto &dataItemAssoc : dataItems)
      {
        auto dataItem = dataItemAssoc.second.lock();
        if (dataItem && ((dataItem->getDataSource() && *dataItem->getDataSource() == adapter) ||
                         (autoAvailable && !dataItem->getDataSource() &&
                          dataItem->getType() == "AVAILABILITY")))
        {
          auto ptr = m_circularBuffer.getLatest().getEventPtr(dataItem->getId());

          if (ptr)
          {
            const string *value = nullptr;
            if (dataItem->getConstantValue())
              value = &dataItem->getConstantValue().value();
            else if (!ptr->isUnavailable())
              value = &g_unavailable;

            if (value)
              addToBuffer(dataItem, *value);
          }
        }
        else if (!dataItem)
          LOG(warning) << "No data Item for " << dataItemAssoc.first;
      }
    }
  }

  void Agent::connected(const std::string &adapter, const StringList &devices, bool autoAvailable)
  {
    if (m_agentDevice)
    {
      auto di = m_agentDevice->getConnectionStatus(adapter);
      addToBuffer(di, "ESTABLISHED");
    }

    if (!autoAvailable)
      return;

    for (auto &name : devices)
    {
      DevicePtr device = findDeviceByUUIDorName(name);
      if (device == nullptr)
      {
        LOG(warning) << "Cannot find device " << name << " when adapter " << adapter << "connected";
        continue;
      }
      LOG(debug) << "Connected to adapter, setting all Availability data items to AVAILABLE";

      if (auto avail = device->getAvailability())
      {
        LOG(debug) << "Adding availabilty event for " << device->getAvailability()->getId();
        addToBuffer(avail, g_available);
      }
      else
        LOG(debug) << "Cannot find availability for " << *device->getComponentName();
    }
  }

  // ----------------------------------------------------
  // Observation Add Method
  // ----------------------------------------------------

  SequenceNumber_t Agent::addToBuffer(ObservationPtr &observation)
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

  SequenceNumber_t Agent::addToBuffer(DataItemPtr dataItem, entity::Properties props,
                                      std::optional<Timestamp> timestamp)
  {
    entity::ErrorList errors;

    Timestamp ts = timestamp ? *timestamp : chrono::system_clock::now();
    auto observation = observation::Observation::make(dataItem, props, ts, errors);
    if (observation && errors.empty())
    {
      auto res = m_loopback->run(observation);
      if (auto obs = dynamic_pointer_cast<Observation>(res))
      {
        return obs->getSequence();
      }
    }
    else
    {
      LOG(error) << "Cannot add observation: ";
      for (auto &e : errors)
      {
        LOG(error) << "Cannot add observation: " << e->what();
      }
    }

    return 0;
  }

  SequenceNumber_t Agent::addToBuffer(DataItemPtr dataItem, const std::string &value,
                                      std::optional<Timestamp> timestamp)
  {
    if (dataItem->isCondition())
      return addToBuffer(dataItem, {{"level", value}}, timestamp);
    else
      return addToBuffer(dataItem, {{"VALUE", value}}, timestamp);
  }

  // ----------------------------------------------------
  // Asset CRUD Methods
  // ----------------------------------------------------
  void Agent::addAsset(AssetPtr asset)
  {
    DevicePtr device = nullptr;
    auto uuid = asset->getDeviceUuid();
    if (uuid)
      device = findDeviceByUUIDorName(*uuid);
    else
      device = defaultDevice();

    if (asset->getDeviceUuid() && *asset->getDeviceUuid() != device->getUuid())
    {
      asset->setProperty("deviceUuid", *device->getUuid());
    }

    string aid = asset->getAssetId();
    if (aid[0] == '@')
    {
      if (aid.empty())
        aid = asset->getAssetId();
      aid.erase(0, 1);
      aid.insert(0, *device->getUuid());
    }
    if (aid != asset->getAssetId())
    {
      asset->setAssetId(aid);
    }

    auto old = m_assetBuffer.addAsset(asset);
    auto cdi = asset->isRemoved() ? device->getAssetRemoved() : device->getAssetChanged();
    if (cdi)
      addToBuffer(cdi, {{"assetType", asset->getType()}, {"VALUE", asset->getAssetId()}},
                  asset->getTimestamp());
    else
      LOG(error) << "Cannot find data item for asset removed or changed. Schema Version: "
                 << m_version;
  }

  AssetPtr Agent::addAsset(DevicePtr device, const std::string &document,
                           const std::optional<std::string> &id,
                           const std::optional<std::string> &type,
                           const std::optional<std::string> &time, entity::ErrorList &errors)
  {
    // Parse the asset
    auto entity = entity::XmlParser::parse(Asset::getRoot(), document, "1.7", errors);
    if (!entity)
    {
      LOG(warning) << "Asset could not be parsed";
      LOG(warning) << document;
      for (auto &e : errors)
        LOG(warning) << e->what();
      return nullptr;
    }

    auto asset = dynamic_pointer_cast<Asset>(entity);

    if (type && asset->getType() != *type)
    {
      stringstream msg;
      msg << "Asset types do not match: "
          << "Parsed type: " << asset->getType() << " does not match " << *type;
      LOG(warning) << msg.str();
      LOG(warning) << document;
      errors.emplace_back(make_unique<entity::EntityError>(msg.str()));
      return asset;
    }

    if (!id && !asset->hasProperty("assetId"))
    {
      stringstream msg;
      msg << "Asset does not have an assetId and assetId not given";
      LOG(warning) << msg.str();
      LOG(warning) << document;
      errors.emplace_back(make_unique<entity::EntityError>(msg.str()));
      return asset;
    }

    if (id)
      asset->setAssetId(*id);

    if (time)
      asset->setProperty("timestamp", *time);

    auto ad = asset->getDeviceUuid();
    if (!ad)
      asset->setProperty("deviceUuid", *device->getUuid());

    addAsset(asset);

    return asset;
  }

  bool Agent::removeAsset(DevicePtr device, const std::string &id,
                          const optional<Timestamp> inputTime)
  {
    auto asset = m_assetBuffer.removeAsset(id, inputTime);
    if (asset)
    {
      if (device == nullptr && asset->getDeviceUuid())
      {
        auto dp = m_deviceUuidMap.find(*asset->getDeviceUuid());
        if (dp != m_deviceUuidMap.end())
          device = dp->second;
      }
      if (device == nullptr)
        device = defaultDevice();

      addToBuffer(device->getAssetRemoved(), {{"assetType", asset->getType()}, {"VALUE", id}},
                  asset->getTimestamp());

      // Check if the asset changed id is the same as this asset.
      auto ptr = m_circularBuffer.getLatest().getEventPtr(device->getAssetChanged()->getId());
      if (ptr && ptr->getValue<string>() == id)
      {
        addToBuffer(device->getAssetChanged(),
                    {{"assetType", asset->getType()}, {"VALUE", "UNAVAILABLE"s}});
      }
    }

    return true;
  }

  bool Agent::removeAllAssets(const std::optional<std::string> device, const optional<string> type,
                              const optional<Timestamp> inputTime, AssetList &list)
  {
    std::lock_guard<AssetBuffer> lock(m_assetBuffer);
    getAssets(nullptr, numeric_limits<int32_t>().max() - 1, false, type, device, list);
    DevicePtr dev {nullptr};
    if (device)
      dev = findDeviceByUUIDorName(*device);
    for (auto a : list)
    {
      removeAsset(dev, a->getAssetId(), inputTime);
    }

    return true;
  }

  // -----------------------------------------------
  // Validation methods
  // -----------------------------------------------

  template <typename T>
  void Agent::checkRange(const Printer *printer, const T value, const T min, const T max,
                         const string &param, bool notZero) const
  {
    using namespace http_server;
    if (value <= min)
    {
      stringstream str;
      str << '\'' << param << '\'' << " must be greater than " << min;
      throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                         printer->mimeType(), http_server::status::bad_request);
    }
    if (value >= max)
    {
      stringstream str;
      str << '\'' << param << '\'' << " must be less than " << max;
      throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                         printer->mimeType(), http_server::status::bad_request);
    }
    if (notZero && value == 0)
    {
      stringstream str;
      str << '\'' << param << '\'' << " must not be zero(0)";
      throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                         printer->mimeType(), http_server::status::bad_request);
    }
  }

  void Agent::checkPath(const Printer *printer, const std::optional<std::string> &path,
                        const DevicePtr device, FilterSet &filter) const
  {
    using namespace http_server;
    try
    {
      auto pd = devicesAndPath(path, device);
      m_xmlParser->getDataItems(filter, pd);
    }
    catch (exception &e)
    {
      throw RequestError(e.what(), printError(printer, "INVALID_XPATH", e.what()),
                         printer->mimeType(), http_server::status::bad_request);
    }

    if (filter.empty())
    {
      string msg = "The path could not be parsed. Invalid syntax: " + *path;
      throw RequestError(msg.c_str(), printError(printer, "INVALID_XPATH", msg),
                         printer->mimeType(), http_server::status::bad_request);
    }
  }

  DevicePtr Agent::checkDevice(const Printer *printer, const std::string &uuid) const
  {
    using namespace http_server;
    auto dev = findDeviceByUUIDorName(uuid);
    if (!dev)
    {
      string msg("Could not find the device '" + uuid + "'");
      throw RequestError(msg.c_str(), printError(printer, "NO_DEVICE", msg), printer->mimeType(),
                         http_server::status::not_found);
    }

    return dev;
  }

  string Agent::devicesAndPath(const std::optional<string> &path, const DevicePtr device) const
  {
    string dataPath;

    if (device != nullptr)
    {
      string prefix;
      if (device->getName() == "Agent")
        prefix = "//Devices/Agent";
      else
        prefix = "//Devices/Device[@uuid=\"" + *device->getUuid() + "\"]";

      if (path)
      {
        stringstream parse(*path);
        string token;

        // Prefix path (i.e. "path1|path2" => "{prefix}path1|{prefix}path2")
        while (getline(parse, token, '|'))
          dataPath += prefix + token + "|";

        dataPath.erase(dataPath.length() - 1);
      }
      else
      {
        dataPath = prefix;
      }
    }
    else
    {
      dataPath = path ? *path : "//Devices/Device";
    }

    return dataPath;
  }

  // -------------------------------------------
  // ReST API Requests
  // -------------------------------------------

  http_server::Response Agent::probeRequest(const Printer *printer,
                                            const std::optional<std::string> &device)
  {
    NAMED_SCOPE("Agent::probeRequest");

    list<DevicePtr> deviceList;

    if (device)
    {
      auto dev = checkDevice(printer, *device);
      deviceList.emplace_back(dev);
    }
    else
    {
      deviceList = m_devices;
    }

    auto counts = m_assetBuffer.getCountsByType();
    return {http_server::status::ok,
            printer->printProbe(m_instanceId, m_circularBuffer.getBufferSize(),
                                m_circularBuffer.getSequence(), getMaxAssets(),
                                m_assetBuffer.getCount(), deviceList, &counts),
            printer->mimeType()};
  }

  http_server::Response Agent::currentRequest(const Printer *printer,
                                              const std::optional<std::string> &device,
                                              const std::optional<SequenceNumber_t> &at,
                                              const std::optional<std::string> &path)
  {
    using namespace http_server;
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
    return {http_server::status::ok, fetchCurrentData(printer, filter, at), printer->mimeType()};
  }

  http_server::Response Agent::sampleRequest(const Printer *printer, const int count,
                                             const std::optional<std::string> &device,
                                             const std::optional<SequenceNumber_t> &from,
                                             const std::optional<SequenceNumber_t> &to,
                                             const std::optional<std::string> &path)
  {
    using namespace http_server;
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

    return {http_server::status::ok,
            fetchSampleData(printer, filter, count, from, to, end, endOfBuffer),
            printer->mimeType()};
  }

  struct AsyncSampleResponse
  {
    AsyncSampleResponse(http_server::SessionPtr &session, boost::asio::io_context &context)
      : m_session(session), m_observer(context), m_last(chrono::system_clock::now())
    {
    }

    http_server::SessionPtr m_session;
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

  void Agent::streamSampleRequest(http_server::SessionPtr session, const Printer *printer,
                                  const int interval, const int heartbeatIn, const int count,
                                  const std::optional<std::string> &device,
                                  const std::optional<SequenceNumber_t> &from,
                                  const std::optional<std::string> &path)
  {
    NAMED_SCOPE("Agent::streamSampleRequest");

    using namespace http_server;
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
      m_dataItemMap[item]->addObserver(&asyncResponse->m_observer);

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
        net::bind_executor(m_strand,
                           boost::bind(&Agent::streamSampleWriteComplete, this, asyncResponse)));
  }

  void Agent::streamSampleWriteComplete(shared_ptr<AsyncSampleResponse> asyncResponse)
  {
    NAMED_SCOPE("Agent::streamSampleWriteComplete");

    if (asyncResponse->m_endOfBuffer)
    {
      using boost::placeholders::_1;
      using boost::placeholders::_2;

      asyncResponse->m_observer.wait(
          asyncResponse->m_heartbeat,
          net::bind_executor(m_strand,
                             boost::bind(&Agent::streamNextSampleChunk, this, asyncResponse, _1)));
    }
    else
    {
      streamNextSampleChunk(asyncResponse, boost::system::error_code {});
    }
  }

  void Agent::streamNextSampleChunk(shared_ptr<AsyncSampleResponse> asyncResponse,
                                    boost::system::error_code ec)
  {
    NAMED_SCOPE("Agent::streamNextSampleChunk");
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
            net::bind_executor(
                m_strand, boost::bind(&Agent::streamNextSampleChunk, this, asyncResponse, _1)));
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
          content, net::bind_executor(m_strand, boost::bind(&Agent::streamSampleWriteComplete, this,
                                                            asyncResponse)));
    }
  }

  struct AsyncCurrentResponse
  {
    AsyncCurrentResponse(http_server::SessionPtr session, net::io_context &context)
      : m_session(session), m_timer(context)
    {
    }

    http_server::SessionPtr m_session;
    chrono::milliseconds m_interval;
    const Printer *m_printer {nullptr};
    FilterSetOpt m_filter;
    boost::asio::steady_timer m_timer;
  };

  void Agent::streamCurrentRequest(SessionPtr session, const Printer *printer, const int interval,
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

  void Agent::streamNextCurrent(std::shared_ptr<AsyncCurrentResponse> asyncResponse,
                                boost::system::error_code ec)
  {
    using boost::placeholders::_1;

    asyncResponse->m_session->writeChunk(
        fetchCurrentData(asyncResponse->m_printer, asyncResponse->m_filter, nullopt),
        boost::asio::bind_executor(m_strand, [this, asyncResponse]() {
          asyncResponse->m_timer.expires_from_now(asyncResponse->m_interval);
          asyncResponse->m_timer.async_wait(boost::asio::bind_executor(
              m_strand, boost::bind(&Agent::streamNextCurrent, this, asyncResponse, _1)));
        }));
  }

  http_server::Response Agent::assetRequest(const Printer *printer, const int32_t count,
                                            const bool removed,
                                            const std::optional<std::string> &type,
                                            const std::optional<std::string> &device)
  {
    using namespace http_server;
    AssetList list;
    getAssets(printer, count, removed, type, device, list);

    return {status::ok,
            printer->printAssets(m_instanceId, m_assetBuffer.getMaxAssets(),
                                 m_assetBuffer.getCount(), list),
            printer->mimeType()};
  }

  http_server::Response Agent::assetIdsRequest(const Printer *printer,
                                               const std::list<std::string> &ids)
  {
    using namespace http_server;

    AssetList list;
    getAssets(printer, ids, list);

    return {status::ok,
            printer->printAssets(m_instanceId, m_assetBuffer.getMaxAssets(),
                                 m_assetBuffer.getCount(), list),
            printer->mimeType()};
  }

  http_server::Response Agent::putAssetRequest(const Printer *printer, const std::string &asset,
                                               const std::optional<std::string> &type,
                                               const std::optional<std::string> &device,
                                               const std::optional<std::string> &uuid)
  {
    using namespace http_server;

    entity::ErrorList errors;
    auto dev = checkDevice(printer, *device);
    auto ap = addAsset(dev, asset, uuid, type, nullopt, errors);
    if (!ap || errors.size() > 0)
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
            printer->printAssets(m_instanceId, m_circularBuffer.getBufferSize(),
                                 m_assetBuffer.getCount(), list),
            printer->mimeType()};
  }

  http_server::Response Agent::deleteAssetRequest(const Printer *printer,
                                                  const std::list<std::string> &ids)
  {
    using namespace http_server;
    std::lock_guard<AssetBuffer> lock(m_assetBuffer);

    AssetList list;
    getAssets(printer, ids, list);

    for (auto asset : list)
    {
      removeAsset(nullptr, asset->getAssetId());
    }

    return {status::ok,
            printer->printAssets(m_instanceId, m_circularBuffer.getBufferSize(),
                                 m_assetBuffer.getCount(), list),
            printer->mimeType()};
  }

  http_server::Response Agent::deleteAllAssetsRequest(const Printer *printer,
                                                      const std::optional<std::string> &device,
                                                      const std::optional<std::string> &type)
  {
    using namespace http_server;

    AssetList list;
    removeAllAssets(device, type, nullopt, list);

    return {status::ok,
            printer->printAssets(m_instanceId, m_circularBuffer.getBufferSize(),
                                 m_assetBuffer.getCount(), list),
            printer->mimeType()};
  }

  http_server::Response Agent::putObservationRequest(const Printer *printer,
                                                     const std::string &device,
                                                     const http_server::QueryMap observations,
                                                     const std::optional<std::string> &time)
  {
    using namespace http_server;

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
        addToBuffer(di, qp.second, ts);
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

  void AgentPipelineContract::deliverAssetCommand(entity::EntityPtr command)
  {
    const std::string &cmd = command->getValue<string>();
    if (cmd == "RemoveAsset")
    {
      string id = command->get<string>("assetId");
      auto device = command->maybeGet<string>("device");
      DevicePtr dev {nullptr};
      if (device)
        dev = m_agent->findDeviceByUUIDorName(*device);
      m_agent->removeAsset(dev, id);
    }
    else if (cmd == "RemoveAll")
    {
      string type = command->get<string>("type");
      auto device = command->maybeGet<string>("device");
      AssetList list;
      m_agent->removeAllAssets(device, type, nullopt, list);
    }
    else
    {
      LOG(error) << "Invalid assent command: " << cmd;
    }
  }

  void Agent::receiveCommand(const std::string &deviceName, const std::string &command,
                             const std::string &value, const std::string &source)
  {
    DevicePtr device {nullptr};
    device = findDeviceByUUIDorName(deviceName);

    std::string oldName, oldUuid;
    if (device)
    {
      oldName = *device->getComponentName();
      oldUuid = *device->getUuid();
    }

    static std::unordered_map<string, function<void(DevicePtr, const string &value)>>
        deviceCommands {
            {"uuid",
             [](DevicePtr device, const string &uuid) {
               if (!device->preserveUuid())
                 device->setUuid(uuid);
             }},
            {"manufacturer", mem_fn(&Device::setManufacturer)},
            {"station", mem_fn(&Device::setStation)},
            {"serialNumber", mem_fn(&Device::setSerialNumber)},
            {"description", mem_fn(&Device::setDescriptionValue)},
            {"nativeName",
             [](DevicePtr device, const string &name) { device->setProperty("nativeName", name); }},
            {"calibration",
             [](DevicePtr device, const string &value) {
               istringstream line(value);

               // Look for name|factor|offset triples
               string name, factor, offset;
               while (getline(line, name, '|') && getline(line, factor, '|') &&
                      getline(line, offset, '|'))
               {
                 // Convert to a floating point number
                 auto di = device->getDeviceDataItem(name);
                 if (!di)
                   LOG(warning) << "Cannot find data item to calibrate for " << name;
                 else
                 {
                   double fact_value = stod(factor);
                   double off_value = stod(offset);

                   device_model::data_item::UnitConversion conv(fact_value, off_value);
                   di->setConverter(conv);
                 }
               }
             }},
        };

    static std::unordered_map<string, string> adapterDataItems {
        {"adapterVersion", "_adapter_software_version"},
        {"mtconnectVersion", "_mtconnect_version"},
    };

    auto action = deviceCommands.find(command);
    if (action == deviceCommands.end())
    {
      auto agentDi = adapterDataItems.find(command);
      if (agentDi == adapterDataItems.end())
      {
        LOG(warning) << "Unknown command '" << command << "' for device '" << deviceName;
      }
      else
      {
        auto id = source + agentDi->second;
        auto di = getDataItemForDevice("Agent", id);
        if (di)
          addToBuffer(di, value);
        else
        {
          LOG(warning) << "Cannot find data item for the Agent device when processing command "
                       << command << " with value " << value << " for adapter " << source;
        }
      }
    }
    else
    {
      action->second(device, value);
      deviceChanged(device, oldUuid, oldName);
    }
  }

  // -------------------------------------------
  // Error formatting
  // -------------------------------------------

  string Agent::printError(const Printer *printer, const string &errorCode,
                           const string &text) const
  {
    LOG(debug) << "Returning error " << errorCode << ": " << text;
    if (printer)
      return printer->printError(m_instanceId, m_circularBuffer.getBufferSize(),
                                 m_circularBuffer.getSequence(), errorCode, text);
    else
      return errorCode + ": " + text;
  }

  // -------------------------------------------
  // Data Collection and Formatting
  // -------------------------------------------

  string Agent::fetchCurrentData(const Printer *printer, const FilterSetOpt &filterSet,
                                 const optional<SequenceNumber_t> &at)
  {
    ObservationList observations;
    SequenceNumber_t firstSeq, seq;

    {
      std::lock_guard<CircularBuffer> lock(m_circularBuffer);

      firstSeq = getFirstSequence();
      seq = m_circularBuffer.getSequence();
      if (at)
      {
        checkRange(printer, *at, firstSeq - 1, seq, "at");

        auto check = m_circularBuffer.getCheckpointAt(*at, filterSet);
        check->getObservations(observations);
      }
      else
      {
        m_circularBuffer.getLatest().getObservations(observations, filterSet);
      }
    }

    return printer->printSample(m_instanceId, m_circularBuffer.getBufferSize(), seq, firstSeq,
                                seq - 1, observations);
  }

  string Agent::fetchSampleData(const Printer *printer, const FilterSetOpt &filterSet, int count,
                                const std::optional<SequenceNumber_t> &from,
                                const std::optional<SequenceNumber_t> &to, SequenceNumber_t &end,
                                bool &endOfBuffer, ChangeObserver *observer)
  {
    std::unique_ptr<ObservationList> observations;
    SequenceNumber_t firstSeq, lastSeq;

    {
      std::lock_guard<CircularBuffer> lock(m_circularBuffer);
      firstSeq = getFirstSequence();
      auto seq = m_circularBuffer.getSequence();
      lastSeq = seq - 1;
      int upperCountLimit = m_circularBuffer.getBufferSize() + 1;
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

      observations =
          m_circularBuffer.getObservations(count, filterSet, from, to, end, firstSeq, endOfBuffer);

      if (observer)
        observer->reset();
    }

    return printer->printSample(m_instanceId, m_circularBuffer.getBufferSize(), end, firstSeq,
                                lastSeq, *observations);
  }

  // -------- Asset Helpers -------

  void Agent::getAssets(const Printer *printer, const std::list<std::string> &ids, AssetList &list)
  {
    std::lock_guard<AssetBuffer> lock(m_assetBuffer);

    using namespace http_server;
    for (auto &id : ids)
    {
      auto asset = m_assetBuffer.getAsset(id);
      if (!asset)
      {
        string msg = "Cannot find asset for assetId: " + id;
        throw RequestError(msg.c_str(), printError(printer, "ASSET_NOT_FOUND", msg),
                           printer->mimeType(), status::not_found);
      }
      list.emplace_back(asset);
    }
  }

  void Agent::getAssets(const Printer *printer, const int32_t count, const bool removed,
                        const std::optional<std::string> &type,
                        const std::optional<std::string> &device, AssetList &list)
  {
    if (printer)
      checkRange(printer, count, 1, std::numeric_limits<int32_t>().max(), "count");

    std::lock_guard<AssetBuffer> lock(m_assetBuffer);

    DevicePtr dev {nullptr};
    if (device)
    {
      dev = checkDevice(printer, *device);
    }
    if (type)
    {
      auto iopt = m_assetBuffer.getAssetsForType(*type);
      if (iopt)
      {
        for (auto &ap : **iopt)
        {
          // Check if we need to filter
          if (removed || !ap.second->isRemoved())
          {
            if (!device ||
                (ap.second->getDeviceUuid() && dev->getUuid() == *ap.second->getDeviceUuid()))
            {
              list.emplace_back(ap.second);
            }

            if (list.size() >= count)
              break;
          }
        }
      }
    }
    else if (dev != nullptr)
    {
      auto iopt = m_assetBuffer.getAssetsForDevice(*dev->getUuid());
      if (iopt)
      {
        for (auto &ap : **iopt)
        {
          if (removed || !ap.second->isRemoved())
          {
            list.emplace_back(ap.second);
            if (list.size() >= count)
              break;
          }
        }
      }
    }
    else
    {
      for (auto &ap : reverse(m_assetBuffer.getAssets()))
      {
        if (removed || !ap->isRemoved())
        {
          list.emplace_back(ap);
          if (list.size() >= count)
            break;
        }
      }
    }
  }

  // -------------------------------------------
  // End
  // -------------------------------------------
}  // namespace mtconnect
