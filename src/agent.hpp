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

#pragma once

#include "adapter.hpp"
#include "asset.hpp"
#include "checkpoint.hpp"
#include "service.hpp"
#include "xml_parser.hpp"
#include "cached_file.hpp"
#include "circular_buffer.hpp"

#include <dlib/md5.h>
#include <dlib/server.h>

#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace mtconnect
{
  class Adapter;
  class Observation;
  class DataItem;
  class Device;
  class AgentDevice;

  using AssetChangeList = std::vector<std::pair<std::string, std::string>>;

  struct OutgoingThings : public dlib::outgoing_things
  {
    OutgoingThings() = default;
    std::ostream *m_out = nullptr;
    const Printer *m_printer = nullptr;
  };
  using IncomingThings = struct dlib::incoming_things;

  class Agent : public dlib::server_http
  {
    class ParameterError
    {
     public:
      ParameterError(const std::string &code, const std::string &message)
      {
        m_code = code;
        m_message = message;
      }

      ParameterError(const ParameterError &anotherError)
      {
        m_code = anotherError.m_code;
        m_message = anotherError.m_message;
      }

      ParameterError &operator=(const ParameterError &anotherError) = default;

      std::string m_code;
      std::string m_message;
    };

   public:
    // Load agent with the xml configuration
    Agent(const std::string &configXmlPath, int bufferSize, int maxAssets,
          const std::string &version, int checkpointFreq = 1000, bool pretty = false);

    // Virtual destructor
    ~Agent() override;

    // Overridden method that is called per web request – not used
    // using httpRequest which is called from our own on_connect method.
    const std::string on_request(const dlib::incoming_things &incoming,
                                 dlib::outgoing_things &outgoing) override
    {
      throw std::logic_error("Not Implemented");
      return "";
    }

    const std::string httpRequest(const IncomingThings &incoming, OutgoingThings &outgoing);

    // Add an adapter to the agent
    void addAdapter(Adapter *adapter, bool start = false);

    // Get device from device map
    Device *getDeviceByName(const std::string &name);
    const Device *getDeviceByName(const std::string &name) const;
    Device *findDeviceByUUIDorName(const std::string &idOrName);
    const std::vector<Device *> &getDevices() const { return m_devices; }

    // Add the a device from a configuration file
    void addDevice(Device *device);

    // Add component events to the sliding buffer
    unsigned int addToBuffer(DataItem *dataItem, const std::string &value, std::string time = "");

    // Asset management
    bool addAsset(Device *device, const std::string &id, const std::string &asset,
                  const std::string &type, const std::string &time = "");

    bool updateAsset(Device *device, const std::string &id, AssetChangeList &list,
                     const std::string &time);

    bool removeAsset(Device *device, const std::string &id, const std::string &time);
    bool removeAllAssets(Device *device, const std::string &type, const std::string &time);

    // Message when adapter has connected and disconnected
    void disconnected(Adapter *adapter, std::vector<Device *> devices);
    void connected(Adapter *adapter, std::vector<Device *> devices);

    DataItem *getDataItemByName(const std::string &deviceName, const std::string &dataItemName);

    Observation *getFromBuffer(uint64_t seq) const
    {
      return m_circularBuffer.getFromBuffer(seq);
    }
    uint64_t getSequence() const { return m_circularBuffer.getSequence(); }
    unsigned int getBufferSize() const { return m_circularBuffer.getBufferSize(); }
    unsigned int getMaxAssets() const { return m_maxAssets; }
    unsigned int getAssetCount() const { return m_assets.size(); }

    int getAssetCount(const std::string &type) const
    {
      const auto assetPos = m_assetCounts.find(type);
      if (assetPos != m_assetCounts.end())
        return assetPos->second;
      return 0;
    }

    uint64_t getFirstSequence() const { return m_circularBuffer.getFirstSequence(); }

    // For testing...
    void setSequence(uint64_t seq) { m_circularBuffer.setSequence(seq); }
    std::list<AssetPtr *> *getAssets() { return &m_assets; }
    auto getAgentDevice() { return m_agentDevice; }

    // Starting
    virtual void start();

    // Shutdown
    void clear();

    void registerFile(const std::string &uri, const std::string &path);
    void addMimeType(const std::string &ext, const std::string &type) { m_mimeTypes[ext] = type; }

    // PUT and POST handling
    void enablePut(bool flag = true) { m_putEnabled = flag; }
    bool isPutEnabled() const { return m_putEnabled; }
    void allowPutFrom(const std::string &host) { m_putAllowedHosts.insert(host); }
    bool isPutAllowedFrom(const std::string &host) const
    {
      return m_putAllowedHosts.find(host) != m_putAllowedHosts.end();
    }

    // For debugging
    void setLogStreamData(bool log) { m_logStreamData = log; }

    // Handle probe calls
    std::string handleProbe(const Printer *printer, const std::string &device);

    // Get the printer for a type
    Printer *getPrinter(const std::string &aType) { return m_printers[aType].get(); }

   protected:
    // Initialization methods
    void createAgentDevice();
    void loadXMLDeviceFile(const std::string &config);
    void verifyDevice(Device *device);
    void initializeDataItems(Device *device);

    // HTTP Protocol
    void on_connect(std::istream &in, std::ostream &out, const std::string &foreign_ip,
                    const std::string &local_ip, unsigned short foreign_port,
                    unsigned short local_port, dlib::uint64) override;

    // HTTP methods to handle the 3 basic calls
    std::string handleCall(const Printer *printer, std::ostream &out, const std::string &path,
                           const dlib::key_value_map &queries, const std::string &call,
                           const std::string &device);

    // HTTP methods to handle the 3 basic calls
    std::string handlePut(const Printer *printer, std::ostream &out, const std::string &path,
                          const dlib::key_value_map &queries, const std::string &call,
                          const std::string &device);

    // Handle stream calls, which includes both current and sample
    std::string handleStream(const Printer *printer, std::ostream &out, const std::string &path,
                             bool current, unsigned int frequency, uint64_t start = 0,
                             int count = 0,
                             std::chrono::milliseconds heartbeat = std::chrono::milliseconds{
                                 10000});

    // Asset related methods
    std::string handleAssets(const Printer *printer, std::ostream &out,
                             const dlib::key_value_map &queries, const std::string &list);

    std::string storeAsset(std::ostream &out, const dlib::key_value_map &queries,
                           const std::string &command, const std::string &asset,
                           const std::string &body);

    // Stream the data to the user
    void streamData(const Printer *printer, std::ostream &out, std::set<std::string> &filterSet,
                    bool current, unsigned int frequency, uint64_t start = 1,
                    unsigned int count = 0,
                    std::chrono::milliseconds heartbeat = std::chrono::milliseconds{10000});

    // Fetch the current/sample data and return the XML in a std::string
    std::string fetchCurrentData(const Printer *printer, std::set<std::string> &filterSet,
                                 uint64_t at);
    std::string fetchSampleData(const Printer *printer, std::set<std::string> &filterSet,
                                uint64_t start, int count, uint64_t &end, bool &endOfBuffer,
                                ChangeObserver *observer = nullptr);

    // Output an XML Error
    std::string printError(const Printer *printer, const std::string &errorCode,
                           const std::string &text);

    // Handle the device/path parameters for the xpath search
    std::string devicesAndPath(const std::string &path, const std::string &device);

    // Get a file
    std::string handleFile(const std::string &uri, OutgoingThings &outgoing);

    bool isFile(const std::string &uri) const { return m_fileMap.find(uri) != m_fileMap.end(); }

    // Perform a check on parameter and return a value or a code
    int checkAndGetParam(const dlib::key_value_map &queries, const std::string &param,
                         const int defaultValue, const int minValue = NO_VALUE32,
                         bool minError = false, const int maxValue = NO_VALUE32,
                         bool positive = true);

    // Perform a check on parameter and return a value or a code
    uint64_t checkAndGetParam64(const dlib::key_value_map &queries, const std::string &param,
                                const uint64_t defaultValue, const uint64_t minValue = NO_VALUE64,
                                bool minError = false, const uint64_t maxValue = NO_VALUE64);

    // Find data items by name/id
    DataItem *getDataItemById(const std::string &id) const
    {
      auto diPos = m_dataItemMap.find(id);
      if (diPos != m_dataItemMap.end())
        return diPos->second;
      return nullptr;
    }

    const Printer *printerForAccepts(const std::string &accepts) const;

   protected:
    // Unique id based on the time of creation
    uint64_t m_instanceId;
    bool m_initialized{false};

    // Pointer to the configuration file for node access
    std::unique_ptr<XmlParser> m_xmlParser;
    std::map<std::string, std::unique_ptr<Printer>> m_printers;
    
    // Circular Buffer
    CircularBuffer m_circularBuffer;

    // For access to the sequence number and sliding buffer, use the mutex
    std::mutex m_assetLock;

    // Asset storage, circ buffer stores ids
    std::list<AssetPtr *> m_assets;
    AssetIndex m_assetMap;

    // Natural key indices for assets
    std::map<std::string, AssetIndex> m_assetIndices;
    unsigned int m_maxAssets;

    // Agent Device
    AgentDevice *m_agentDevice{nullptr};

    // Data containers
    std::vector<Adapter *> m_adapters;
    std::vector<Device *> m_devices;
    std::map<std::string, Device *> m_deviceNameMap;
    std::map<std::string, Device *> m_deviceUuidMap;
    std::map<std::string, DataItem *> m_dataItemMap;
    std::map<std::string, int> m_assetCounts;

    // For file handling, small files will be cached
    std::map<std::string, std::string> m_fileMap;
    std::map<std::string, RefCountedPtr<CachedFile>> m_fileCache;
    std::map<std::string, std::string> m_mimeTypes;

    // Put handling controls
    bool m_putEnabled;
    std::set<std::string> m_putAllowedHosts;

    // For debugging
    bool m_logStreamData;
    bool m_pretty;
  };
}  // namespace mtconnect
