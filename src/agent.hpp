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

#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "adapter/adapter.hpp"
#include "agent_loopback_pipeline.hpp"
#include "rest_service/asset_buffer.hpp"
#include "configuration/service.hpp"
#include "device_model/agent_device.hpp"
#include "device_model/device.hpp"
#include "rest_service/server.hpp"
#include "rest_service/checkpoint.hpp"
#include "rest_service/circular_buffer.hpp"
#include "pipeline/pipeline.hpp"
#include "pipeline/pipeline_contract.hpp"
#include "printer.hpp"
#include "xml_parser.hpp"

#include "sink.hpp"
#include "source.hpp"

namespace mtconnect
{
  struct AsyncSampleResponse;
  struct AsyncCurrentResponse;
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
    // Load agent with the xml configuration
    Agent(boost::asio::io_context &context, std::unique_ptr<rest_service::Server> &server,
          std::unique_ptr<rest_service::FileCache> &cache, const std::string &configXmlPath,
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
    
    // For testing
    auto getAgentDevice() { return m_agentDevice; }

  protected:
    friend class AgentPipelineContract;

    // Initialization methods
    void createAgentDevice();
    void loadXMLDeviceFile(const std::string &config);
    void verifyDevice(DevicePtr device);
    void initializeDataItems(DevicePtr device);
    void loadCachedProbe();


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

  protected:
    boost::asio::io_context &m_context;
    boost::asio::io_context::strand m_strand;

    // Unique id based on the time of creation
    bool m_initialized {false};
    
    // Sources and Sinks
    SourceList m_sources;
    SinkList m_sinks;

    // Pointer to the configuration file for node access
    std::unique_ptr<XmlParser> m_xmlParser;
    std::map<std::string, std::unique_ptr<Printer>> m_printers;

    // Agent Device
    device_model::AgentDevicePtr m_agentDevice;

    // Data containers
    std::list<adapter::Adapter *> m_adapters;
    std::list<DevicePtr> m_devices;
    std::unordered_map<std::string, DevicePtr> m_deviceNameMap;
    std::unordered_map<std::string, DevicePtr> m_deviceUuidMap;
    std::unordered_map<std::string, DataItemPtr> m_dataItemMap;

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
