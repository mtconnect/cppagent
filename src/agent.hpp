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
#include "assets/asset_buffer.hpp"
#include "checkpoint.hpp"
#include "circular_buffer.hpp"
#include "http_server/server.hpp"
#include "printer.hpp"
#include "service.hpp"
#include "xml_parser.hpp"

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
    struct RequestResult
    {
      RequestResult(){};
      RequestResult(const std::string &body, const http_server::ResponseCode status,
                    const std::string &format)
        : m_body(body), m_status(status), m_format(format)
      {
      }
      RequestResult(const RequestResult &) = default;

      std::string m_body;
      http_server::ResponseCode m_status;
      std::string m_format;
    };

    // Load agent with the xml configuration
    Agent(std::unique_ptr<http_server::Server> &server,
          std::unique_ptr<http_server::FileCache> &cache, const std::string &configXmlPath,
          int bufferSize, int maxAssets, const std::string &version, int checkpointFreq = 1000,
          bool pretty = false);

    // Virtual destructor
    ~Agent();

    // Start and stop
    void start();
    void stop();

    // HTTP Server
    auto getServer() const { return m_server.get(); }

    // Add an adapter to the agent
    void addAdapter(Adapter *adapter, bool start = false);

    // Get device from device map
    Device *getDeviceByName(const std::string &name);
    Device *getDeviceByName(const std::string &name) const;
    Device *findDeviceByUUIDorName(const std::string &idOrName) const;
    const auto &getDevices() const { return m_devices; }

    // Add the a device from a configuration file
    void addDevice(Device *device);
    void deviceChanged(Device *device, const std::string &oldUuid,
                       const std::string &oldName);

    // Add component events to the sliding buffer
    unsigned int addToBuffer(DataItem *dataItem, const std::string &value, const std::string &time);

    // Asset management
    AssetPtr addAsset(Device *device, const std::string &asset,
                      const std::optional<std::string> &id, const std::optional<std::string> &type,
                      const std::optional<std::string> &time, entity::ErrorList &errors);
    bool removeAsset(Device *device, const std::string &id, const std::optional<std::string> time);
    bool removeAllAssets(const std::optional<std::string> device,
                         const std::optional<std::string> type,
                         const std::optional<std::string> time, AssetList &list);

    // Message when adapter has connected and disconnected
    void connecting(Adapter *adapter);
    void disconnected(Adapter *adapter, std::vector<Device *> devices);
    void connected(Adapter *adapter, std::vector<Device *> devices);

    DataItem *getDataItemByName(const std::string &deviceName,
                                const std::string &dataItemName) const
    {
      auto dev = getDeviceByName(deviceName);
      return (dev) ? dev->getDeviceDataItem(dataItemName) : nullptr;
    }

    Observation *getFromBuffer(uint64_t seq) const { return m_circularBuffer.getFromBuffer(seq); }
    SequenceNumber_t getSequence() const { return m_circularBuffer.getSequence(); }
    unsigned int getBufferSize() const { return m_circularBuffer.getBufferSize(); }
    auto getMaxAssets() const { return m_assetBuffer.getMaxAssets(); }
    auto getAssetCount(bool active = true) const { return m_assetBuffer.getCount(active); }
    const auto &getAssets() const { return m_assetBuffer.getAssets(); }
    auto getFileCache() { return m_fileCache.get(); }

    auto getAssetCount(const std::string &type, bool active = true) const
    {
      return m_assetBuffer.getCountForType(type, active);
    }

    SequenceNumber_t getFirstSequence() const { return m_circularBuffer.getFirstSequence(); }

    // For testing...
    void setSequence(uint64_t seq) { m_circularBuffer.setSequence(seq); }
    auto getAgentDevice() { return m_agentDevice; }

    // MTConnect Requests
    RequestResult probeRequest(const std::string &format,
                               const std::optional<std::string> &device = std::nullopt);
    RequestResult currentRequest(const std::string &format,
                                 const std::optional<std::string> &device = std::nullopt,
                                 const std::optional<SequenceNumber_t> &at = std::nullopt,
                                 const std::optional<std::string> &path = std::nullopt);
    RequestResult sampleRequest(const std::string &format, const int count = 100,
                                const std::optional<std::string> &device = std::nullopt,
                                const std::optional<SequenceNumber_t> &from = std::nullopt,
                                const std::optional<SequenceNumber_t> &to = std::nullopt,
                                const std::optional<std::string> &path = std::nullopt);
    RequestResult streamSampleRequest(http_server::Response &response, const std::string &format,
                                      const int interval, const int heartbeat,
                                      const int count = 100,
                                      const std::optional<std::string> &device = std::nullopt,
                                      const std::optional<SequenceNumber_t> &from = std::nullopt,
                                      const std::optional<std::string> &path = std::nullopt);
    RequestResult streamCurrentRequest(http_server::Response &response, const std::string &format,
                                       const int interval,
                                       const std::optional<std::string> &device = std::nullopt,
                                       const std::optional<std::string> &path = std::nullopt);
    RequestResult assetRequest(const std::string &format, const int32_t count, const bool removed,
                               const std::optional<std::string> &type = std::nullopt,
                               const std::optional<std::string> &device = std::nullopt);
    RequestResult assetIdsRequest(const std::string &format, const std::list<std::string> &ids);
    RequestResult putAssetRequest(const std::string &format, const std::string &asset,
                                  const std::optional<std::string> &type,
                                  const std::optional<std::string> &device = std::nullopt,
                                  const std::optional<std::string> &uuid = std::nullopt);
    RequestResult deleteAssetRequest(const std::string &format, const std::list<std::string> &ids);
    RequestResult deleteAllAssetsRequest(const std::string &format,
                                         const std::optional<std::string> &device = std::nullopt,
                                         const std::optional<std::string> &type = std::nullopt);
    RequestResult putObservationRequest(const std::string &format, const std::string &device,
                                        const http_server::Routing::QueryMap observations,
                                        const std::optional<std::string> &time = std::nullopt);

    // For debugging
    void setLogStreamData(bool log) { m_logStreamData = log; }

    // Get the printer for a type
    const std::string acceptFormat(const std::string &accepts) const;
    Printer *getPrinter(const std::string &aType) const
    {
      auto printer = m_printers.find(aType);
      if (printer != m_printers.end())
        return printer->second.get();
      else
        return nullptr;
    }
    const Printer *printerForAccepts(const std::string &accepts) const
    {
      return getPrinter(acceptFormat(accepts));
    }

   protected:
    // Initialization methods
    void createAgentDevice();
    void loadXMLDeviceFile(const std::string &config);
    void verifyDevice(Device *device);
    void initializeDataItems(Device *device);
    void loadCachedProbe();

    // HTTP Routings
    void createPutObservationRoutings();
    void createFileRoutings();
    void createProbeRoutings();
    void createSampleRoutings();
    void createCurrentRoutings();
    void createAssetRoutings();

    // Current Data Collection
    std::string fetchCurrentData(const Printer *printer, const FilterSetOpt &filterSet,
                                 const std::optional<SequenceNumber_t> &at);

    // Sample data collection
    std::string fetchSampleData(const Printer *printer, const FilterSetOpt &filterSet, int count,
                                const std::optional<SequenceNumber_t> &from,
                                const std::optional<SequenceNumber_t> &to, SequenceNumber_t &end,
                                bool &endOfBuffer, ChangeObserver *observer = nullptr);

    // Asset methods
    void getAssets(const Printer *printer, const std::list<std::string> &ids, AssetList &list);
    void getAssets(const Printer *printer, const int32_t count, const bool removed,
                   const std::optional<std::string> &type, const std::optional<std::string> &device,
                   AssetList &list);

    // Output an XML Error
    std::string printError(const Printer *printer, const std::string &errorCode,
                           const std::string &text) const;

    // Handle the device/path parameters for the xpath search
    std::string devicesAndPath(const std::optional<std::string> &path, const Device *device) const;

    // Find data items by name/id
    DataItem *getDataItemById(const std::string &id) const
    {
      auto diPos = m_dataItemMap.find(id);
      if (diPos != m_dataItemMap.end())
        return diPos->second;
      return nullptr;
    }

    // Verification methods
    template <typename T>
    void checkRange(const Printer *printer, const T value, const T min, const T max,
                    const std::string &param, bool notZero = false) const;
    void checkPath(const Printer *printer, const std::optional<std::string> &path,
                   const Device *device, FilterSet &filter) const;
    Device *checkDevice(const Printer *printer, const std::string &uuid) const;

   protected:
    // Unique id based on the time of creation
    uint64_t m_instanceId;
    bool m_initialized{false};

    // HTTP Server
    std::unique_ptr<http_server::Server> m_server;
    std::unique_ptr<http_server::FileCache> m_fileCache;

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
    std::list<Adapter *> m_adapters;
    std::list<Device *> m_devices;
    std::map<std::string, Device *> m_deviceNameMap;
    std::map<std::string, Device *> m_deviceUuidMap;
    std::map<std::string, DataItem *> m_dataItemMap;

    // For debugging
    bool m_logStreamData;
    bool m_pretty;
  };
}  // namespace mtconnect
