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
      : m_putEnabled(false),
        m_logStreamData(false),
        m_circularBuffer(bufferSize, checkpointFreq),
        m_pretty(pretty),
        m_mimeTypes({{{"xsl", "text/xsl"},
                      {"xml", "text/xml"},
                      {"json", "application/json"},
                      {"css", "text/css"},
                      {"xsd", "text/xml"},
                      {"jpg", "image/jpeg"},
                      {"jpeg", "image/jpeg"},
                      {"png", "image/png"},
                      {"ico", "image/x-icon"}}}),
        m_maxAssets(maxAssets),
        m_xmlParser(make_unique<XmlParser>())
  {
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
                                                     m_maxAssets, m_assets.size(),
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
    m_assets.clear();
  }

  void Agent::start()
  {
    try
    {
      // Start all the adapters
      for (const auto adapter : m_adapters)
        adapter->start();

      // Start the server. This blocks until the server stops.
      server_http::start();
    }
    catch (dlib::socket_error &e)
    {
      g_logger << LFATAL << "Cannot start server: " << e.what();
      std::exit(1);
    }
  }

  void Agent::clear()
  {
    // Stop all adapter threads...
    g_logger << LINFO << "Shutting down adapters";
    // Deletes adapter and waits for it to exit.
    for (const auto adapter : m_adapters)
      adapter->stop();

    g_logger << LINFO << "Shutting down server";
    server::http_1a::clear();
    g_logger << LINFO << "Shutting completed";

    for (const auto adapter : m_adapters)
      delete adapter;

    m_adapters.clear();
  }

  // Register a file
  void Agent::registerFile(const string &aUri, const string &aPath)
  {
    try
    {
      directory dir(aPath);
      queue<file>::kernel_1a files;
      dir.get_files(files);
      files.reset();
      string baseUri = aUri;

      XmlPrinter *xmlPrinter = dynamic_cast<XmlPrinter *>(m_printers["xml"].get());

      if (*baseUri.rbegin() != '/')
        baseUri.append(1, '/');

      while (files.move_next())
      {
        file &file = files.element();
        string name = file.name();
        string uri = baseUri + name;
        m_fileMap.insert(pair<string, string>(uri, file.full_name()));

        // Check if the file name maps to a standard MTConnect schema file.
        if (!name.find("MTConnect") && name.substr(name.length() - 4u, 4u) == ".xsd" &&
            xmlPrinter->getSchemaVersion() == name.substr(name.length() - 7u, 3u))
        {
          string version = name.substr(name.length() - 7u, 3u);

          if (name.substr(9u, 5u) == "Error")
          {
            string urn = "urn:mtconnect.org:MTConnectError:" + xmlPrinter->getSchemaVersion();
            xmlPrinter->addErrorNamespace(urn, uri, "m");
          }
          else if (name.substr(9u, 7u) == "Devices")
          {
            string urn = "urn:mtconnect.org:MTConnectDevices:" + xmlPrinter->getSchemaVersion();
            xmlPrinter->addDevicesNamespace(urn, uri, "m");
          }
          else if (name.substr(9u, 6u) == "Assets")
          {
            string urn = "urn:mtconnect.org:MTConnectAssets:" + xmlPrinter->getSchemaVersion();
            xmlPrinter->addAssetsNamespace(urn, uri, "m");
          }
          else if (name.substr(9u, 7u) == "Streams")
          {
            string urn = "urn:mtconnect.org:MTConnectStreams:" + xmlPrinter->getSchemaVersion();
            xmlPrinter->addStreamsNamespace(urn, uri, "m");
          }
        }
      }
    }
    catch (directory::dir_not_found e)
    {
      g_logger << LDEBUG << "registerFile: Path " << aPath << " is not a directory: " << e.what()
               << ", trying as a file";

      try
      {
        file file(aPath);
        m_fileMap.insert(pair<string, string>(aUri, aPath));
      }
      catch (file::file_not_found e)
      {
        g_logger << LERROR << "Cannot register file: " << aPath << ": " << e.what();
      }
    }
  }

  void Agent::on_connect(std::istream &in, std::ostream &out, const std::string &foreign_ip,
                         const std::string &local_ip, unsigned short foreign_port,
                         unsigned short local_port, uint64)
  {
    try
    {
      IncomingThings incoming(foreign_ip, local_ip, foreign_port, local_port);
      OutgoingThings outgoing;

      parse_http_request(in, incoming, get_max_content_length());
      read_body(in, incoming);
      outgoing.m_out = &out;
      const std::string &result = httpRequest(incoming, outgoing);
      if (out.good())
      {
        write_http_response(out, outgoing, result);
      }
    }
    catch (dlib::http_parse_error &e)
    {
      g_logger << LERROR << "Error processing request from: " << foreign_ip << " - " << e.what();
      write_http_response(out, e);
    }
    catch (std::exception &e)
    {
      g_logger << LERROR << "Error processing request from: " << foreign_ip << " - " << e.what();
      write_http_response(out, e);
    }
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
  const string Agent::httpRequest(const IncomingThings &incoming, OutgoingThings &outgoing)
  {
    string result;

    const Printer *printer = nullptr;
    auto accepts = incoming.headers.find("Accept");
    if (accepts != incoming.headers.end())
    {
      printer = printerForAccepts(accepts->second);
    }

    // If there are no specified accepts that match,
    // default to XML
    if (printer == nullptr)
      printer = getPrinter("xml");

    outgoing.m_printer = printer;
    outgoing.headers["Content-Type"] = printer->mimeType();

    try
    {
      g_logger << LDEBUG << "Request: " << incoming.request_type << " " << incoming.path << " from "
               << incoming.foreign_ip << ":" << incoming.foreign_port;

      if (m_putEnabled && incoming.request_type != "GET")
      {
        if (incoming.request_type != "PUT" && incoming.request_type != "POST" &&
            incoming.request_type != "DELETE")
        {
          return printError(printer, "UNSUPPORTED",
                            "Only the HTTP GET, PUT, POST, and DELETE requests are supported");
        }

        if (!m_putAllowedHosts.empty() && !m_putAllowedHosts.count(incoming.foreign_ip))
        {
          return printError(
              printer, "UNSUPPORTED",
              "HTTP PUT, POST, and DELETE are not allowed from " + incoming.foreign_ip);
        }
      }
      else if (incoming.request_type != "GET")
      {
        return printError(printer, "UNSUPPORTED", "Only the HTTP GET request is supported");
      }

      // Parse the URL path looking for '/'
      string path = incoming.path;
      auto qm = path.find_last_of('?');
      if (qm != string::npos)
        path = path.substr(0, qm);

      if (isFile(path))
        return handleFile(path, outgoing);

      string::size_type loc1 = path.find('/', 1);
      string::size_type end = (path[path.length() - 1] == '/') ? path.length() - 1 : string::npos;

      string first = path.substr(1, loc1 - 1);
      string call, device;

      if (first == "assets" || first == "asset")
      {
        string list;
        if (loc1 != string::npos)
          list = path.substr(loc1 + 1);

        if (incoming.request_type == "GET")
          result = handleAssets(printer, *outgoing.m_out, incoming.queries, list);
        else
          result = storeAsset(*outgoing.m_out, incoming.queries, incoming.request_type, list,
                              incoming.body);
      }
      else
      {
        // If a '/' was found
        if (loc1 < end)
        {
          // Look for another '/'
          string::size_type loc2 = path.find('/', loc1 + 1);

          if (loc2 == end)
          {
            device = first;
            call = path.substr(loc1 + 1, loc2 - loc1 - 1);
          }
          else
          {
            // Path is too long
            return printError(printer, "UNSUPPORTED", "The following path is invalid: " + path);
          }
        }
        else
        {
          // Try to handle the call
          call = first;
        }

        if (incoming.request_type == "GET")
          result = handleCall(printer, *outgoing.m_out, path, incoming.queries, call, device);
        else
          result = handlePut(printer, *outgoing.m_out, path, incoming.queries, call, device);
      }
    }
    catch (exception &e)
    {
      printError(printer, "SERVER_EXCEPTION", (string)e.what());
    }

    return result;
  }

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

  bool Agent::addAsset(Device *device, const string &id, const string &asset, const string &type,
                       const string &inputTime)
  {
    // Check to make sure the values are present
    if (type.empty() || asset.empty() || id.empty())
    {
      g_logger << LWARN << "Asset '" << id
               << "' missing required type, id, or body. Asset is rejected.";
      return false;
    }

    string time;
    if (inputTime.empty())
      time = getCurrentTime(GMT_UV_SEC);
    else
      time = inputTime;

    AssetPtr ptr;

    // Lock the asset addition to protect from multithreaded collisions. Releaes
    // before we add the event so we don't cause a race condition.
    {
      std::lock_guard<std::mutex> lock(m_assetLock);

      try
      {
        ptr = m_xmlParser->parseAsset(id, type, asset);
      }
      catch (runtime_error &e)
      {
        g_logger << LERROR << "addAsset: Error parsing asset: " << asset << "\n" << e.what();
        return false;
      }

      if (!ptr.getObject())
      {
        g_logger << LERROR << "addAssete: Error parsing asset";
        return false;
      }

      auto old = &m_assetMap[id];
      if (!ptr->isRemoved())
      {
        if (old->getObject())
          m_assets.remove(old);
        else
          m_assetCounts[type] += 1;
      }
      else if (!old->getObject())
      {
        g_logger << LWARN << "Cannot remove non-existent asset";
        return false;
      }

      if (!ptr.getObject())
      {
        g_logger << LWARN << "Asset could not be created";
        return false;
      }
      else
      {
        ptr->setAssetId(id);
        ptr->setTimestamp(time);
        ptr->setDeviceUuid(device->getUuid());
      }

      // Check for overflow
      if (m_assets.size() >= m_maxAssets)
      {
        AssetPtr oldref(*m_assets.front());
        m_assetCounts[oldref->getType()] -= 1;
        m_assets.pop_front();
        m_assetMap.erase(oldref->getAssetId());

        // Remove secondary keys
        const auto &keys = oldref->getKeys();
        for (const auto &key : keys)
        {
          auto &index = m_assetIndices[key.first];
          index.erase(key.second);
        }
      }

      m_assetMap[id] = ptr;
      if (!ptr->isRemoved())
      {
        AssetPtr &newPtr = m_assetMap[id];
        m_assets.emplace_back(&newPtr);
      }

      // Add secondary keys
      const auto &keys = ptr->getKeys();
      for (const auto &key : keys)
      {
        auto &index = m_assetIndices[key.first];
        index[key.second] = ptr;
      }
    }

    // Generate an asset changed event.
    if (ptr->isRemoved())
      addToBuffer(device->getAssetRemoved(), type + "|" + id, time);
    else
      addToBuffer(device->getAssetChanged(), type + "|" + id, time);

    return true;
  }

  bool Agent::updateAsset(Device *device, const std::string &id, AssetChangeList &assetChangeList,
                          const string &inputTime)
  {
    AssetPtr asset;
    string time;
    if (inputTime.empty())
      time = getCurrentTime(GMT_UV_SEC);
    else
      time = inputTime;

    {
      std::lock_guard<std::mutex> lock(m_assetLock);

      asset = m_assetMap[id];
      if (!asset.getObject())
        return false;

      if (asset->getType() != "CuttingTool" && asset->getType() != "CuttingToolArchitype")
        return false;

      CuttingToolPtr tool((CuttingTool *)asset.getObject());

      try
      {
        for (const auto &assetChange : assetChangeList)
        {
          if (assetChange.first == "xml")
            m_xmlParser->updateAsset(asset, asset->getType(), assetChange.second);
          else
            tool->updateValue(assetChange.first, assetChange.second);
        }
      }
      catch (runtime_error &e)
      {
        g_logger << LERROR << "updateAsset: Error parsing asset: " << asset << "\n" << e.what();
        return false;
      }

      // Move it to the front of the queue
      m_assets.remove(&asset);
      m_assets.emplace_back(&asset);

      tool->setTimestamp(time);
      tool->setDeviceUuid(device->getUuid());
      tool->changed();
    }

    addToBuffer(device->getAssetChanged(), asset->getType() + "|" + id, time);

    return true;
  }

  bool Agent::removeAsset(Device *device, const std::string &id, const string &inputTime)
  {
    AssetPtr asset;

    string time;
    if (inputTime.empty())
      time = getCurrentTime(GMT_UV_SEC);
    else
      time = inputTime;

    {
      std::lock_guard<std::mutex> lock(m_assetLock);

      asset = m_assetMap[id];
      if (!asset.getObject())
        return false;

      asset->setRemoved(true);
      asset->setTimestamp(time);

      // Check if the asset changed id is the same as this asset.
      auto ptr = m_circularBuffer.getLatest().getEventPtr(device->getAssetChanged()->getId());
      if (ptr && (*ptr)->getValue() == id)
        addToBuffer(device->getAssetChanged(), asset->getType() + "|UNAVAILABLE", time);
    }

    addToBuffer(device->getAssetRemoved(), asset->getType() + "|" + id, time);

    return true;
  }

  bool Agent::removeAllAssets(Device *device, const std::string &type, const std::string &inputTime)
  {
    string time;
    if (inputTime.empty())
      time = getCurrentTime(GMT_UV_SEC);
    else
      time = inputTime;

    {
      std::lock_guard<std::mutex> lock(m_assetLock);

      auto ptr = m_circularBuffer.getLatest().getEventPtr(device->getAssetChanged()->getId());
      string changedId;
      if (ptr)
        changedId = (*ptr)->getValue();

      list<AssetPtr *>::reverse_iterator iter;
      for (iter = m_assets.rbegin(); iter != m_assets.rend(); ++iter)
      {
        AssetPtr asset = (**iter);
        if (type == asset->getType() && !asset->isRemoved())
        {
          asset->setRemoved(true);
          asset->setTimestamp(time);

          addToBuffer(device->getAssetRemoved(), asset->getType() + "|" + asset->getAssetId(),
                      time);

          if (changedId == asset->getAssetId())
            addToBuffer(device->getAssetChanged(), asset->getType() + "|UNAVAILABLE", time);
        }
      }
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

  // Agent protected methods
  string Agent::handleCall(const Printer *printer, ostream &out, const string &path,
                           const key_value_map &queries, const string &call, const string &device)
  {
    try
    {
      string deviceName;
      if (!device.empty())
        deviceName = device;

      if (call == "current")
      {
        const string path = queries[(string) "path"];
        string result;

        int freq =
            checkAndGetParam(queries, "frequency", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);
        // Check for 1.2 conversion to interval
        if (freq == NO_FREQ)
          freq = checkAndGetParam(queries, "interval", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);

        auto at =
            checkAndGetParam64(queries, "at", NO_START, getFirstSequence(), true, m_circularBuffer.getSequence() - 1);
        auto heartbeat = std::chrono::milliseconds{
            checkAndGetParam(queries, "heartbeat", 10000, 10, true, 600000)};

        if (freq != NO_FREQ && at != NO_START)
          return printError(
              printer, "INVALID_REQUEST",
              "You cannot specify both the at and frequency arguments to a current request");

        return handleStream(printer, out, devicesAndPath(path, deviceName), true, freq, at, 0,
                            heartbeat);
      }
      else if (call == "probe" || call.empty())
        return handleProbe(printer, deviceName);
      else if (call == "sample")
      {
        string path = queries[(string) "path"];
        string result;

        int32_t count = checkAndGetParam(queries, "count", DEFAULT_COUNT,
                                         -m_circularBuffer.getBufferSize(),
                                         true, m_circularBuffer.getBufferSize(), false);
        if (count == 0)
          throw ParameterError("OUT_OF_RANGE", "'count' must not be 0.");

        auto freq =
            checkAndGetParam(queries, "interval", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);
        // Check for 1.2 conversion to interval
        if (freq == NO_FREQ)
          freq = checkAndGetParam(queries, "frequency", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);

        if (count < 0 && freq != NO_FREQ)
          throw ParameterError("OUT_OF_RANGE", "'count' must not be used with an 'interval'.");

        auto start =
            checkAndGetParam64(queries, "from", NO_START, getFirstSequence(), true, getSequence());

        if (start == NO_START)  // If there was no data in queries
          start =
              checkAndGetParam64(queries, "start", NO_START, getFirstSequence(), true, getSequence());

        auto heartbeat = std::chrono::milliseconds{
            checkAndGetParam(queries, "heartbeat", 10000, 10, true, 600000)};

        return handleStream(printer, out, devicesAndPath(path, deviceName), false, freq, start,
                            count, heartbeat);
      }
      else if (findDeviceByUUIDorName(call) && device.empty())
        return handleProbe(printer, call);
      else
        return printError(printer, "UNSUPPORTED", "The following path is invalid: " + path);
    }
    catch (ParameterError &aError)
    {
      return printError(printer, aError.m_code, aError.m_message);
    }
  }

  string Agent::handlePut(const Printer *printer, ostream &out, const string &path,
                          const key_value_map &queries, const string &adapter,
                          const string &deviceName)
  {
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

    return printer->printProbe(m_instanceId, m_circularBuffer.getBufferSize(), m_circularBuffer.getSequence(), m_maxAssets,
                               m_assets.size(), deviceList, &m_assetCounts);
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

  std::string Agent::handleAssets(const Printer *printer, std::ostream &aOut,
                                  const key_value_map &queries, const std::string &list)
  {
    using namespace dlib;
    std::vector<AssetPtr> assets;

    if (!list.empty())
    {
      std::lock_guard<std::mutex> lock(m_assetLock);
      istringstream str(list);
      tokenizer_kernel_1 tok;
      tok.set_stream(str);
      tok.set_identifier_token(
          tok.lowercase_letters() + tok.uppercase_letters() + tok.numbers() + "_.@$%&^:+-_=",
          tok.lowercase_letters() + tok.uppercase_letters() + tok.numbers() + "_.@$%&^:+-_=");

      int type;
      string token;
      for (tok.get_token(type, token); type != tok.END_OF_FILE; tok.get_token(type, token))
      {
        if (type == tok.IDENTIFIER)
        {
          AssetPtr ptr = m_assetMap[token];
          if (!ptr.getObject())
            return printer->printError(m_instanceId, 0, 0, "ASSET_NOT_FOUND",
                                       (string) "Could not find asset: " + token);
          assets.emplace_back(ptr);
        }
      }
    }
    else
    {
      std::lock_guard<std::mutex> lock(m_assetLock);
      // Return all asssets, first check if there is a type attribute

      string type = queries["type"];
      auto removed = (queries.count("removed") > 0 && queries["removed"] == "true");
      auto count = checkAndGetParam(queries, "count", m_assets.size(), 1, false, NO_VALUE32);

      std::list<AssetPtr *>::reverse_iterator iter;
      for (iter = m_assets.rbegin(); iter != m_assets.rend() && count > 0; ++iter, --count)
      {
        if ((type.empty() || type == (**iter)->getType()) && (removed || !(**iter)->isRemoved()))
        {
          assets.emplace_back(**iter);
        }
      }
    }

    return printer->printAssets(m_instanceId, m_maxAssets, m_assets.size(), assets);
  }

  // Store an asset in the map by asset # and use the circular buffer as
  // an LRU. Check if we're removing an existing asset and clean up the
  // map, and then store this asset.
  std::string Agent::storeAsset(std::ostream &aOut, const key_value_map &queries,
                                const std::string &command, const std::string &id,
                                const std::string &body)
  {
    string name = queries["device"];
    string type = queries["type"];
    Device *device = nullptr;

    if (!name.empty())
      device = getDeviceByName(name);

    // If the device was not found or was not provided, use the default device.
    if (!device)
      device = m_devices[0];

    if (command == "PUT" || command == "POST")
    {
      if (addAsset(device, id, body, type))
        return "<success/>";
      else
        return "<failure>Cannot add asset (" + id + ")</failure>";
    }
    else if (command == "DELETE")
    {
      if (removeAsset(device, id, ""))
        return "<success/>";
      else
        return "<failure>Cannot remove asset (" + id + ")</failure>";
    }

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
}  // namespace mtconnect
