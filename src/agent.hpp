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

#pragma once

#include "adapter/adapter.hpp"
#include "agent_loopback_pipeline.hpp"
#include "assets/asset_buffer.hpp"
#include "device_model/agent_device.hpp"
#include "device_model/device.hpp"
#include "http_server/server.hpp"
#include "observation/checkpoint.hpp"
#include "observation/circular_buffer.hpp"
#include "pipeline/pipeline.hpp"
#include "pipeline/pipeline_contract.hpp"
#include "printer.hpp"
#include "configuration/service.hpp"
#include "xml_parser.hpp"
#include <unordered_map>

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
  namespace adapter
  {
    class Adapter;
  }
  namespace device_model
  {
    namespace data_item
    {
      class DataItem;
    }
  };  // namespace device_model
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;

  using AssetChangeList = std::vector<std::pair<std::string, std::string>>;

  class Agent
  {
  public:
    struct RequestResult
    {
      RequestResult() {};
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

    // Make loopback pipeline
    void initialize(pipeline::PipelineContextPtr context, const ConfigOptions &options);

    // Start and stop
    void start();
    void stop();

    // HTTP Server
    auto getServer() const { return m_server.get(); }
    std::unique_ptr<pipeline::PipelineContract> makePipelineContract();

    // Add an adapter to the agent
    void addAdapter(adapter::Adapter *adapter, bool start = false);
    const auto &getAdapters() const { return m_adapters; }

    // Get device from device map
    DevicePtr getDeviceByName(const std::string &name);
    DevicePtr getDeviceByName(const std::string &name) const;
    DevicePtr findDeviceByUUIDorName(const std::string &idOrName) const;
    const auto &getDevices() const { return m_devices; }
    DevicePtr defaultDevice() const
    {
      if (m_devices.size() > 0)
      {
        for (auto device : m_devices)
          if (device->getName() != "Agent")
            return device;
      }

      return nullptr;
    }

    // Add the a device from a configuration file
    void addDevice(DevicePtr device);
    void deviceChanged(DevicePtr device, const std::string &oldUuid, const std::string &oldName);

    // Add component events to the sliding buffer
    observation::SequenceNumber_t addToBuffer(observation::ObservationPtr &observation);
    observation::SequenceNumber_t addToBuffer(DataItemPtr dataItem, entity::Properties props,
                                              std::optional<Timestamp> timestamp = std::nullopt);
    observation::SequenceNumber_t addToBuffer(DataItemPtr dataItem, const std::string &value,
                                              std::optional<Timestamp> timestamp = std::nullopt);

    // Asset management
    void addAsset(AssetPtr asset);
    AssetPtr addAsset(DevicePtr device, const std::string &asset,
                      const std::optional<std::string> &id, const std::optional<std::string> &type,
                      const std::optional<std::string> &time, entity::ErrorList &errors);
    bool removeAsset(DevicePtr device, const std::string &id,
                     const std::optional<Timestamp> time = std::nullopt);
    bool removeAllAssets(const std::optional<std::string> device,
                         const std::optional<std::string> type, const std::optional<Timestamp> time,
                         AssetList &list);

    // Message when adapter has connected and disconnected
    void connecting(const std::string &adapter);
    void disconnected(const std::string &adapter, const StringList &devices, bool autoAvailable);
    void connected(const std::string &adapter, const StringList &devices, bool autoAvailable);

    // Message protocol command
    void receiveCommand(const std::string &device, const std::string &command,
                        const std::string &value, const std::string &source);

    DataItemPtr getDataItemForDevice(const std::string &deviceName,
                                     const std::string &dataItemName) const
    {
      auto dev = findDeviceByUUIDorName(deviceName);
      return (dev) ? dev->getDeviceDataItem(dataItemName) : nullptr;
    }

    observation::ObservationPtr getFromBuffer(uint64_t seq) const
    {
      return m_circularBuffer.getFromBuffer(seq);
    }
    observation::SequenceNumber_t getSequence() const { return m_circularBuffer.getSequence(); }
    unsigned int getBufferSize() const { return m_circularBuffer.getBufferSize(); }
    auto getMaxAssets() const { return m_assetBuffer.getMaxAssets(); }
    auto getAssetCount(bool active = true) const { return m_assetBuffer.getCount(active); }
    const auto &getAssets() const { return m_assetBuffer.getAssets(); }
    auto getFileCache() { return m_fileCache.get(); }

    auto getAssetCount(const std::string &type, bool active = true) const
    {
      return m_assetBuffer.getCountForType(type, active);
    }

    observation::SequenceNumber_t getFirstSequence() const
    {
      return m_circularBuffer.getFirstSequence();
    }

    // For testing...
    void setSequence(uint64_t seq) { m_circularBuffer.setSequence(seq); }
    auto getAgentDevice() { return m_agentDevice; }

    // MTConnect Requests
    RequestResult probeRequest(const std::string &format,
                               const std::optional<std::string> &device = std::nullopt);
    RequestResult currentRequest(
        const std::string &format, const std::optional<std::string> &device = std::nullopt,
        const std::optional<observation::SequenceNumber_t> &at = std::nullopt,
        const std::optional<std::string> &path = std::nullopt);
    RequestResult sampleRequest(
        const std::string &format, const int count = 100,
        const std::optional<std::string> &device = std::nullopt,
        const std::optional<observation::SequenceNumber_t> &from = std::nullopt,
        const std::optional<observation::SequenceNumber_t> &to = std::nullopt,
        const std::optional<std::string> &path = std::nullopt);
    RequestResult streamSampleRequest(
        http_server::Response &response, const std::string &format, const int interval,
        const int heartbeat, const int count = 100,
        const std::optional<std::string> &device = std::nullopt,
        const std::optional<observation::SequenceNumber_t> &from = std::nullopt,
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
    friend class AgentPipelineContract;

    // Initialization methods
    void createAgentDevice();
    void loadXMLDeviceFile(const std::string &config);
    void verifyDevice(DevicePtr device);
    void initializeDataItems(DevicePtr device);
    void loadCachedProbe();

    // HTTP Routings
    void createPutObservationRoutings();
    void createFileRoutings();
    void createProbeRoutings();
    void createSampleRoutings();
    void createCurrentRoutings();
    void createAssetRoutings();

    // Current Data Collection
    std::string fetchCurrentData(const Printer *printer, const observation::FilterSetOpt &filterSet,
                                 const std::optional<observation::SequenceNumber_t> &at);

    // Sample data collection
    std::string fetchSampleData(const Printer *printer, const observation::FilterSetOpt &filterSet,
                                int count, const std::optional<observation::SequenceNumber_t> &from,
                                const std::optional<observation::SequenceNumber_t> &to,
                                observation::SequenceNumber_t &end, bool &endOfBuffer,
                                observation::ChangeObserver *observer = nullptr);

    // Asset methods
    void getAssets(const Printer *printer, const std::list<std::string> &ids, AssetList &list);
    void getAssets(const Printer *printer, const int32_t count, const bool removed,
                   const std::optional<std::string> &type, const std::optional<std::string> &device,
                   AssetList &list);

    // Output an XML Error
    std::string printError(const Printer *printer, const std::string &errorCode,
                           const std::string &text) const;

    // Handle the device/path parameters for the xpath search
    std::string devicesAndPath(const std::optional<std::string> &path,
                               const DevicePtr device) const;

    // Find data items by name/id
    DataItemPtr getDataItemById(const std::string &id) const
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
                   const DevicePtr device, observation::FilterSet &filter) const;
    DevicePtr checkDevice(const Printer *printer, const std::string &uuid) const;

  protected:
    // Unique id based on the time of creation
    uint64_t m_instanceId;
    bool m_initialized {false};

    // HTTP Server
    std::unique_ptr<http_server::Server> m_server;
    std::unique_ptr<http_server::FileCache> m_fileCache;

    // Pointer to the configuration file for node access
    std::unique_ptr<XmlParser> m_xmlParser;
    std::map<std::string, std::unique_ptr<Printer>> m_printers;

    // Circular Buffer
    observation::CircularBuffer m_circularBuffer;

    // Agent Device
    device_model::AgentDevicePtr m_agentDevice;

    // Asset Buffer
    AssetBuffer m_assetBuffer;

    // Data containers
    std::list<adapter::Adapter *> m_adapters;
    std::list<DevicePtr> m_devices;
    std::unordered_map<std::string, DevicePtr> m_deviceNameMap;
    std::unordered_map<std::string, DevicePtr> m_deviceUuidMap;
    std::unordered_map<std::string, DataItemPtr> m_dataItemMap;

    // Loopback
    std::unique_ptr<AgentLoopbackPipeline> m_loopback;

    // Xml Config
    std::string m_version;
    std::string m_configXmlPath;

    // For debugging
    bool m_logStreamData;
    bool m_pretty;
  };

  class AgentPipelineContract : public pipeline::PipelineContract
  {
  public:
    AgentPipelineContract(Agent *agent) : m_agent(agent) {}
    ~AgentPipelineContract() = default;

    DevicePtr findDevice(const std::string &device) override
    {
      return m_agent->findDeviceByUUIDorName(device);
    }
    DataItemPtr findDataItem(const std::string &device, const std::string &name) override
    {
      DevicePtr dev = m_agent->findDeviceByUUIDorName(device);
      if (dev != nullptr)
      {
        return dev->getDeviceDataItem(name);
      }
      return nullptr;
    }
    void eachDataItem(EachDataItem fun) override
    {
      for (auto &di : m_agent->m_dataItemMap)
      {
        fun(di.second);
      }
    }
    void deliverObservation(observation::ObservationPtr obs) override { m_agent->addToBuffer(obs); }
    void deliverAsset(AssetPtr asset) override { m_agent->addAsset(asset); }
    void deliverAssetCommand(entity::EntityPtr command) override;
    void deliverConnectStatus(entity::EntityPtr, const StringList &devices,
                              bool autoAvailable) override;
    void deliverCommand(entity::EntityPtr) override;

  protected:
    Agent *m_agent;
  };

  inline std::unique_ptr<pipeline::PipelineContract> Agent::makePipelineContract()
  {
    return std::make_unique<AgentPipelineContract>(this);
  }

}  // namespace mtconnect
