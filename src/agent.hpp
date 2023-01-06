//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "asset/asset_buffer.hpp"
#include "buffer/checkpoint.hpp"
#include "buffer/circular_buffer.hpp"
#include "configuration/service.hpp"
#include "device_model/agent_device.hpp"
#include "device_model/device.hpp"
#include "parser/xml_parser.hpp"
#include "pipeline/pipeline.hpp"
#include "pipeline/pipeline_contract.hpp"
#include "printer/printer.hpp"
#include "sink/rest_sink/rest_service.hpp"
#include "sink/rest_sink/server.hpp"
#include "sink/sink.hpp"
#include "source/adapter/adapter.hpp"
#include "source/loopback_source.hpp"
#include "source/source.hpp"

namespace mtconnect {
  namespace mic = boost::multi_index;

  namespace source::adapter {
    class Adapter;
  }

  namespace sink {
    class Sink;
  }

  namespace device_model {
    namespace data_item {
      class DataItem;
    }
  };  // namespace device_model
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;
  using WeakDataItemPtr = std::weak_ptr<device_model::data_item::DataItem>;

  using AssetChangeList = std::vector<std::pair<std::string, std::string>>;

  class Agent
  {
  public:
    // Load agent with the xml configuration
    Agent(boost::asio::io_context &context, const std::string &deviceXmlPath,
          const ConfigOptions &options);

    // Virtual destructor
    ~Agent();

    // Initialize models and pipeline
    void initialize(pipeline::PipelineContextPtr context);
    void initialDataItemObservations();

    // Start and stop
    void start();
    void stop();
    auto &getContext() { return m_context; }

    // Pipeline Contract
    std::unique_ptr<pipeline::PipelineContract> makePipelineContract();
    auto getPipelineContext() { return m_pipelineContext; }

    // Sink Contract
    sink::SinkContractPtr makeSinkContract();
    const auto &getXmlParser() const { return m_xmlParser; }

    auto &getCircularBuffer() { return m_circularBuffer; }

    // Add an adapter to the agent
    void addSource(source::SourcePtr adapter, bool start = false);
    void addSink(sink::SinkPtr sink, bool start = false);

    // Source and Sink
    source::SourcePtr findSource(const std::string &name) const
    {
      for (auto &s : m_sources)
        if (s->getIdentity() == name)
          return s;

      return nullptr;
    }

    sink::SinkPtr findSink(const std::string &name) const
    {
      for (auto &s : m_sinks)
        if (s->getName() == name)
          return s;

      return nullptr;
    }

    const auto &getSources() const { return m_sources; }
    const auto &getSinks() const { return m_sinks; }

    const auto &getSchemaVersion() const { return m_schemaVersion; }

    // Get device from device map
    DevicePtr getDeviceByName(const std::string &name);
    DevicePtr getDeviceByName(const std::string &name) const;
    DevicePtr findDeviceByUUIDorName(const std::string &idOrName) const;
    const auto getDevices() const
    {
      std::list<DevicePtr> list;
      boost::push_back(list, m_deviceIndex);
      return list;
    }
    DevicePtr defaultDevice() const
    {
      if (m_deviceIndex.size() > 0)
      {
        for (auto device : m_deviceIndex)
          if (device->getName() != "Agent")
            return device;
      }

      return nullptr;
    }

    // Asset information
    asset::AssetStorage *getAssetStorage() { return m_assetStorage.get(); }

    // Add the a device from a configuration file
    void addDevice(DevicePtr device);
    void deviceChanged(DevicePtr device, const std::string &oldUuid, const std::string &oldName);
    bool reloadDevices(const std::string &deviceFile);

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

    // Find data items by name/id
    DataItemPtr getDataItemById(const std::string &id) const
    {
      auto diPos = m_dataItemMap.find(id);
      if (diPos != m_dataItemMap.end())
        return diPos->second.lock();
      return nullptr;
    }

    // Pipeline methods
    void receiveObservation(observation::ObservationPtr observation);
    void receiveAsset(asset::AssetPtr asset);
    bool receiveDevice(device_model::DevicePtr device, bool version = true);
    bool removeAsset(DevicePtr device, const std::string &id,
                     const std::optional<Timestamp> time = std::nullopt);
    bool removeAllAssets(const std::optional<std::string> device,
                         const std::optional<std::string> type, const std::optional<Timestamp> time,
                         asset::AssetList &list);
    void notifyAssetRemoved(DevicePtr device, const asset::AssetPtr &asset);

    // Adapter feedback
    void sourceFailed(const std::string &identity);

    // For testing
    auto getAgentDevice() { return m_agentDevice; }

    // Printers
    printer::Printer *getPrinter(const std::string &aType) const
    {
      auto printer = m_printers.find(aType);
      if (printer != m_printers.end())
        return printer->second.get();
      else
        return nullptr;
    }
    const auto &getPrinters() const { return m_printers; }

    // Handle the device/path parameters for the xpath search
    std::string devicesAndPath(const std::optional<std::string> &path,
                               const DevicePtr device) const;

  protected:
    friend class AgentPipelineContract;

    // Initialization methods
    void createAgentDevice();
    std::list<device_model::DevicePtr> loadXMLDeviceFile(const std::string &config);
    void verifyDevice(DevicePtr device);
    void initializeDataItems(DevicePtr device,
                             std::optional<std::set<std::string>> skip = std::nullopt);
    void loadCachedProbe();
    void versionDeviceXml();

    // Asset count management
    void updateAssetCounts(const DevicePtr &device, const std::optional<std::string> type);

    observation::ObservationPtr getLatest(const std::string &id)
    {
      return m_circularBuffer.getLatest().getObservation(id);
    }

    observation::ObservationPtr getLatest(const DataItemPtr &di) { return getLatest(di->getId()); }

  protected:
    ConfigOptions m_options;
    boost::asio::io_context &m_context;
    boost::asio::io_context::strand m_strand;

    std::shared_ptr<source::LoopbackSource> m_loopback;

    // Asset Management
    std::unique_ptr<asset::AssetStorage> m_assetStorage;

    // Unique id based on the time of creation
    bool m_initialized {false};
    bool m_observationsInitialized {false};

    // Sources and Sinks
    source::SourceList m_sources;
    sink::SinkList m_sinks;

    // Pipeline
    pipeline::PipelineContextPtr m_pipelineContext;

    // Pointer to the configuration file for node access
    std::unique_ptr<parser::XmlParser> m_xmlParser;
    PrinterMap m_printers;

    // Agent Device
    device_model::AgentDevicePtr m_agentDevice;

    struct BySeq
    {};
    struct ByName
    {};
    struct ByUuid
    {};

    struct ExtractDeviceUuid
    {
      using result_type = std::string;
      const result_type &operator()(const DevicePtr &d) const { return *d->getUuid(); }
      result_type operator()(const DevicePtr &d) { return *d->getUuid(); }
    };
    struct ExtractDeviceName
    {
      using result_type = std::string;
      const result_type &operator()(const DevicePtr &d) const { return *d->getComponentName(); }
      result_type operator()(DevicePtr &d) { return *d->getComponentName(); }
    };

    // Data containers
    using DeviceIndex = mic::multi_index_container<
        DevicePtr, mic::indexed_by<mic::sequenced<mic::tag<BySeq>>,
                                   mic::hashed_unique<mic::tag<ByUuid>, ExtractDeviceUuid>,
                                   mic::hashed_unique<mic::tag<ByName>, ExtractDeviceName>>>;

    DeviceIndex m_deviceIndex;
    std::unordered_map<std::string, WeakDataItemPtr> m_dataItemMap;

    // Xml Config
    std::optional<std::string> m_schemaVersion;
    std::string m_deviceXmlPath;
    bool m_versionDeviceXml = false;

    // Circular Buffer
    buffer::CircularBuffer m_circularBuffer;

    // For debugging
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
        auto ldi = di.second.lock();
        if (ldi)
          fun(ldi);
      }
    }
    void deliverObservation(observation::ObservationPtr obs) override
    {
      m_agent->receiveObservation(obs);
    }
    void deliverAsset(asset::AssetPtr asset) override { m_agent->receiveAsset(asset); }
    void deliverAssetCommand(entity::EntityPtr command) override;
    void deliverConnectStatus(entity::EntityPtr, const StringList &devices,
                              bool autoAvailable) override;
    void deliverCommand(entity::EntityPtr) override;
    void deliverDevice(DevicePtr device) override { m_agent->receiveDevice(device); }

    void sourceFailed(const std::string &identity) override { m_agent->sourceFailed(identity); }

  protected:
    Agent *m_agent;
  };

  inline std::unique_ptr<pipeline::PipelineContract> Agent::makePipelineContract()
  {
    return std::make_unique<AgentPipelineContract>(this);
  }

  class AgentSinkContract : public sink::SinkContract
  {
  public:
    AgentSinkContract(Agent *agent) : m_agent(agent) {}
    ~AgentSinkContract() = default;

    printer::Printer *getPrinter(const std::string &aType) const override
    {
      return m_agent->getPrinter(aType);
    }

    // Get device from device map
    DevicePtr getDeviceByName(const std::string &name) const override
    {
      return m_agent->getDeviceByName(name);
    }
    DevicePtr findDeviceByUUIDorName(const std::string &idOrName) const override
    {
      return m_agent->findDeviceByUUIDorName(idOrName);
    }
    const std::list<DevicePtr> getDevices() const override { return m_agent->getDevices(); }
    DevicePtr defaultDevice() const override { return m_agent->defaultDevice(); }
    DataItemPtr getDataItemById(const std::string &id) const override
    {
      return m_agent->getDataItemById(id);
    }
    void addSource(source::SourcePtr source) override { m_agent->addSource(source); }

    // Asset information
    asset::AssetStorage *getAssetStorage() override { return m_agent->getAssetStorage(); }
    const PrinterMap &getPrinters() const override { return m_agent->getPrinters(); }

    void getDataItemsForPath(const DevicePtr device, const std::optional<std::string> &path,
                             FilterSet &filter) const override
    {
      std::string dataPath = m_agent->devicesAndPath(path, device);
      const auto &parser = m_agent->getXmlParser();
      parser->getDataItems(filter, dataPath);
    }

    buffer::CircularBuffer &getCircularBuffer() override { return m_agent->getCircularBuffer(); }

  protected:
    Agent *m_agent;
  };

  inline sink::SinkContractPtr Agent::makeSinkContract()
  {
    return std::make_unique<AgentSinkContract>(this);
  }
}  // namespace mtconnect
