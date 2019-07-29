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
#include "dlib/logger.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <dlib/tokenizer.h>
#include <dlib/misc_api.h>
#include <dlib/dir_nav.h>
#include <dlib/config_reader.h>
#include <dlib/queue.h>
#include <functional>
#include <algorithm>

#include "xml_printer.hpp"
#include "json_printer.hpp"

using namespace std;

namespace mtconnect {
  static const string g_unavailable("UNAVAILABLE");
  static const string g_conditionUnavailable("UNAVAILABLE|||");
  
  static const string g_available("AVAILABLE");
  static dlib::logger g_logger("agent");
  
  // Agent public methods
  Agent::Agent(
               const string& configXmlPath,
               int bufferSize,
               int maxAssets,
               const std::string &version,
               std::chrono::milliseconds checkpointFreq,
               bool pretty) :
  m_putEnabled(false),
  m_logStreamData(false), m_pretty(pretty)
  {
    m_mimeTypes["xsl"] = "text/xsl";
    m_mimeTypes["xml"] = "text/xml";
    m_mimeTypes["json"] = "application/json";
    m_mimeTypes["css"] = "text/css";
    m_mimeTypes["xsd"] = "text/xml";
    m_mimeTypes["jpg"] = "image/jpeg";
    m_mimeTypes["jpeg"] = "image/jpeg";
    m_mimeTypes["png"] = "image/png";
    m_mimeTypes["ico"] = "image/x-icon";

    // Create the XmlPrinter
    XmlPrinter *xmlPrinter = new XmlPrinter(version, m_pretty);
    m_printers["xml"].reset(xmlPrinter);

    JsonPrinter *jsonPrinter = new JsonPrinter(version, m_pretty);
    m_printers["json"].reset(jsonPrinter);

    try
    {
      // Load the configuration for the Agent
      m_xmlParser = make_unique<XmlParser>();
      m_devices = m_xmlParser->parseFile(configXmlPath, xmlPrinter);
      std::set<std::string> uuids;
      for (const auto &device : m_devices)
      {
        if (uuids.count(device->getUuid()) > 0)
          throw runtime_error("Duplicate UUID: " + device->getUuid());
        
        uuids.insert(device->getUuid());
        device->resolveReferences();
      }
    }
    catch (runtime_error & e)
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
    
    // Grab data from configuration
    string time = getCurrentTime(GMT_UV_SEC);
    
    // Unique id number for agent instance
    m_instanceId = getCurrentTimeInSec();
    
    // Sequence number and sliding buffer for data
    m_sequence = 1ull;
    m_slidingBufferSize = 1 << bufferSize;
    m_slidingBuffer = make_unique<sliding_buffer_kernel_1<ObservationPtr>>();
    m_slidingBuffer->set_size(bufferSize);
    m_checkpointFreq = checkpointFreq.count();
    m_checkpointCount = (m_slidingBufferSize / checkpointFreq.count()) + 1;
    
    // Asset sliding buffer
    m_maxAssets = maxAssets;
    
    // Create the checkpoints at a regular frequency
    m_checkpoints.reserve(m_checkpointCount);
    for(auto i = 0; i < m_checkpointCount; i++)
      m_checkpoints.emplace_back();
    
    // Add the devices to the device map and create availability and
    // asset changed events if they don't exist
    for (const auto device  : m_devices)
    {
      m_deviceNameMap[device->getName()] = device;
      m_deviceUuidMap[device->getUuid()] = device;
      
      // Make sure we have two device level data items:
      // 1. Availability
      // 2. AssetChanged
      if ( !device->getAvailability() )
      {
        // Create availability data item and add it to the device.
        std::map<string, string> attrs;
        attrs["type"] = "AVAILABILITY";
        attrs["id"] = device->getId() + "_avail";
        attrs["category"] = "EVENT";
        
        auto di = new DataItem(attrs);
        di->setComponent(*device);
        device->addDataItem(*di);
        device->addDeviceDataItem(*di);
        device->m_availabilityAdded = true;
      }
      
      int major, minor;
      char c;
      stringstream ss(xmlPrinter->getSchemaVersion());
      ss >> major >> c >> minor;
      if ( !device->getAssetChanged() &&
          (major > 1 || (major == 1 && minor >= 2)) )
      {
        // Create asset change data item and add it to the device.
        std::map<string,string> attrs;
        attrs["type"] = "ASSET_CHANGED";
        attrs["id"] = device->getId() + "_asset_chg";
        attrs["category"] = "EVENT";
        auto di = new DataItem(attrs);
        di->setComponent(*device);
        device->addDataItem(*di);
        device->addDeviceDataItem(*di);
      }
      
      if ( !device->getAssetRemoved() &&
          (major > 1 || (major == 1 && minor >= 3)) )
      {
        // Create asset removed data item and add it to the device.
        std::map<string, string> attrs;
        attrs["type"] = "ASSET_REMOVED";
        attrs["id"] = device->getId() + "_asset_rem";
        attrs["category"] = "EVENT";
        
        auto di = new DataItem(attrs);
        di->setComponent(*device);
        device->addDataItem(*di);
        device->addDeviceDataItem(*di);
      }
    }
    
    // Reload the document for path resolution
    m_xmlParser->loadDocument(
                              xmlPrinter->printProbe(
                                                     m_instanceId,
                                                     m_slidingBufferSize,
                                                     m_maxAssets,
                                                     m_assets.size(),
                                                     m_sequence,
                                                     m_devices) );
    
    // Initialize the id mapping for the devices and set all data items to UNAVAILABLE
    for (const auto device : m_devices)
    {
      const auto &items = device->getDeviceDataItems();
      
      for (const auto item : items)
      {
        // Check for single valued constrained data items.
        auto d = item.second;
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
          g_logger << LFATAL << "Duplicate DataItem id " << d->getId() <<
          " for device: " << device->getName() << " and data item name: " <<
          d->getName();
          std::exit(1);
        }
      }
    }
  }
  
  
  const Device * Agent::getDeviceByName(const std::string& name) const
  {
    auto devPos = m_deviceNameMap.find(name);
    if(devPos != m_deviceNameMap.end())
      return devPos->second;
    
    return nullptr;
  }
  
  
  Device * Agent::getDeviceByName(const std::string& name)
  {
    auto devPos = m_deviceNameMap.find(name);
    if(devPos != m_deviceNameMap.end())
      return devPos->second;
    
    return nullptr;
  }
  
  
  Device *Agent::findDeviceByUUIDorName(const std::string& idOrName)
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
    m_slidingBuffer.reset();
    m_xmlParser.reset();
    m_checkpoints.clear();
    
    for (auto &i : m_devices) delete i;
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
      
      XmlPrinter *xmlPrinter = dynamic_cast<XmlPrinter*>(m_printers["xml"].get());
      
      if (*baseUri.rbegin() != '/')
        baseUri.append(1, '/');
      
      while (files.move_next())
      {
        file &file = files.element();
        string name = file.name();
        string uri = baseUri + name;
        m_fileMap.insert(pair<string, string>(uri, file.full_name()));
        
        // Check if the file name maps to a standard MTConnect schema file.
        if (!name.find("MTConnect") &&
            name.substr(name.length() - 4u, 4u) == ".xsd" &&
            xmlPrinter->getSchemaVersion() == name.substr(name.length() - 7u, 3u) )
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
      g_logger << LDEBUG << "registerFile: Path " << aPath << " is not a directory: "
      << e.what() << ", trying as a file";
      
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
  
  void Agent::on_connect (
                          std::istream& in,
                          std::ostream& out,
                          const std::string& foreign_ip,
                          const std::string& local_ip,
                          unsigned short foreign_port,
                          unsigned short local_port,
                          uint64
                          )
  {
    try
    {
      IncomingThings incoming(foreign_ip, local_ip, foreign_port, local_port);
      OutgoingThings outgoing;
      
      parse_http_request(in, incoming, get_max_content_length());
      read_body(in, incoming);
      outgoing.m_out = &out;
      const std::string& result = httpRequest(incoming, outgoing);
      if (out.good()) {
        write_http_response(out, outgoing, result);
      }
    }
    catch (dlib::http_parse_error& e)
    {
      g_logger << LERROR << "Error processing request from: " << foreign_ip << " - " << e.what();
      write_http_response(out, e);
    }
    catch (std::exception& e)
    {
      g_logger << LERROR << "Error processing request from: " << foreign_ip << " - " << e.what();
      write_http_response(out, e);
    }
  }
    
  const Printer *Agent::printerForAccepts(const std::string &accepts) const
  {
    stringstream list(accepts);
    string accept;
    while (getline(list, accept, ',')) {
      for (const auto &p : m_printers) {
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
    if (accepts != incoming.headers.end()) {
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
      g_logger << LDEBUG << "Request: " << incoming.request_type << " " <<
      incoming.path << " from " << incoming.foreign_ip << ":" << incoming.foreign_port;
      
      if (m_putEnabled)
      {
        if ((incoming.request_type == "PUT" || incoming.request_type == "POST") &&
            !m_putAllowedHosts.empty() &&
            !m_putAllowedHosts.count(incoming.foreign_ip))
        {
          return printError(printer, "UNSUPPORTED",
                            "HTTP PUT is not allowed from " + incoming.foreign_ip);
        }
        
        if (incoming.request_type != "GET" &&
            incoming.request_type != "PUT" &&
            incoming.request_type != "POST")
        {
          return printError(printer, "UNSUPPORTED",
                            "Only the HTTP GET and PUT requests are supported");
        }
      }
      else
      {
        if (incoming.request_type != "GET")
        {
          return printError(printer, "UNSUPPORTED",
                            "Only the HTTP GET request is supported");
        }
      }
      
      // Parse the URL path looking for '/'
      string path = incoming.path;
      auto qm = path.find_last_of('?');
      if (qm != string::npos)
        path = path.substr(0, qm);
      
      if (isFile(path))
        return handleFile(path, outgoing);
      
      string::size_type loc1 = path.find("/", 1);
      string::size_type end = (path[path.length() - 1] == '/') ?
      path.length() - 1 : string::npos;
      
      string first =  path.substr(1, loc1 - 1);
      string call, device;
      
      if (first == "assets" || first == "asset")
      {
        string list;
        if (loc1 != string::npos)
          list = path.substr(loc1 + 1);
        
        if (incoming.request_type == "GET")
          result = handleAssets(printer, *outgoing.m_out, incoming.queries, list);
        else
          result = storeAsset(*outgoing.m_out, incoming.queries, list, incoming.body);
      }
      else
      {
        // If a '/' was found
        if (loc1 < end)
        {
          // Look for another '/'
          string::size_type loc2 = path.find("/", loc1 + 1);
          
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
      printError(printer, "SERVER_EXCEPTION", (string) e.what());
    }
    
    return result;
  }
  
  
  Adapter *Agent::addAdapter(
                             const string& deviceName,
                             const string& host,
                             const unsigned int port,
                             bool start,
                             std::chrono::seconds legacyTimeout )
  {
    auto adapter = new Adapter(deviceName, host, port, legacyTimeout);
    adapter->setAgent(*this);
    m_adapters.push_back(adapter);
    
    const auto dev = getDeviceByName(deviceName);
    if (dev && dev->m_availabilityAdded)
      adapter->setAutoAvailable(true);
    
    if (start)
      adapter->start();
    
    return adapter;
  }
  
  
  unsigned int Agent::addToBuffer(
                                  DataItem *dataItem,
                                  const string& value,
                                  string time )
  {
    if (!dataItem)
      return 0;
    
    std::lock_guard<std::mutex> lock(m_sequenceLock);
    
    auto seqNum = m_sequence;
    auto event = new Observation(*dataItem, seqNum, time, value);
    
    if (!dataItem->allowDups() &&
        dataItem->isDataSet() &&
        !m_latest.dataSetDifference(event))
    {
      event->unrefer();
      return 0;
    }
    else
    {
      m_sequence++;
    }
    
    (*m_slidingBuffer)[seqNum] = event;
    m_latest.addObservation(event);
    event->unrefer();
    
    // Special case for the first event in the series to prime the first checkpoint.
    if (seqNum == 1)
      m_first.addObservation(event);
    
    // Checkpoint management
    int index = m_slidingBuffer->get_element_id(seqNum);
    if (m_checkpointCount > 0 && !(index % m_checkpointFreq))
    {
      // Copy the checkpoint from the current into the slot
      m_checkpoints[index / m_checkpointFreq].copy(m_latest);
    }
    
    
    // See if the next sequence has an event. If the event exists it
    // should be added to the first checkpoint.
    if ((*m_slidingBuffer)[m_sequence])
    {
      // Keep the last checkpoint up to date with the last.
      m_first.addObservation((*m_slidingBuffer)[m_sequence]);
    }
    
    dataItem->signalObservers(seqNum);
    
    return seqNum;
  }
  
  
  bool Agent::addAsset(
                       Device *device,
                       const string &id,
                       const string &asset,
                       const string &type,
                       const string &inputTime)
  {
    // Check to make sure the values are present
    if (type.empty() || asset.empty() || id.empty())
    {
      g_logger << LWARN << "Asset '" << id << "' missing required type, id, or body. Asset is rejected.";
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
        m_assets.push_back(&newPtr);
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
  
  
  bool Agent::updateAsset(
                          Device *device,
                          const std::string &id,
                          AssetChangeList &assetChangeList,
                          const string &inputTime )
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
      
      if (asset->getType() != "CuttingTool" &&
          asset->getType() != "CuttingToolArchitype")
        return false;
      
      CuttingToolPtr tool((CuttingTool*) asset.getObject());
      
      try
      {
        for (const auto & assetChange : assetChangeList)
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
      m_assets.push_back(&asset);
      
      tool->setTimestamp(time);
      tool->setDeviceUuid(device->getUuid());
      tool->changed();
    }
    
    addToBuffer(device->getAssetChanged(), asset->getType() + "|" + id, time);
    
    return true;
  }
  
  
  bool Agent::removeAsset(
                          Device *device,
                          const std::string &id,
                          const string &inputTime )
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
      auto ptr = m_latest.getEventPtr(device->getAssetChanged()->getId());
      if (ptr && (*ptr)->getValue() == id)
        addToBuffer(device->getAssetChanged(), asset->getType() + "|UNAVAILABLE", time);
    }
    
    addToBuffer(device->getAssetRemoved(), asset->getType() + "|" + id, time);
    
    return true;
  }
  
  
  bool Agent::removeAllAssets(
                              Device *device,
                              const std::string &type,
                              const std::string &inputTime )
  {
    string time;
    if (inputTime.empty())
      time = getCurrentTime(GMT_UV_SEC);
    else
      time = inputTime;
    
    {
      std::lock_guard<std::mutex> lock(m_assetLock);
      
      auto ptr = m_latest.getEventPtr(device->getAssetChanged()->getId());
      string changedId;
      if (ptr)
        changedId = (*ptr)->getValue();
      
      list<AssetPtr*>::reverse_iterator iter;
      for (iter = m_assets.rbegin(); iter != m_assets.rend(); ++iter)
      {
        AssetPtr asset = (**iter);
        if (type == asset->getType() &&
            !asset->isRemoved() )
        {
          asset->setRemoved(true);
          asset->setTimestamp(time);
          
          addToBuffer(device->getAssetRemoved(), asset->getType() + "|" + asset->getAssetId(), time);
          
          if (changedId == asset->getAssetId())
            addToBuffer(device->getAssetChanged(), asset->getType() + "|UNAVAILABLE", time);
        }
      }
    }
    
    return true;
  }
  
  
  // Add values for related data items UNAVAILABLE
  void Agent::disconnected(Adapter *adapter, std::vector<Device*> devices)
  {
    auto time = getCurrentTime(GMT_UV_SEC);
    g_logger << LDEBUG << "Disconnected from adapter, setting all values to UNAVAILABLE";
    
    for (const auto device : devices)
    {
      const auto &dataItems = device->getDeviceDataItems();
      for (const auto & dataItemAssoc : dataItems)
      {
        auto dataItem = dataItemAssoc.second;
        if (dataItem &&
            (dataItem->getDataSource() == adapter ||
             ( adapter->isAutoAvailable() &&
              !dataItem->getDataSource() &&
              dataItem->getType() == "AVAILABILITY") ) )
        {
          auto ptr = m_latest.getEventPtr(dataItem->getId());
          
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
            
            if (value &&
                !adapter->isDuplicate(dataItem, *value, NAN))
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
    if (!adapter->isAutoAvailable())
      return;
    
    auto time = getCurrentTime(GMT_UV_SEC);
    for (const auto device : devices)
    {
      g_logger << LDEBUG << "Connected to adapter, setting all Availability data items to AVAILABLE";
      
      if (device->getAvailability() )
      {
        g_logger << LDEBUG << "Adding availabilty event for " << device->getAvailability()->getId();
        addToBuffer(device->getAvailability(), g_available, time);
      }
      else
        g_logger << LDEBUG << "Cannot find availability for " << device->getName();
    }
  }
  
  
  // Agent protected methods
  string Agent::handleCall(
                           const Printer *printer,
                           ostream& out,
                           const string& path,
                           const key_value_map& queries,
                           const string& call,
                           const string& device)
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
        
        int freq = checkAndGetParam(queries, "frequency", NO_FREQ, FASTEST_FREQ, false,SLOWEST_FREQ);
        // Check for 1.2 conversion to interval
        if (freq == NO_FREQ)
          freq = checkAndGetParam(queries, "interval", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);
        
        auto at = checkAndGetParam64(queries, "at", NO_START, getFirstSequence(), true, m_sequence - 1);
        auto heartbeat = std::chrono::milliseconds{checkAndGetParam(queries, "heartbeat", 10000, 10, true, 600000)};
        
        if (freq != NO_FREQ && at != NO_START)
          return printError(printer, "INVALID_REQUEST", "You cannot specify both the at and frequency arguments to a current request");
        
        return handleStream(
                            printer,
                            out,
                            devicesAndPath(path, deviceName),
                            true,
                            freq,
                            at,
                            0,
                            heartbeat);
      }
      else if (call == "probe" || call.empty())
        return handleProbe(printer, deviceName);
      else if (call == "sample")
      {
        string path = queries[(string) "path"];
        string result;
        
        auto count = checkAndGetParam(queries, "count", DEFAULT_COUNT, 1, true, m_slidingBufferSize);
        auto freq = checkAndGetParam(queries, "frequency", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);
        // Check for 1.2 conversion to interval
        if (freq == NO_FREQ)
          freq = checkAndGetParam(queries, "interval", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);
        
        auto start = checkAndGetParam64(queries, "start", NO_START, getFirstSequence(), true, m_sequence);
        
        if (start == NO_START) // If there was no data in queries
          start = checkAndGetParam64(queries, "from", 1, getFirstSequence(), true, m_sequence);
        
        auto heartbeat = std::chrono::milliseconds{checkAndGetParam(queries, "heartbeat", 10000, 10, true, 600000)};
        
        return handleStream(
                            printer,
                            out,
                            devicesAndPath(path, deviceName),
                            false,
                            freq,
                            start,
                            count,
                            heartbeat);
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
  
  
  string Agent::handlePut(
                          const Printer *printer,
                          ostream &out,
                          const string &path,
                          const key_value_map &queries,
                          const string &adapter,
                          const string &deviceName )
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
        for (const auto & kv : queries)
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
      
      for (const auto & kv : queries)
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
        deviceList.push_back(device);
    }
    else
      deviceList = m_devices;
    
    return printer->printProbe(
                                  m_instanceId,
                                  m_slidingBufferSize,
                                  m_sequence,
                                  m_maxAssets,
                                  m_assets.size(),
                                  deviceList,
                                  &m_assetCounts);
  }
  
  
  string Agent::handleStream(
                             const Printer *printer,
                             ostream &out,
                             const string &path,
                             bool current,
                             unsigned int frequency,
                             uint64_t start,
                             unsigned int count,
                             std::chrono::milliseconds heartbeat )
  {
    std::set<string> filter;
    try
    {
      m_xmlParser->getDataItems(filter, path);
    }
    catch (exception& e)
    {
      return printError(printer, "INVALID_XPATH", e.what());
    }
    
    if (filter.empty())
      return printError(printer, "INVALID_XPATH", "The path could not be parsed. Invalid syntax: " + path);
    
    // Check if there is a frequency to stream data or not
    if (frequency != (unsigned)NO_FREQ )
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
  
  
  std::string Agent::handleAssets(
                                  const Printer *printer,
                                  std::ostream& aOut,
                                  const key_value_map& queries,
                                  const std::string& list )
  {
    using namespace dlib;
    std::vector<AssetPtr> assets;
    
    if (!list.empty())
    {
      std::lock_guard<std::mutex> lock(m_assetLock);
      istringstream str(list);
      tokenizer_kernel_1 tok;
      tok.set_stream(str);
      tok.set_identifier_token(tok.lowercase_letters() + tok.uppercase_letters() +
                               tok.numbers() + "_.@$%&^:+-_=",
                               tok.lowercase_letters() + tok.uppercase_letters() +
                               tok.numbers() + "_.@$%&^:+-_=");
      
      int type;
      string token;
      for (tok.get_token(type, token); type != tok.END_OF_FILE; tok.get_token(type, token))
      {
        if (type == tok.IDENTIFIER)
        {
          AssetPtr ptr = m_assetMap[token];
          if (!ptr.getObject())
            return printer->printError(m_instanceId, 0, 0, "ASSET_NOT_FOUND", (string)"Could not find asset: " + token);
          assets.push_back(ptr);
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
      
      std::list<AssetPtr*>::reverse_iterator iter;
      for (iter = m_assets.rbegin(); iter != m_assets.rend() && count > 0; ++iter, --count)
      {
        if ( (type.empty() || type == (**iter)->getType()) &&
            (removed || !(**iter)->isRemoved()) )
        {
          assets.push_back(**iter);
        }
      }
    }
    
    return printer->printAssets(m_instanceId, m_maxAssets, m_assets.size(), assets);
  }
  
  
  // Store an asset in the map by asset # and use the circular buffer as
  // an LRU. Check if we're removing an existing asset and clean up the
  // map, and then store this asset.
  std::string Agent::storeAsset(
                                std::ostream& aOut,
                                const key_value_map& queries,
                                const std::string& id,
                                const std::string& body )
  {
    string name = queries["device"];
    string type = queries["type"];
    Device *device = nullptr;
    
    if (!name.empty())
      device = getDeviceByName(name);
    
    // If the device was not found or was not provided, use the default device.
    if (!device)
      device = m_devices[0];
    
    if (addAsset(device, id, body, type))
      return "<success/>";
    else
      return "<failure/>";
  }
  
  
  string Agent::handleFile(const string &uri, OutgoingThings& outgoing)
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
        m_fileCache.insert(pair<string, RefCountedPtr<CachedFile> >(uri, cachedFile));
      
    }
    
    (*outgoing.m_out) << "HTTP/1.1 200 OK\r\n"
    "Date: " << getCurrentTime(HUM_READ) << "\r\n"
    "Server: MTConnectAgent\r\n"
    "Connection: close\r\n"
    "Content-Length: " << cachedFile->m_size << "\r\n"
    "Expires: " << getCurrentTime(std::chrono::system_clock::now() + std::chrono::seconds(60*60*24), HUM_READ) << "\r\n"
    "Content-Type: " << contentType << "\r\n\r\n";
    
    outgoing.m_out->write(cachedFile->m_buffer.get(), cachedFile->m_size);
    outgoing.m_out->setstate(ios::badbit);
    
    return "";
  }
  
  
  void Agent::streamData(
                         const Printer *printer,
                         ostream& out,
                         std::set<string> &filterSet,
                         bool current,
                         unsigned int interval,
                         uint64_t start,
                         unsigned int count,
                         std::chrono::milliseconds heartbeat )
  {
    // Create header
    string boundary = md5(intToString(time(nullptr)));
    
    ofstream log;
    if (m_logStreamData)
    {
      string filename = "Stream_" + getCurrentTime(LOCAL) + "_" +
      int64ToString((uint64_t) dlib::get_thread_id()) + ".log";
      log.open(filename.c_str());
    }
    
    out << "HTTP/1.1 200 OK\r\n"
    "Date: " << getCurrentTime(HUM_READ) << "\r\n"
    "Server: MTConnectAgent\r\n"
    "Expires: -1\r\n"
    "Connection: close\r\n"
    "Cache-Control: private, max-age=0\r\n"
    "Content-Type: multipart/x-mixed-replace;boundary=" << boundary << "\r\n"
    "Transfer-Encoding: chunked\r\n\r\n";
    
    // This object will automatically clean up all the observer from the
    // signalers in an exception proof manor.
    ChangeObserver observer;
    
    // Add observers
    for (const auto &item : filterSet)
      m_dataItemMap[item]->addObserver(&observer);
    
    chrono::milliseconds interMilli{interval};
    uint64_t firstSeq = getFirstSequence();
    if (start < firstSeq)
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
            throw ParameterError("OUT_OF_RANGE", "Client can't keep up with event stream, disconnecting");
          }
          else
          {
            // end and endOfBuffer are set during the fetch sample data while the
            // mutex is held. This removed the race to check if we are at the end of
            // the bufffer and setting the next start to the last sequence number
            // sent.
            content = fetchSampleData(printer, filterSet, start, count, end, endOfBuffer, &observer);
          }
          
          if (m_logStreamData)
            log << content << endl;
        }
        
        ostringstream str;
        
        // Make sure we're terminated with a <cr><nl>
        content.append("\r\n");
        out.setf(ios::dec, ios::basefield);
        str << "--" << boundary << "\r\n"
          "Content-type: " << printer->mimeType() << "\r\n"
          "Content-length: " << content.length() << "\r\n\r\n"
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
            while ( delta < heartbeat &&
                   observer.wait((heartbeat - delta).count()) &&
                   !observer.wasSignaled() )
            {
              delta = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - last);
            }
            
            {
              std::lock_guard<std::mutex> lock(m_sequenceLock);
              
              // Make sure the observer was signaled!
              if (!observer.wasSignaled())
              {
                // If nothing came out during the last wait, we may have still have advanced
                // the sequence number. We should reset the start to something closer to the
                // current sequence. If we lock the sequence lock, we can check if the observer
                // was signaled between the time the wait timed out and the mutex was locked.
                // Otherwise, nothing has arrived and we set to the next sequence number to
                // the next sequence number to be allocated and continue.
                start = m_sequence;
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
        str << "--" << boundary << "\r\n"
        "Content-type: " << printer->mimeType() << "\r\n"
        "Content-length: " << content.length() << "\r\n\r\n"
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
        string content = printError(printer, "INTERNAL_ERROR", "Unknown error occurred during streaming");
        str << "--" << boundary << "\r\n"
        "Content-type: " << printer->mimeType() << "\r\n"
        "Content-length: " << content.length() << "\r\n\r\n"
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
      std::lock_guard<std::mutex> lock(m_sequenceLock);
      firstSeq = getFirstSequence();
      seq = m_sequence;
      if (at == NO_START)
        m_latest.getObservations(events, &filterSet);
      else
      {
        long pos = (long) m_slidingBuffer->get_element_id(at);
        long first = (long) m_slidingBuffer->get_element_id(firstSeq);
        long checkIndex = pos / m_checkpointFreq;
        long closestCp = checkIndex * m_checkpointFreq;
        unsigned long index;
        
        Checkpoint *ref(nullptr);
        
        // Compute the closest checkpoint. If the checkpoint is after the
        // first checkpoint and before the next incremental checkpoint,
        // use first.
        if (first > closestCp && pos >= first)
        {
          ref = &m_first;
          // The checkpoint is inclusive of the "first" event. So we add one
          // so we don't duplicate effort.
          index = first + 1;
        }
        else
        {
          index = closestCp + 1;
          ref = &m_checkpoints[checkIndex];
        }
        
        Checkpoint check(*ref, &filterSet);
        
        // Roll forward from the checkpoint.
        for (; index <= (unsigned long) pos; index++)
          check.addObservation(((*m_slidingBuffer)[(unsigned long)index]).getObject());
        
        check.getObservations(events);
      }
    }
    
    return printer->printSample(
                                m_instanceId,
                                m_slidingBufferSize,
                                seq, firstSeq,
                                m_sequence - 1,
                                events);
  }
  
  
  string Agent::fetchSampleData(
                                const Printer *printer,
                                std::set<string> &filterSet,
                                uint64_t start,
                                unsigned int count,
                                uint64_t &end,
                                bool &endOfBuffer,
                                ChangeObserver *observer)
  {
    ObservationPtrArray results;
    uint64_t firstSeq;
    {
      std::lock_guard<std::mutex> lock(m_sequenceLock);
      
      firstSeq = (m_sequence > m_slidingBufferSize) ?
      m_sequence - m_slidingBufferSize : 1;
      
      // START SHOULD BE BETWEEN 0 AND SEQUENCE NUMBER
      start = (start <= firstSeq) ? firstSeq : start;
      
      uint64_t i;
      for (i = start; results.size() < count && i < m_sequence; i++)
      {
        // Filter out according to if it exists in the list
        const string &dataId = (*m_slidingBuffer)[i]->getDataItem()->getId();
        if (filterSet.count(dataId) > 0)
        {
          ObservationPtr event = (*m_slidingBuffer)[i];
          results.push_back(event);
        }
      }
      
      end = i;
      if (i >= m_sequence)
        endOfBuffer = true;
      else
        endOfBuffer = false;
      
      if (observer)
        observer->reset();
    }
    
    return printer->printSample(
                                   m_instanceId,
                                   m_slidingBufferSize,
                                   end,
                                   firstSeq,
                                   m_sequence - 1,
                                   results);
  }
  
  
  string Agent::printError(const Printer *printer, const string &errorCode, const string &text)
  {
    g_logger << LDEBUG << "Returning error " << errorCode << ": " << text;
    return printer->printError(
                                  m_instanceId,
                                  m_slidingBufferSize,
                                  m_sequence,
                                  errorCode,
                                  text);
  }
  
  
  string Agent::devicesAndPath(const string &path, const string &device)
  {
    string dataPath = "";
    
    if (!device.empty())
    {
      string prefix = "//Devices/Device[@name=\"" + device +
      "\"or@uuid=\"" + device + "\"]";
      
      if (!path.empty())
      {
        istringstream toParse(path);
        string token;
        
        // Prefix path (i.e. "path1|path2" => "{prefix}path1|{prefix}path2")
        while (getline(toParse, token, '|'))
          dataPath += prefix + token + "|";
        
        dataPath.erase(dataPath.length()-1);
      }
      else
        dataPath = prefix;
    }
    else
      dataPath = (!path.empty()) ? path : "//Devices/Device";
    
    return dataPath;
  }
  
  
  int Agent::checkAndGetParam(
                              const key_value_map &queries,
                              const string &param,
                              const int defaultValue,
                              const int minValue,
                              bool minError,
                              const int maxValue)
  {
    if (!queries.count(param))
      return defaultValue;
    
    if (queries[param].empty())
      throw ParameterError("QUERY_ERROR", "'" + param + "' cannot be empty.");
    
    if (!isNonNegativeInteger(queries[param]))
      throw ParameterError("OUT_OF_RANGE","'" + param + "' must be a positive integer.");
    
    long int value = strtol(queries[param].c_str(), nullptr, 10);
    
    if (minValue != NO_VALUE32 && value < minValue)
    {
      if (minError)
        throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be greater than or equal to " + intToString(minValue) + ".");
      
      return minValue;
    }
    
    if (maxValue != NO_VALUE32 && value > maxValue)
      throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be less than or equal to " + intToString(maxValue) + ".");
    
    return value;
  }
  
  
  uint64_t Agent::checkAndGetParam64(
                                     const key_value_map &queries,
                                     const string &param,
                                     const uint64_t defaultValue,
                                     const uint64_t minValue,
                                     bool minError,
                                     const uint64_t maxValue )
  {
    if (!queries.count(param))
      return defaultValue;
    
    if (queries[param].empty())
      throw ParameterError("QUERY_ERROR", "'" + param + "' cannot be empty.");
    
    if (!isNonNegativeInteger(queries[param]))
    {
      throw ParameterError("OUT_OF_RANGE",
                           "'" + param + "' must be a positive integer.");
    }
    
    uint64_t value = strtoull(queries[param].c_str(), nullptr, 10);
    
    if (minValue != NO_VALUE64 && value < minValue)
    {
      if (minError)
        throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be greater than or equal to " + intToString(minValue) + ".");
      
      return minValue;
    }
    
    if (maxValue != NO_VALUE64 && value > maxValue)
      throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be less than or equal to " + int64ToString(maxValue) + ".");
    
    return value;
  }
  
  
  DataItem * Agent::getDataItemByName(const string &deviceName, const string &dataItemName)
  {
    auto dev = getDeviceByName(deviceName);
    return (dev) ? dev->getDeviceDataItem(dataItemName) : nullptr;
  }
}
