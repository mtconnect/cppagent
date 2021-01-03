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

#include "agent.hpp"

#include "agent_device.hpp"
#include "json_printer.hpp"
#include "xml_printer.hpp"
#include "asset.hpp"
#include "file_asset.hpp"
#include "cutting_tool.hpp"

#include "entity/xml_parser.hpp"
#include <sys/stat.h>

#include <dlib/config_reader.h>
#include <dlib/dir_nav.h>
#include <dlib/logger.h>
#include <dlib/misc_api.h>
#include <dlib/queue.h>
#include <dlib/tokenizer.h>

#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace std;

namespace mtconnect
{
  static const string g_unavailable("UNAVAILABLE");
  static const string g_conditionUnavailable("UNAVAILABLE|||");

  static const string g_available("AVAILABLE");
  static dlib::logger g_logger("agent");
  
  // Agent public methods
  Agent::Agent(const string &configXmlPath, int bufferSize, int maxAssets,
               const std::string &version, int checkpointFreq, bool pretty)
      : m_logStreamData(false),
        m_circularBuffer(bufferSize, checkpointFreq),
        m_pretty(pretty),        
        m_assetBuffer(maxAssets),
        m_xmlParser(make_unique<XmlParser>())
  
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

    auto xmlPrinter = dynamic_cast<XmlPrinter *>(m_printers["xml"].get());

    // TODO: Check Schema version before creating Agent Device. Only > 1.7'
    int major, minor;
    char c;
    stringstream vstr(version);
    vstr >> major >> c >> minor;
    if (major > 1 || (major == 1 && minor >= 7))
    {
      createAgentDevice();
    }
    loadXMLDeviceFile(configXmlPath);
    loadCachedProbe();
    
    m_initialized = true;
  }

  void Agent::createAgentDevice()
  {
    // Create the Agent Device
    m_agentDevice = new AgentDevice({{"name", "Agent"},
                                     {"uuid", "0b49a3a0-18ca-0139-8748-2cde48001122"},
                                     {"id", "agent_2cde48001122"},
                                     {"mtconnectVersion", "1.7"}});

    addDevice(m_agentDevice);
  }

  void Agent::loadXMLDeviceFile(const std::string &configXmlPath)
  {
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
      g_logger << LFATAL << "Error loading xml configuration: " + configXmlPath;
      g_logger << LFATAL << "Error detail: " << e.what();
      cerr << e.what() << endl;
      throw e;
    }
    catch (exception &f)
    {
      g_logger << LFATAL << "Error loading xml configuration: " + configXmlPath;
      g_logger << LFATAL << "Error detail: " << f.what();
      cerr << f.what() << endl;
      throw f;
    }
  }

  void Agent::verifyDevice(Device *device)
  {
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
      auto di = new DataItem(
          {{"type", "AVAILABILITY"}, {"id", device->getId() + "_avail"}, {"category", "EVENT"}});
      di->setComponent(*device);
      device->addDataItem(di);
      device->m_availabilityAdded = true;
    }

    int major, minor;
    char c;
    stringstream ss(schemaVersion);
    ss >> major >> c >> minor;
    if (!device->getAssetChanged() && (major > 1 || (major == 1 && minor >= 2)))
    {
      // Create asset change data item and add it to the device.
      auto di = new DataItem({{"type", "ASSET_CHANGED"},
                              {"id", device->getId() + "_asset_chg"},
                              {"category", "EVENT"}});
      di->setComponent(*device);
      device->addDataItem(di);
    }

    if (device->getAssetChanged() && (major > 1 || (major == 1 && minor >= 5)))
    {
      auto di = device->getAssetChanged();
      di->makeDiscrete();
    }

    if (!device->getAssetRemoved() && (major > 1 || (major == 1 && minor >= 3)))
    {
      // Create asset removed data item and add it to the device.
      auto di = new DataItem({{"type", "ASSET_REMOVED"},
                              {"id", device->getId() + "_asset_rem"},
                              {"category", "EVENT"}});
      di->setComponent(*device);
      device->addDataItem(di);
    }
  }

  void Agent::initializeDataItems(Device *device)
  {
    // Grab data from configuration
    string time = getCurrentTime(GMT_UV_SEC);

    // Initialize the id mapping for the devices and set all data items to UNAVAILABLE
    for (auto item : device->getDeviceDataItems())
    {
      auto d = item.second;
      if (!d->isInitialized())
      {
        // Check for single valued constrained data items.
        const string *value = &g_unavailable;
        if (d->isCondition())
          value = &g_conditionUnavailable;
        else if (d->hasConstantValue())
          value = &(d->getConstrainedValues()[0]);

        addToBuffer(d, *value, time);
        if (!m_dataItemMap.count(d->getId()))
          m_dataItemMap[d->getId()] = d;
        else
        {
          g_logger << LFATAL << "Duplicate DataItem id " << d->getId()
                   << " for device: " << device->getName()
                   << " and data item name: " << d->getName();
          std::exit(1);
        }

        d->setInitialized();
      }
    }
  }

  // Add the a device from a configuration file
  void Agent::addDevice(Device *device)
  {
    // Check if device already exists
    auto old = m_deviceUuidMap.find(device->getUuid());
    if (old != m_deviceUuidMap.end())
    {
      // Update existing device
      g_logger << LFATAL << "Device " << device->getUuid() << " already exists. "
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
        m_deviceNameMap[device->getName()] = device;
        m_deviceUuidMap[device->getUuid()] = device;

        device->resolveReferences();
        verifyDevice(device);
        initializeDataItems(device);

        // Check for single valued constrained data items.
        if (m_agentDevice && device != m_agentDevice)
        {
          auto d = m_agentDevice->getDeviceDataItem("device_added");
          addToBuffer(d, device->getUuid(), getCurrentTime(GMT_UV_SEC));
        }
      }
      else
        g_logger << LWARN << "Adding device " << device->getUuid()
                 << " after initialialization not supported yet";
    }
  }

  void Agent::loadCachedProbe()
  {
    // Reload the document for path resolution
    auto xmlPrinter = dynamic_cast<XmlPrinter *>(m_printers["xml"].get());
    m_xmlParser->loadDocument(xmlPrinter->printProbe(m_instanceId,
                                                     m_circularBuffer.getBufferSize(),
                                                     getMaxAssets(),    m_assetBuffer.getCount(),
                                                     m_circularBuffer.getSequence(),
                                                     m_devices));
  }

  const Device *Agent::getDeviceByName(const std::string &name) const
  {
    auto devPos = m_deviceNameMap.find(name);
    if (devPos != m_deviceNameMap.end())
      return devPos->second;

    return nullptr;
  }

  Device *Agent::getDeviceByName(const std::string &name)
  {
    auto devPos = m_deviceNameMap.find(name);
    if (devPos != m_deviceNameMap.end())
      return devPos->second;

    return nullptr;
  }

  Device *Agent::findDeviceByUUIDorName(const std::string &idOrName)
  {
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

  Agent::~Agent()
  {
    m_xmlParser.reset();
    m_agentDevice = nullptr;
    for (auto &i : m_devices)
      delete i;
    m_devices.clear();
  }

  void Agent::start()
  {
    try
    {
      // Start all the adapters
      for (const auto adapter : m_adapters)
        adapter->start();

      // Start the server. This blocks until the server stops.
      //server_http::start();
    }
    catch (dlib::socket_error &e)
    {
      g_logger << LFATAL << "Cannot start server: " << e.what();
      std::exit(1);
    }
  }

  void Agent::stop()
  {
    // Stop all adapter threads...
    g_logger << LINFO << "Shutting down adapters";
    // Deletes adapter and waits for it to exit.
    for (const auto adapter : m_adapters)
      adapter->stop();

    g_logger << LINFO << "Shutting down server";
    //server::http_1a::clear();
    g_logger << LINFO << "Shutting completed";

    for (const auto adapter : m_adapters)
      delete adapter;

    m_adapters.clear();
  }

  const Printer *Agent::printerForAccepts(const std::string &accepts) const
  {
    stringstream list(accepts);
    string accept;
    while (getline(list, accept, ','))
    {
      for (const auto &p : m_printers)
      {
        if (endsWith(accept, p.first))
          return p.second.get();
      }
    }

    return nullptr;
  }

  // Methods for service
  void Agent::addAdapter(Adapter *adapter, bool start)
  {
    adapter->setAgent(*this);
    m_adapters.emplace_back(adapter);

    const auto dev = getDeviceByName(adapter->getDeviceName());
    if (dev && dev->m_availabilityAdded)
      adapter->setAutoAvailable(true);

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

  unsigned int Agent::addToBuffer(DataItem *dataItem, const string &value, string time)
  {
    if (!dataItem)
      return 0;

    std::lock_guard<CircularBuffer> lock(m_circularBuffer);

    RefCountedPtr<Observation> event(new Observation(*dataItem, time, value), true);
    if (!dataItem->allowDups() && dataItem->isDataSet() && !m_circularBuffer.getLatest().dataSetDifference(event))
    {
      return 0;
    }
    
    auto seqNum = m_circularBuffer.addToBuffer(event);

    dataItem->signalObservers(seqNum);

    return seqNum;
  }

  bool Agent::addAsset(Device *device, const string &id, const string &doc,
                       const string &type, const string &inputTime,
                       entity::ErrorList &errors)
  {
    // TODO: Error handling for parse errors.
    
    // Parse the asset
    entity::XmlParser parser;
    
    auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
    if (!entity)
    {
      g_logger << LWARN << "Asset '" << id << " could not be parsed";
      g_logger << LWARN << doc;
      for (auto &e : errors)
        g_logger << LWARN << e->what();
      return false;
    }

    auto asset = dynamic_pointer_cast<Asset>(entity);
    string aid = id;
    if (aid[0] == '@' || asset->getAssetId()[0] == '@')
    {
      if (aid.empty())
        aid = asset->getAssetId();
      aid.erase(0, 1);
      aid.insert(0, device->getUuid());
    }
    if (aid != asset->getAssetId())
    {
      asset->setAssetId(aid);
    }
    
    auto ts = asset->getTimestamp();
    if (!ts && !inputTime.empty())
    {
      asset->setProperty("timestamp", inputTime);
    }
    
    auto ad = asset->getDeviceUuid();
    if (!ad)
    {
      asset->setProperty("deviceUuid", device->getUuid());
    }

    auto old = m_assetBuffer.addAsset(asset);
    
    auto at = asset->getTimestamp();
    if (asset->isRemoved())
      addToBuffer(device->getAssetRemoved(), type + "|" + id, *at);
    else
      addToBuffer(device->getAssetChanged(), type + "|" + id, *at);

    return true;
  }
  

  bool Agent::removeAsset(Device *device, const std::string &id, const string &inputTime)
  {
    // TODO: Error handling for parse errors.

    auto asset = m_assetBuffer.removeAsset(id, inputTime);
    if (asset)
    {
      auto time = asset->getTimestamp();
      addToBuffer(device->getAssetRemoved(), asset->getType() + "|" + id, *time);
      
      // Check if the asset changed id is the same as this asset.
      auto ptr = m_circularBuffer.getLatest().getEventPtr(device->getAssetChanged()->getId());
      if (ptr && (*ptr)->getValue() == id)
      {
        addToBuffer(device->getAssetChanged(), asset->getType() + "|UNAVAILABLE",
                    *time);
      }
    }

    return true;
  }

  bool Agent::removeAllAssets(Device *device, const std::string &type,
                              const std::string &inputTime)
  {
    std::lock_guard<AssetBuffer> lock(m_assetBuffer);
    auto assets = m_assetBuffer.getAssetsForType(type);
    auto uuid = device->getUuid();
    for (auto a : assets)
    {
      if (!a.second->getDeviceUuid() || a.second->getDeviceUuid() == uuid)
        removeAsset(device, a.second->getAssetId(), inputTime);
    }
    
    return true;
  }
  
  void Agent::connecting(Adapter *adapter)
  {
    if (m_agentDevice)
    {
      auto di = m_agentDevice->getConnectionStatus(adapter);
      addToBuffer(di, "LISTENING");
    }
  }

  // Add values for related data items UNAVAILABLE
  void Agent::disconnected(Adapter *adapter, std::vector<Device *> devices)
  {
    auto time = getCurrentTime(GMT_UV_SEC);
    g_logger << LDEBUG << "Disconnected from adapter, setting all values to UNAVAILABLE";

    if (m_agentDevice)
    {
      auto di = m_agentDevice->getConnectionStatus(adapter);
      addToBuffer(di, "CLOSED", time);
    }
    
    for (const auto device : devices)
    {
      const auto &dataItems = device->getDeviceDataItems();
      for (const auto &dataItemAssoc : dataItems)
      {
        auto dataItem = dataItemAssoc.second;
        if (dataItem && (dataItem->getDataSource() == adapter ||
                         (adapter->isAutoAvailable() && !dataItem->getDataSource() &&
                          dataItem->getType() == "AVAILABILITY")))
        {
          auto ptr = m_circularBuffer.getLatest().getEventPtr(dataItem->getId());

          if (ptr)
          {
            const string *value = nullptr;
            if (dataItem->isCondition())
            {
              if ((*ptr)->getLevel() != Observation::UNAVAILABLE)
                value = &g_conditionUnavailable;
            }
            else if (dataItem->hasConstraints())
            {
              const auto &values = dataItem->getConstrainedValues();
              if (values.size() > 1 && (*ptr)->getValue() != g_unavailable)
                value = &g_unavailable;
            }
            else if ((*ptr)->getValue() != g_unavailable)
              value = &g_unavailable;

            if (value && !adapter->isDuplicate(dataItem, *value, NAN))
              addToBuffer(dataItem, *value, time);
          }
        }
        else if (!dataItem)
          g_logger << LWARN << "No data Item for " << dataItemAssoc.first;
      }
    }
  }

  void Agent::connected(Adapter *adapter, std::vector<Device *> devices)
  {
    auto time = getCurrentTime(GMT_UV_SEC);
    if (m_agentDevice)
    {
      auto di = m_agentDevice->getConnectionStatus(adapter);
      addToBuffer(di, "ESTABLISHED", time);
    }
    
    if (!adapter->isAutoAvailable())
      return;

    for (const auto device : devices)
    {
      g_logger << LDEBUG
               << "Connected to adapter, setting all Availability data items to AVAILABLE";

      if (device->getAvailability())
      {
        g_logger << LDEBUG << "Adding availabilty event for " << device->getAvailability()->getId();
        addToBuffer(device->getAvailability(), g_available, time);
      }
      else
        g_logger << LDEBUG << "Cannot find availability for " << device->getName();
    }
  }
  
#if 0
  string Agent::handlePut(const Printer *printer, ostream &out, const string &path,
                          const key_value_map &queries, const string &adapter,
                          const string &deviceName)
  {
    // TODO: Improve error handling for failed PUTS.

    string device = deviceName;
    if (device.empty() && adapter.empty())
      return printError(printer, "UNSUPPORTED", "Device must be specified for PUT");
    else if (device.empty())
      device = adapter;

    auto dev = findDeviceByUUIDorName(device);
    if (!dev)
    {
      string message = ((string) "Cannot find device: ") + device;
      return printError(printer, "UNSUPPORTED", message);
    }

    // First check if this is an adapter put or a data put...
    if (queries["_type"] == "command")
    {
      for (const auto adpt : dev->m_adapters)
      {
        for (const auto &kv : queries)
        {
          string command = kv.first + "=" + kv.second;
          g_logger << LDEBUG << "Sending command '" << command << "' to " << device;
          adpt->sendCommand(command);
        }
      }
    }
    else
    {
      string time = queries["time"];
      if (time.empty())
        time = getCurrentTime(GMT_UV_SEC);

      for (const auto &kv : queries)
      {
        if (kv.first != "time")
        {
          auto di = dev->getDeviceDataItem(kv.first);
          if (di)
            addToBuffer(di, kv.second, time);
          else
            g_logger << LWARN << "(" << device << ") Could not find data item: " << kv.first;
        }
      }
    }

    return "<success/>";
  }

  string Agent::handleProbe(const Printer *printer, const string &name)
  {
    std::vector<Device *> deviceList;

    if (!name.empty())
    {
      auto device = findDeviceByUUIDorName(name);
      if (!device)
        return printError(printer, "NO_DEVICE", "Could not find the device '" + name + "'");
      else
        deviceList.emplace_back(device);
    }
    else
      deviceList = m_devices;

    auto counts = m_assetBuffer.getCountsByType();
    
    return printer->printProbe(m_instanceId, m_circularBuffer.getBufferSize(), m_circularBuffer.getSequence(), getMaxAssets(),
                               m_assetBuffer.getCount(), deviceList,
                               &counts);
  }
  string Agent::handleStream(const Printer *printer, ostream &out, const string &path, bool current,
                             unsigned int frequency, uint64_t start, int count,
                             std::chrono::milliseconds heartbeat)
  {
    std::set<string> filter;
    try
    {
      m_xmlParser->getDataItems(filter, path);
    }
    catch (exception &e)
    {
      return printError(printer, "INVALID_XPATH", e.what());
    }

    if (filter.empty())
      return printError(printer, "INVALID_XPATH",
                        "The path could not be parsed. Invalid syntax: " + path);

    // Check if there is a frequency to stream data or not
    if (frequency != (unsigned)NO_FREQ)
    {
      streamData(printer, out, filter, current, frequency, start, count, heartbeat);
      return "";
    }
    else
    {
      uint64_t end;
      bool endOfBuffer;
      if (current)
        return fetchCurrentData(printer, filter, start);
      else
        return fetchSampleData(printer, filter, start, count, end, endOfBuffer);
    }
  }
  std::string Agent::handleAssets(const Printer *printer, ostream &aOut,
                                  const key_value_map &queries,
                                  const string &list)
  {
    AssetList assets;
    if (!list.empty())
    {
      lock_guard<AssetBuffer> lock(m_assetBuffer);
      stringstream str(list);
      string id;
      while (getline(str, id, ','))
      {
        auto asset = m_assetBuffer.getAsset(id);
        if (asset)
        {
          assets.emplace_back(asset);
        }
        else
        {
          // TODO: Handle multiple errors
          return printer->printError(m_instanceId, 0, 0, "ASSET_NOT_FOUND",
                                     (string) "Could not find asset: " + id);
        }
      }
    }
    else
    {
      auto type = queries.find("type");
      auto device = queries.find("device");
      auto removedQuery = queries.find("removed");
      auto removed = removedQuery != queries.end() && removedQuery->second == "true";
      auto count = checkAndGetParam(queries, "count", m_assetBuffer.getCount(false),
                                    1, false, NO_VALUE32);

      if (type != queries.end())
      {
        auto list = m_assetBuffer.getAssetsForType(type->second);
        for (auto asset : reverse(list))
        {
          if ((!asset.second->isRemoved() || removed) &&
              (device == queries.end() ||
               (asset.second->getDeviceUuid() &&
                asset.second->getDeviceUuid() == device->second)))
          {
            assets.emplace_back(asset.second);
            if (assets.size() >= count)
              break;
          }
        }
      }
      else if (device != queries.end())
      {
        auto list = m_assetBuffer.getAssetsForDevice(device->second);
        for (auto asset : reverse(list))
        {
          if (!asset.second->isRemoved() || removed)
          {
            assets.emplace_back(asset.second);
            if (assets.size() >= count)
              break;
          }
        }
      }
      else
      {
        for (auto asset : reverse(m_assetBuffer.getAssets()))
        {
          if (!asset->isRemoved() || removed)
          {
            assets.emplace_back(asset);
            if (assets.size() >= count)
              break;
          }
        }
      }
    }
    
    // TODO: Count should still have removed. Maybe make configurable
    return printer->printAssets(m_instanceId, getMaxAssets(),
                                m_assetBuffer.getCount(), assets);
  }
  
  // Store an asset in the map by asset # and use the circular buffer as
  // an LRU. Check if we're removing an existing asset and clean up the
  // map, and then store this asset.
  std::string Agent::storeAsset(std::ostream &aOut, const key_value_map &queries,
                                const std::string &command, const std::string &id,
                                const std::string &body)
  {
    string deviceId = queries["device"];
    string type = queries["type"];
    Device *device = nullptr;

    if (!deviceId.empty())
      device = findDeviceByUUIDorName(deviceId);

    // If the device was not found or was not provided, use the default device.
    if (!device)
      device = m_devices[0];

    if (command == "PUT" || command == "POST")
    {
      entity::ErrorList errors;
      bool added = addAsset(device, id, body, type, "", errors);
      if (!added || errors.size() > 0)
      {
        ProtoErrorList errorResp;
        if (!added)
          errorResp.emplace_back("INVALID_REQUEST", "Could not parse Asset: " + id);
        else
          errorResp.emplace_back("INVALID_REQUEST", "Asset parsed with errors: " + id);
        for (auto &e : errors)
        {
          errorResp.emplace_back("INVALID_REQUEST", e->what());
        }
        auto printer = m_printers["xml"].get();
        return printer->printErrors(m_instanceId, m_circularBuffer.getBufferSize(),
                                    m_circularBuffer.getSequence(),
                                    errorResp);
      }
      else
      {
        return "<success/>";
      }
    }
    else if (command == "DELETE")
    {
      if (removeAsset(device, id, ""))
        return "<success/>";
      else
        return "<failure>Cannot remove asset (" + id + ")</failure>";
    }

    // TODO: Improve error handling. Iterate errors from parser
    return "<failure>Bad Command:" + command + "</failure>";
  }

  string Agent::handleFile(const string &uri, OutgoingThings &outgoing)
  {
    // Get the mime type for the file.
    bool unknown = true;
    auto last = uri.find_last_of("./");
    string contentType;
    if (last != string::npos && uri[last] == '.')
    {
      auto ext = uri.substr(last + 1u);
      if (m_mimeTypes.count(ext) > 0)
      {
        contentType = m_mimeTypes.at(ext);
        unknown = false;
      }
    }

    if (unknown)
      contentType = "application/octet-stream";

    // Check if the file is cached
    RefCountedPtr<CachedFile> cachedFile;
    auto cached = m_fileCache.find(uri);
    if (cached != m_fileCache.end())
      cachedFile = cached->second;
    else
    {
      auto file = m_fileMap.find(uri);

      // Should never happen
      if (file == m_fileMap.end())
      {
        outgoing.http_return = 404;
        outgoing.http_return_status = "File not found";
        return "";
      }

      const auto path = file->second.c_str();

      struct stat fs;
      auto res = stat(path, &fs);
      if (res)
      {
        outgoing.http_return = 404;
        outgoing.http_return_status = "File not found";
        return "";
      }

      auto fd = open(path, O_RDONLY | O_BINARY);
      if (res < 0)
      {
        outgoing.http_return = 404;
        outgoing.http_return_status = "File not found";
        return "";
      }

      cachedFile.setObject(new CachedFile(fs.st_size), true);
      auto bytes = read(fd, cachedFile->m_buffer.get(), fs.st_size);
      close(fd);

      if (bytes < fs.st_size)
      {
        outgoing.http_return = 404;
        outgoing.http_return_status = "File not found";
        return "";
      }

      // If this is a small file, cache it.
      if (bytes <= SMALL_FILE)
        m_fileCache.insert(pair<string, RefCountedPtr<CachedFile>>(uri, cachedFile));
    }

    (*outgoing.m_out) << "HTTP/1.1 200 OK\r\n"
                         "Date: "
                      << getCurrentTime(HUM_READ)
                      << "\r\n"
                         "Server: MTConnectAgent\r\n"
                         "Connection: close\r\n"
                         "Content-Length: "
                      << cachedFile->m_size
                      << "\r\n"
                         "Expires: "
                      << getCurrentTime(
                             std::chrono::system_clock::now() + std::chrono::seconds(60 * 60 * 24),
                             HUM_READ)
                      << "\r\n"
                         "Content-Type: "
                      << contentType << "\r\n\r\n";

    outgoing.m_out->write(cachedFile->m_buffer.get(), cachedFile->m_size);
    outgoing.m_out->setstate(ios::badbit);

    return "";
  }
  
  void Agent::streamData(const Printer *printer, ostream &out, std::set<string> &filterSet,
                         bool current, unsigned int interval, uint64_t start, unsigned int count,
                         std::chrono::milliseconds heartbeat)
  {
    // Create header
    string boundary = md5(intToString(time(nullptr)));

    ofstream log;
    if (m_logStreamData)
    {
      string filename = "Stream_" + getCurrentTime(LOCAL) + "_" +
                        int64ToString((uint64_t)dlib::get_thread_id()) + ".log";
      log.open(filename.c_str());
    }

    out << "HTTP/1.1 200 OK\r\n"
           "Date: "
        << getCurrentTime(HUM_READ)
        << "\r\n"
           "Server: MTConnectAgent\r\n"
           "Expires: -1\r\n"
           "Connection: close\r\n"
           "Cache-Control: private, max-age=0\r\n"
           "Content-Type: multipart/x-mixed-replace;boundary="
        << boundary
        << "\r\n"
           "Transfer-Encoding: chunked\r\n\r\n";

    // This object will automatically clean up all the observer from the
    // signalers in an exception proof manor.
    ChangeObserver observer;

    // Add observers
    for (const auto &item : filterSet)
      m_dataItemMap[item]->addObserver(&observer);

    chrono::milliseconds interMilli{interval};
    uint64_t firstSeq = getFirstSequence();
    if (start == NO_START || start < firstSeq)
      start = firstSeq;

    try
    {
      // Loop until the user closes the connection
      while (out.good())
      {
        // Remember when we started this grab...
        auto last = chrono::system_clock::now();

        // Fetch sample data now resets the observer while holding the sequence
        // mutex to make sure that a new event will be recorded in the observer
        // when it returns.
        string content;
        uint64_t end(0ull);
        bool endOfBuffer = true;
        if (current)
          content = fetchCurrentData(printer, filterSet, NO_START);
        else
        {
          // Check if we're falling too far behind. If we are, generate an
          // MTConnectError and return.
          if (start < getFirstSequence())
          {
            g_logger << LWARN << "Client fell too far behind, disconnecting";
            throw ParameterError("OUT_OF_RANGE",
                                 "Client can't keep up with event stream, disconnecting");
          }
          else
          {
            // end and endOfBuffer are set during the fetch sample data while the
            // mutex is held. This removed the race to check if we are at the end of
            // the bufffer and setting the next start to the last sequence number
            // sent.
            content =
                fetchSampleData(printer, filterSet, start, count, end, endOfBuffer, &observer);
          }

          if (m_logStreamData)
            log << content << endl;
        }

        ostringstream str;

        // Make sure we're terminated with a <cr><nl>
        content.append("\r\n");
        out.setf(ios::dec, ios::basefield);
        str << "--" << boundary
            << "\r\n"
               "Content-type: "
            << printer->mimeType()
            << "\r\n"
               "Content-length: "
            << content.length() << "\r\n\r\n"
            << content;

        string chunk = str.str();
        out.setf(ios::hex, ios::basefield);
        out << chunk.length() << "\r\n";
        out << chunk << "\r\n";
        out.flush();

        // Wait for up to frequency ms for something to arrive... Don't wait if
        // we are not at the end of the buffer. Just put the next set after aInterval
        // has elapsed. Check also if in the intervening time between the last fetch
        // and now. If so, we just spin through and wait the next interval.

        // Even if we are at the end of the buffer, or within range. If we are filtering,
        // we will need to make sure we are not spinning when there are no valid events
        // to be reported. we will waste cycles spinning on the end of the buffer when
        // we should be in a heartbeat wait as well.
        if (!endOfBuffer)
        {
          // If we're not at the end of the buffer, move to the end of the previous set and
          // begin filtering from where we left off.
          start = end;

          // For replaying of events, we will stream as fast as we can with a 1ms sleep
          // to allow other threads to run.
          this_thread::sleep_for(1ms);
        }
        else
        {
          chrono::milliseconds delta;

          if (!current)
          {
            // Busy wait to make sure the signal was actually signaled. We have observed that
            // a signal can occur in rare conditions where there are multiple threads listening
            // on separate condition variables and this pops out too soon. This will make sure
            // observer was actually signaled and instead of throwing an error will wait again
            // for the remaining hartbeat interval.
            delta = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - last);
            while (delta < heartbeat && observer.wait((heartbeat - delta).count()) &&
                   !observer.wasSignaled())
            {
              delta =
                  chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - last);
            }

            {
              std::lock_guard<CircularBuffer> lock(m_circularBuffer);

              // Make sure the observer was signaled!
              if (!observer.wasSignaled())
              {
                // If nothing came out during the last wait, we may have still have advanced
                // the sequence number. We should reset the start to something closer to the
                // current sequence. If we lock the sequence lock, we can check if the observer
                // was signaled between the time the wait timed out and the mutex was locked.
                // Otherwise, nothing has arrived and we set to the next sequence number to
                // the next sequence number to be allocated and continue.
                start = m_circularBuffer.getSequence();
              }
              else
              {
                // Get the sequence # signaled in the observer when the earliest event arrived.
                // This will allow the next set of data to be pulled. Any later events will have
                // greater sequence numbers, so this should not cause a problem. Also, signaled
                // sequence numbers can only decrease, never increase.
                start = observer.getSequence();
              }
            }
          }

          // Now wait the remainder if we triggered before the timer was up.
          delta = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - last);
          if (delta < interMilli)
          {
            // Sleep the remainder
            this_thread::sleep_for(interMilli - delta);
          }
        }
      }
    }
    catch (ParameterError &aError)
    {
      g_logger << LINFO << "Caught a parameter error.";
      if (out.good())
      {
        ostringstream str;
        string content = printError(printer, aError.m_code, aError.m_message);
        str << "--" << boundary
            << "\r\n"
               "Content-type: "
            << printer->mimeType()
            << "\r\n"
               "Content-length: "
            << content.length() << "\r\n\r\n"
            << content;

        string chunk = str.str();
        out.setf(ios::hex, ios::basefield);
        out << chunk.length() << "\r\n";
        out << chunk << "\r\n";
        out.flush();
      }
    }
    catch (...)
    {
      g_logger << LWARN << "Error occurred during streaming data";
      if (out.good())
      {
        ostringstream str;
        string content =
            printError(printer, "INTERNAL_ERROR", "Unknown error occurred during streaming");
        str << "--" << boundary
            << "\r\n"
               "Content-type: "
            << printer->mimeType()
            << "\r\n"
               "Content-length: "
            << content.length() << "\r\n\r\n"
            << content;

        string chunk = str.str();
        out.setf(ios::hex, ios::basefield);
        out << chunk.length() << "\r\n";
        out << chunk;
        out.flush();
      }
    }

    out.setstate(ios::badbit);
    // Observer is auto removed from signalers
  }

  string Agent::fetchCurrentData(const Printer *printer, std::set<string> &filterSet, uint64_t at)
  {
    ObservationPtrArray events;
    uint64_t firstSeq, seq;
    {
      std::lock_guard<CircularBuffer> lock(m_circularBuffer);
      firstSeq = getFirstSequence();
      seq = m_circularBuffer.getSequence();
      if (at == NO_START)
        m_circularBuffer.getLatest().getObservations(events, &filterSet);
      else
      {
        auto check = m_circularBuffer.getCheckpointAt(at, filterSet);
        check->getObservations(events);
      }
    }

    return printer->printSample(m_instanceId, m_circularBuffer.getBufferSize(),
                                seq, firstSeq, m_circularBuffer.getSequence() - 1,
                                events);
  }

  string Agent::fetchSampleData(const Printer *printer, std::set<string> &filterSet, uint64_t start,
                                int count, uint64_t &end, bool &endOfBuffer,
                                ChangeObserver *observer)
  {
    uint64_t firstSeq;
    auto results = m_circularBuffer.getObservations(filterSet, start, count,
                                                    firstSeq, end, endOfBuffer);

    if (observer)
      observer->reset();

    return printer->printSample(m_instanceId, m_circularBuffer.getBufferSize(),
                                end, firstSeq, m_circularBuffer.getSequence() - 1,
                                *results);
  }

  string Agent::printError(const Printer *printer, const string &errorCode, const string &text)
  {
    g_logger << LDEBUG << "Returning error " << errorCode << ": " << text;
    return printer->printError(m_instanceId, m_circularBuffer.getBufferSize(),
                               m_circularBuffer.getSequence(), errorCode, text);
  }

  string Agent::devicesAndPath(const string &path, const string &device)
  {
    string dataPath;

    if (!device.empty())
    {
      string prefix;
      if (device == "Agent")
        prefix = "//Devices/Agent";
      else
        prefix = "//Devices/Device[@name=\"" + device + "\"or@uuid=\"" + device + "\"]";


      if (!path.empty())
      {
        istringstream toParse(path);
        string token;

        // Prefix path (i.e. "path1|path2" => "{prefix}path1|{prefix}path2")
        while (getline(toParse, token, '|'))
          dataPath += prefix + token + "|";

        dataPath.erase(dataPath.length() - 1);
      }
      else
        dataPath = prefix;
    }
    else
      dataPath = (!path.empty()) ? path : "//Devices/Device";

    return dataPath;
  }

  int Agent::checkAndGetParam(const key_value_map &queries, const string &param,
                              const int defaultValue, const int minValue, bool minError,
                              const int maxValue, bool positive)
  {
    if (!queries.count(param))
      return defaultValue;

    const auto &v = queries[param];

    if (v.empty())
      throw ParameterError("QUERY_ERROR", "'" + param + "' cannot be empty.");

    if (positive && !isNonNegativeInteger(v))
      throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be a positive integer.");

    if (!positive && !isInteger(v))
      throw ParameterError("OUT_OF_RANGE", "'" + param + "' must an integer.");

    int value = stringToInt(v.c_str(), maxValue + 1);

    if (minValue != NO_VALUE32 && value < minValue)
    {
      if (minError)
        throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be greater than or equal to " +
                                                 int32ToString(minValue) + ".");

      return minValue;
    }

    if (maxValue != NO_VALUE32 && value > maxValue)
      throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be less than or equal to " +
                                               int32ToString(maxValue) + ".");

    return value;
  }

  uint64_t Agent::checkAndGetParam64(const key_value_map &queries, const string &param,
                                     const uint64_t defaultValue, const uint64_t minValue,
                                     bool minError, const uint64_t maxValue)
  {
    if (!queries.count(param))
      return defaultValue;

    if (queries[param].empty())
      throw ParameterError("QUERY_ERROR", "'" + param + "' cannot be empty.");

    if (!isNonNegativeInteger(queries[param]))
    {
      throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be a positive integer.");
    }

    uint64_t value = strtoull(queries[param].c_str(), nullptr, 10);

    if (minValue != NO_VALUE64 && value < minValue)
    {
      if (minError)
        throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be greater than or equal to " +
                                                 intToString(minValue) + ".");

      return minValue;
    }

    if (maxValue != NO_VALUE64 && value > maxValue)
      throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be less than or equal to " +
                                               int64ToString(maxValue) + ".");

    return value;
  }

  DataItem *Agent::getDataItemByName(const string &deviceName, const string &dataItemName)
  {
    auto dev = getDeviceByName(deviceName);
    return (dev) ? dev->getDeviceDataItem(dataItemName) : nullptr;
  }
#endif
}  // namespace mtconnect
