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
#include "checkpoint.hpp"
#include "service.hpp"
#include "xml_parser.hpp"
#include "circular_buffer.hpp"
#include "asset_buffer.hpp"
#include "printer.hpp"
#include "http_server/http_server.hpp"

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

  class Agent
  {
   public:
    using RequestResult = std::pair<std::string,unsigned short>;
    
    // Load agent with the xml configuration
    Agent(const std::string &configXmlPath, int bufferSize, int maxAssets,
          const std::string &version, int checkpointFreq = 1000, bool pretty = false);

    // Virtual destructor
    ~Agent();

    // Start and stop
    void start();   
    void stop();    

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
    bool addAsset(Device *device, const std::string &id,
                  const std::string &asset,
                  const std::string &type,
                  const std::string &time,
                  entity::ErrorList &errors);
    bool removeAsset(Device *device, const std::string &id,
                     const std::string &time);
    bool removeAllAssets(Device *device, const std::string &type,
                         const std::string &time);

    // Message when adapter has connected and disconnected
    void connecting(Adapter *adapter);
    void disconnected(Adapter *adapter, std::vector<Device *> devices);
    void connected(Adapter *adapter, std::vector<Device *> devices);

    DataItem *getDataItemByName(const std::string &deviceName, const std::string &dataItemName);

    Observation *getFromBuffer(uint64_t seq) const
    {
      return m_circularBuffer.getFromBuffer(seq);
    }
    SequenceNumber_t getSequence() const { return m_circularBuffer.getSequence(); }
    unsigned int getBufferSize() const { return m_circularBuffer.getBufferSize(); }
    auto getMaxAssets() const { return m_assetBuffer.getMaxAssets(); }
    auto getAssetCount(bool active = true) const { return m_assetBuffer.getCount(active); }
    const auto &getAssets() const { return m_assetBuffer.getAssets(); }
    
    auto getAssetCount(const std::string &type, bool active = true) const
    {
      return m_assetBuffer.getCountForType(type, active);
    }

    SequenceNumber_t getFirstSequence() const { return m_circularBuffer.getFirstSequence(); }

    // For testing...
    void setSequence(uint64_t seq) { m_circularBuffer.setSequence(seq); }
    auto getAgentDevice() { return m_agentDevice; }
    
    // MTConnect Requests
    RequestResult probeRequest(std::string format,
                               std::optional<std::string> device = std::nullopt);
    RequestResult currentRequest(std::string format,
                                 std::optional<std::string> device = std::nullopt,
                                 std::optional<SequenceNumber_t> at = std::nullopt);
    RequestResult sampleRequest(std::string format,
                                std::optional<std::string> device = std::nullopt,
                                std::optional<SequenceNumber_t> from = std::nullopt,
                                std::optional<int> count = std::nullopt);
    RequestResult streamSampleRequest(std::string format,
                                      std::optional<std::string> device = std::nullopt,
                                      std::optional<SequenceNumber_t> from = std::nullopt,
                                      std::optional<int> count = std::nullopt,
                                      std::optional<int> interval = std::nullopt);
    RequestResult streamCurrentRequest(std::string format,
                                       std::optional<std::string> device = std::nullopt,
                                       std::optional<int> interval = std::nullopt);


    // For debugging
    void setLogStreamData(bool log) { m_logStreamData = log; }

    // Get the printer for a type
    Printer *getPrinter(const std::string &aType) { return m_printers[aType].get(); }

   protected:
    // Initialization methods
    void createAgentDevice();
    void loadXMLDeviceFile(const std::string &config);
    void verifyDevice(Device *device);
    void initializeDataItems(Device *device);
    void loadCachedProbe();
#if 0

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
#endif
    // Output an XML Error
    std::string printError(const Printer *printer, const std::string &errorCode,
                           const std::string &text);

    // Handle the device/path parameters for the xpath search
    std::string devicesAndPath(const std::string &path, const std::string &device);

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

    // Agent Device
    AgentDevice *m_agentDevice{nullptr};
    
    // Asset Buffer
    AssetBuffer m_assetBuffer;

    // Data containers
    std::vector<Adapter *> m_adapters;
    std::vector<Device *> m_devices;
    std::map<std::string, Device *> m_deviceNameMap;
    std::map<std::string, Device *> m_deviceUuidMap;
    std::map<std::string, DataItem *> m_dataItemMap;
    
    // For debugging
    bool m_logStreamData;
    bool m_pretty;
  };
}  // namespace mtconnect
