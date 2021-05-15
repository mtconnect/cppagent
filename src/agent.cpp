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

#include "asset/asset.hpp"
#include "asset/cutting_tool.hpp"
#include "asset/file_asset.hpp"
#include "device_model/agent_device.hpp"
#include "entity/xml_parser.hpp"
#include "json_printer.hpp"
#include "logging.hpp"
#include "observation/observation.hpp"
#include "rest_service/file_cache.hpp"
#include "rest_service/session.hpp"
#include "xml_printer.hpp"

using namespace std;
using namespace mtconnect::observation;

namespace mtconnect
{
  using namespace device_model;
  using namespace data_item;
  using namespace entity;
  using namespace rest_service;
  namespace net = boost::asio;

  static const string g_unavailable("UNAVAILABLE");
  static const string g_available("AVAILABLE");

  // Agent public methods
  Agent::Agent(boost::asio::io_context &context, const string &configXmlPath,
               const std::string &version, bool pretty)
    : m_context(context),
      m_strand(m_context),
      m_xmlParser(make_unique<mtconnect::XmlParser>()),
      m_version(version),
      m_configXmlPath(configXmlPath),
      m_pretty(pretty)
  {
    CuttingToolArchetype::registerAsset();
    CuttingTool::registerAsset();
    FileArchetypeAsset::registerAsset();
    FileAsset::registerAsset();

    // Create the Printers
    m_printers["xml"] = make_unique<XmlPrinter>(version, m_pretty);
    m_printers["json"] = make_unique<Printer>(version, m_pretty);
  }

  void Agent::initialize(pipeline::PipelineContextPtr context, const ConfigOptions &options)
  {
    NAMED_SCOPE("Agent::initialize");

    m_loopback = std::make_unique<LoopbackPipeline>(context);
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
      //m_server->start();
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
    // m_server->stop();

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

        m_loopback->addObservation(d, *value);
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
          m_loopback->addObservation(d, uuid);
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
        m_loopback->addObservation(d, oldUuid);
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
        m_loopback->addObservation(d, uuid);
      }
      else
      {
        auto d = m_agentDevice->getDeviceDataItem("device_changed");
        m_loopback->addObservation(d, uuid);
      }
    }
  }

  void Agent::loadCachedProbe()
  {
    NAMED_SCOPE("Agent::loadCachedProbe");

    // Reload the document for path resolution
    auto xmlPrinter = dynamic_cast<XmlPrinter *>(m_printers["xml"].get());
    m_xmlParser->loadDocument(xmlPrinter->printProbe(0, 0, 0, 0, 0, m_devices));
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
      m_loopback->addObservation(di, "LISTENING");
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
      m_loopback->addObservation(di, "CLOSED");
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
          auto ptr = getLatest(dataItem->getId());

          if (ptr)
          {
            const string *value = nullptr;
            if (dataItem->getConstantValue())
              value = &dataItem->getConstantValue().value();
            else if (!ptr->isUnavailable())
              value = &g_unavailable;

            if (value)
              m_loopback->addObservation(dataItem, *value);
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
      m_loopback->addObservation(di, "ESTABLISHED");
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
        m_loopback->addObservation(avail, g_available);
      }
      else
        LOG(debug) << "Cannot find availability for " << *device->getComponentName();
    }
  }


  // -----------------------------------------------
  // Validation methods
  // -----------------------------------------------

  template <typename T>
  void Agent::checkRange(const Printer *printer, const T value, const T min, const T max,
                         const string &param, bool notZero) const
  {
    using namespace rest_service;
    if (value <= min)
    {
      stringstream str;
      str << '\'' << param << '\'' << " must be greater than " << min;
      throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                         printer->mimeType(), rest_service::status::bad_request);
    }
    if (value >= max)
    {
      stringstream str;
      str << '\'' << param << '\'' << " must be less than " << max;
      throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                         printer->mimeType(), rest_service::status::bad_request);
    }
    if (notZero && value == 0)
    {
      stringstream str;
      str << '\'' << param << '\'' << " must not be zero(0)";
      throw RequestError(str.str().c_str(), printError(printer, "OUT_OF_RANGE", str.str()),
                         printer->mimeType(), rest_service::status::bad_request);
    }
  }

  void Agent::checkPath(const Printer *printer, const std::optional<std::string> &path,
                        const DevicePtr device, FilterSet &filter) const
  {
    using namespace rest_service;
    try
    {
      auto pd = devicesAndPath(path, device);
      m_xmlParser->getDataItems(filter, pd);
    }
    catch (exception &e)
    {
      throw RequestError(e.what(), printError(printer, "INVALID_XPATH", e.what()),
                         printer->mimeType(), rest_service::status::bad_request);
    }

    if (filter.empty())
    {
      string msg = "The path could not be parsed. Invalid syntax: " + *path;
      throw RequestError(msg.c_str(), printError(printer, "INVALID_XPATH", msg),
                         printer->mimeType(), rest_service::status::bad_request);
    }
  }

  DevicePtr Agent::checkDevice(const Printer *printer, const std::string &uuid) const
  {
    using namespace rest_service;
    auto dev = findDeviceByUUIDorName(uuid);
    if (!dev)
    {
      string msg("Could not find the device '" + uuid + "'");
      throw RequestError(msg.c_str(), printError(printer, "NO_DEVICE", msg), printer->mimeType(),
                         rest_service::status::not_found);
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
          m_loopback->addObservation(di, value);
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

#if 0
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

    using namespace rest_service;
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
#endifØ
  // -------------------------------------------
  // End
  // -------------------------------------------
}  // namespace mtconnect
