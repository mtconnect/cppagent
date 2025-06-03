//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/asset/asset_buffer.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/buffer/circular_buffer.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/configuration/async_context.hpp"
#include "mtconnect/configuration/hook_manager.hpp"
#include "mtconnect/configuration/service.hpp"
#include "mtconnect/device_model/agent_device.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/parser/xml_parser.hpp"
#include "mtconnect/pipeline/pipeline.hpp"
#include "mtconnect/pipeline/pipeline_contract.hpp"
#include "mtconnect/printer/printer.hpp"
#include "mtconnect/sink/rest_sink/rest_service.hpp"
#include "mtconnect/sink/rest_sink/server.hpp"
#include "mtconnect/sink/sink.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/loopback_source.hpp"
#include "mtconnect/source/source.hpp"

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

  /// Agent Class controls message flow and owns all sources and sinks.
  class AGENT_LIB_API Agent
  {
  public:
    /// @brief Agent constructor
    /// @param[in] context boost asio io context
    /// @param[in] deviceXmlPath Device.xml configuration file path
    /// @param[in] options Configuration Options
    ///     - SchemaVersion
    ///     - CheckpointFrequency
    ///     - Pretty
    ///     - VersionDeviceXml
    ///     - JsonVersion
    ///     - DisableAgentDevice
    Agent(configuration::AsyncContext &context, const std::string &deviceXmlPath,
          const ConfigOptions &options);

    /// Destructor for the Agent.
    /// > Note: Does not stop the agent.
    ~Agent();

    /// @brief Hook callback type
    using Hook = std::function<void(Agent &)>;

    /// @brief Functions to run before the agent begins the initialization process.
    /// @return configuration::HookManager<Agent>&
    auto &beforeInitializeHooks() { return m_beforeInitializeHooks; }

    /// @brief Function that run after all agent initialization is complete
    /// @return configuration::HookManager<Agent>&
    auto &afterInitializeHooks() { return m_afterInitializeHooks; }

    /// @brief Hooks to run when before the agent starts all the soures and sinks
    /// @return configuration::HookManager<Agent>&
    auto &beforeStartHooks() { return m_beforeStartHooks; }

    /// @brief Hooks to run when after the agent starts all the soures and sinks
    /// @return configuration::HookManager<Agent>&
    auto &afterStartHooks() { return m_afterStartHooks; }

    /// @brief Hooks before the agent stops all the sources and sinks
    /// @return configuration::HookManager<Agent>&
    auto &beforeStopHooks() { return m_beforeStopHooks; }

    /// @brief Hooks before the agent versions and write the device xml file
    /// @return configuration::HookManager<Agent>&
    auto &beforeDeviceXmlUpdateHooks() { return m_beforeDeviceXmlUpdateHooks; }

    /// @brief Hooks after the agent versions and write the device xml file
    /// @return configuration::HookManager<Agent>&
    auto &afterDeviceXmlUpdateHooks() { return m_afterDeviceXmlUpdateHooks; }

    /// @brief the agent given a pipeline context
    /// @param context: the pipeline context shared between all pipelines
    void initialize(pipeline::PipelineContextPtr context);

    /// @brief initial UNAVAILABLE observations for all data items
    ///        unless they have constant values.
    void initialDataItemObservations();

    // Start and stop
    /// @brief Starts all the sources and sinks
    void start();
    /// @brief Stops all the sources and syncs.
    void stop();

    /// @brief Get the boost asio io context
    /// @return boost::asio::io_context
    auto &getContext() { return m_context; }

    /// @brief Create a contract for pipelines to access agent information
    /// @return A contract between the pipeline and this agent
    std::unique_ptr<pipeline::PipelineContract> makePipelineContract();
    /// @brief Gets the pipe context shared by all pipelines
    /// @return A shared pipeline context
    auto getPipelineContext() { return m_pipelineContext; }

    /// @brief Makes a unique sink contract
    /// @return A contract between the sink and the agent
    sink::SinkContractPtr makeSinkContract();
    /// @brief Get a reference to the XML parser
    /// @return The XML parser
    const auto &getXmlParser() const { return m_xmlParser; }
    /// @brief Get a reference to the circular buffer. Used by sinks to
    ///        get latest and historical data.
    /// @return A reference to the circular buffer
    auto &getCircularBuffer() { return m_circularBuffer; }
    /// @brief Get a const reference to the circular buffer. Used by sinks to
    ///        get latest and historical data.
    /// @return A const reference to the circular buffer
    const auto &getCircularBuffer() const { return m_circularBuffer; }

    /// @brief Adds an adapter to the agent
    /// @param[in] source: shared pointer to the source being added
    /// @param[in] start: starts the source if start is true, otherwise delayed start
    void addSource(source::SourcePtr source, bool start = false);
    /// @brief Adds a sink to the agent
    /// @param[in] sink shared pointer to the the sink being added
    /// @param[in] start: starts the source if start is true, otherwise delayed start
    void addSink(sink::SinkPtr sink, bool start = false);

    // Source and Sink
    /// @brief Find a source by name
    /// @param[in] name the identity to find
    /// @return A shared pointer to the source if found, otherwise nullptr
    source::SourcePtr findSource(const std::string &name) const
    {
      for (auto &s : m_sources)
      {
        if (s->getIdentity() == name || s->getName() == name)
          return s;
      }
      return nullptr;
    }
    /// @brief Find a sink by name
    /// @param name the name to find
    /// @return A shared pointer to the sink if found, otherwise nullptr
    sink::SinkPtr findSink(const std::string &name) const
    {
      for (auto &s : m_sinks)
        if (s->getName() == name)
          return s;

      return nullptr;
    }

    /// @brief Get the list of all sources
    /// @return The list of all source in the agent
    const auto &getSources() const { return m_sources; }
    /// @brief Get the list of all sinks
    /// @return The list of all sinks in the agent
    const auto &getSinks() const { return m_sinks; }

    /// @brief Get the MTConnect schema version the agent is supporting
    /// @return The MTConnect schema version as a string
    const auto &getSchemaVersion() const { return m_schemaVersion; }

    /// @brief Get the validation state of the agent
    /// @returns the validation state of the agent
    bool isValidating() const { return m_validation; }

    /// @brief Get the integer schema version based on configuration.
    /// @returns the schema version as an integer [major * 100 + minor] as a 32bit integer.
    const auto getIntSchemaVersion() const { return m_intSchemaVersion; }

    /// @brief Find a device by name
    /// @param[in] name The name of the device to find
    /// @return A shared pointer to the device
    DevicePtr getDeviceByName(const std::string &name);
    /// @brief Find a device by name (Const Version)
    /// @param[in] name The name of the device to find
    /// @return A shared pointer to the device
    DevicePtr getDeviceByName(const std::string &name) const;
    /// @brief Finds the device given either its UUID or its name
    /// @param[in] idOrName The uuid or name of the device
    /// @return A shared pointer to the device
    DevicePtr findDeviceByUUIDorName(const std::string &idOrName) const;
    /// @brief Gets the list of devices
    /// @return The list of devices owned by the caller
    const auto getDevices() const
    {
      std::list<DevicePtr> list;
      boost::push_back(list, m_deviceIndex);
      return list;
    }

    /// @brief Get a pointer to the default device
    ///
    /// The default device is the first device that is not the Agent device.
    ///
    /// @return A shared pointer to the default device
    DevicePtr getDefaultDevice() const
    {
      if (m_deviceIndex.size() > 0)
      {
        for (auto device : m_deviceIndex)
          if (device->getName() != "Agent")
            return device;
      }

      return nullptr;
    }
    /// @deprecated use `getDefaultDevice()` instead
    /// @brief Get a pointer to the default device
    /// @return A shared pointer to the default device
    /// @note Cover method for `getDefaultDevice()`
    DevicePtr defaultDevice() const { return getDefaultDevice(); }

    /// @brief Get a pointer to the asset storage object
    /// @return A pointer to the asset storage object
    asset::AssetStorage *getAssetStorage() { return m_assetStorage.get(); }

    /// @brief Add a device to the agent
    /// @param[in] device The device to add.
    /// @note This method is not fully implemented after agent initialization
    void addDevice(DevicePtr device);
    /// @brief Updates a device's UUID and/or its name
    /// @param[in] device The modified device
    /// @param[in] oldUuid The old uuid
    /// @param[in] oldName The old name
    void deviceChanged(DevicePtr device, const std::string &uuid);
    /// @brief Reload the devices from a device file after updates
    /// @param[in] deviceFile The device file to load
    /// @return true if successful
    bool reloadDevices(const std::string &deviceFile);

    /// @brief receive a single device from a source
    /// @param[in] deviceXml the device xml as a string
    /// @param[in] source the source loading the device
    void loadDevices(std::list<DevicePtr> device,
                     const std::optional<std::string> source = std::nullopt, bool force = false);

    /// @brief receive and parse a single device from a source
    /// @param[in] deviceXml the device xml as a string
    /// @param[in] source the source loading the device
    void loadDeviceXml(const std::string &deviceXml,
                       const std::optional<std::string> source = std::nullopt);

    /// @name Message when source has connected and disconnected
    ///@{

    /// @brief Called when source begins trying to connect
    /// @param source The source identity
    void connecting(const std::string &source);
    /// @brief Called when source is disconnected
    /// @param[in] source The source identity
    /// @param[in] devices The list of devices associated with this source
    /// @param[in] autoAvailable `true` if the source should automatically set available to
    /// `UNAVAILABLE`
    void disconnected(const std::string &source, const StringList &devices, bool autoAvailable);
    /// @brief Called when source is connected
    /// @param source The source identity
    /// @param[in] devices The list of devices associated with this source
    /// @param[in] autoAvailable `true` if the source should automatically set available to
    /// `AVAILABLE`
    void connected(const std::string &source, const StringList &devices, bool autoAvailable);

    ///@}

    /// @brief Called when a source receives a command from a data source
    /// @param[in] device The device name associated with this source
    /// @param[in] command The command being sent
    /// @param[in] value The value of the command
    /// @param[in] source The identity of the source
    void receiveCommand(const std::string &device, const std::string &command,
                        const std::string &value, const std::string &source);

    /// @brief Method to get a data item for a device
    /// @param[in] deviceName The name or uuid of the device
    /// @param[in] dataItemName The name or id of the data item
    /// @return Shared pointer to the data item if found
    /// @note Cover method for `findDeviceByUUIDorName()` and `DataItem::getDeviceDataItem()` from
    /// the device.
    DataItemPtr getDataItemForDevice(const std::string &deviceName,
                                     const std::string &dataItemName) const
    {
      auto dev = findDeviceByUUIDorName(deviceName);
      return (dev) ? dev->getDeviceDataItem(dataItemName) : nullptr;
    }

    /// @brief Get a data item by its id.
    /// @param id Unique id of the data item
    /// @return Shared pointer to the data item if found
    DataItemPtr getDataItemById(const std::string &id) const
    {
      auto diPos = m_dataItemMap.find(id);
      if (diPos != m_dataItemMap.end())
        return diPos->second.lock();
      return nullptr;
    }

    /// @name Pipeline related methods to receive data from sources
    ///@{

    /// @brief Receive an observation
    /// @param[in] observation A shared pointer to the observation
    void receiveObservation(observation::ObservationPtr observation);
    /// @brief Receive an asset
    /// @param[in] asset A shared pointer to the asset
    void receiveAsset(asset::AssetPtr asset);
    /// @brief Receive a device
    /// @param[in] device A shared pointer to the device
    /// @param[in] version If the DeviceXML should be versioned
    /// @return If the device could be updated or added
    bool receiveDevice(device_model::DevicePtr device, bool version = true);
    /// @brief Received an instruction from the source to remove an asset
    /// @param[in] device  Device name or uuid the asset belongs to
    /// @param[in] id The asset id
    /// @param[in] time The timestamp the remove occurred at
    /// @return `true` if the asset was found and removed
    bool removeAsset(DevicePtr device, const std::string &id,
                     const std::optional<Timestamp> time = std::nullopt);
    /// @brief Removes all assets for by device, type, or device and type
    /// @param[in] device Optional device name or uuid
    /// @param[in] type Optional type of asset
    /// @param[in] time Timestamp of the observations
    /// @param[out] list List of assets removed
    /// @return `true` if any assets were found
    bool removeAllAssets(const std::optional<std::string> device,
                         const std::optional<std::string> type, const std::optional<Timestamp> time,
                         asset::AssetList &list);
    /// @brief Send asset removed observation when an asset is removed.
    ///
    /// Also sets asset changed to `UNAVAILABLE` if the asset removed asset was the last changed.
    ///
    /// @param device The device related to the asset
    /// @param asset The asset
    void notifyAssetRemoved(DevicePtr device, const asset::AssetPtr &asset);

    ///@}

    /// @brief Method called by source when it cannot continue
    /// @param identity identity of the source
    void sourceFailed(const std::string &identity);

    /// @name For testing
    ///@{

    /// @brief Returns a shared pointer to the agent device
    /// @return shared pointer to the agent device
    auto getAgentDevice() { return m_agentDevice; }
    ///@}

    /// @brief Get a pointer to the printer for a mime type
    /// @param type The mime type
    /// @return pointer to the printer or nullptr if it does not exist
    /// @note Currently `xml` and `json` are supported.
    printer::Printer *getPrinter(const std::string &type) const
    {
      auto printer = m_printers.find(type);
      if (printer != m_printers.end())
        return printer->second.get();
      else
        return nullptr;
    }

    /// @brief Get the map of available printers
    /// @return A const reference to the printer map
    const auto &getPrinters() const { return m_printers; }

    /// @brief Prefixes the path with the device and rewrites the composed
    ///        paths by repeating the prefix. The resulting path is valid
    ///        XPath.
    ///
    /// For example: a Device with a uuid="AAABBB" with the path
    /// ```
    /// //DataItem[@type='EXECUTION']|//DataItem[@type='CONTROLLER_MODE']
    /// ```
    ///
    /// Is rewritten:
    /// ```
    /// //Devices/Device[@uuid="AAABBB"]//DataItem[@type='EXECUTION']|//Devices/Device[@uuid="AAABBB"]//DataItem[@type='CONTROLLER_MODE']
    /// ```
    ///
    /// @param[in] path Optional path to prefix
    /// @param[in] device Optional device if one device is specified
    /// @param[in] deviceType optional Agent or Device selector
    /// @return The rewritten path properly prefixed
    std::string devicesAndPath(const std::optional<std::string> &path, const DevicePtr device,
                               const std::optional<std::string> &deviceType = std::nullopt) const;

    /// @brief Creates unique ids for the device model and maps to the originals
    ///
    /// Also updates the agents data item map by adding the new ids. Duplicate original
    /// ids will be last in wins.
    ///
    /// @param[in] device device to modify
    void createUniqueIds(DevicePtr device);

    /// @brief get agent options
    /// @returns constant reference to option map
    const auto &getOptions() const { return m_options; }

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
    configuration::AsyncContext &m_context;
    boost::asio::io_context::strand m_strand;

    std::shared_ptr<source::LoopbackSource> m_loopback;

    bool m_started {false};

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

    /// @brief tag for Device multi-index unsorted
    struct BySeq
    {};
    /// @brief tag for Device multi-index by Name
    struct ByName
    {};
    /// @brief tag for Device multi-index by UUID
    struct ByUuid
    {};

    /// @brief Device UUID extractor for multi-index
    struct ExtractDeviceUuid
    {
      using result_type = std::string;
      const result_type &operator()(const DevicePtr &d) const { return *d->getUuid(); }
      result_type operator()(const DevicePtr &d) { return *d->getUuid(); }
    };

    /// @brief Device name extractor for multi-index
    struct ExtractDeviceName
    {
      using result_type = std::string;
      const result_type &operator()(const DevicePtr &d) const { return *d->getComponentName(); }
      result_type operator()(DevicePtr &d) { return *d->getComponentName(); }
    };

    /// @brief Devuce multi-index
    using DeviceIndex = mic::multi_index_container<
        DevicePtr, mic::indexed_by<mic::sequenced<mic::tag<BySeq>>,
                                   mic::hashed_unique<mic::tag<ByUuid>, ExtractDeviceUuid>,
                                   mic::hashed_unique<mic::tag<ByName>, ExtractDeviceName>>>;

    DeviceIndex m_deviceIndex;
    std::unordered_map<std::string, WeakDataItemPtr> m_dataItemMap;

    // Xml Config
    std::optional<std::string> m_schemaVersion;
    std::string m_deviceXmlPath;
    bool m_versionDeviceXml {false};
    bool m_createUniqueIds {false};
    int32_t m_intSchemaVersion = IntDefaultSchemaVersion();

    // Circular Buffer
    buffer::CircularBuffer m_circularBuffer;

    // For debugging
    bool m_pretty;

    // validation
    bool m_validation {false};

    // Agent hooks
    configuration::HookManager<Agent> m_beforeInitializeHooks;
    configuration::HookManager<Agent> m_afterInitializeHooks;
    configuration::HookManager<Agent> m_beforeStartHooks;
    configuration::HookManager<Agent> m_afterStartHooks;
    configuration::HookManager<Agent> m_beforeStopHooks;
    configuration::HookManager<Agent> m_beforeDeviceXmlUpdateHooks;
    configuration::HookManager<Agent> m_afterDeviceXmlUpdateHooks;
  };

  /// @brief Association of the pipeline's interface to the `Agent`
  class AGENT_LIB_API AgentPipelineContract : public pipeline::PipelineContract
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
    int32_t getSchemaVersion() const override { return m_agent->getIntSchemaVersion(); }
    bool isValidating() const override { return m_agent->isValidating(); }
    void deliverObservation(observation::ObservationPtr obs) override
    {
      m_agent->receiveObservation(obs);
    }
    void deliverAsset(asset::AssetPtr asset) override { m_agent->receiveAsset(asset); }
    void deliverAssetCommand(entity::EntityPtr command) override;
    void deliverConnectStatus(entity::EntityPtr, const StringList &devices,
                              bool autoAvailable) override;
    void deliverCommand(entity::EntityPtr) override;
    void deliverDevice(DevicePtr device) override
    {
      m_agent->loadDevices({device}, std::nullopt, true);
    }
    void deliverDevices(std::list<DevicePtr> devices) override { m_agent->loadDevices(devices); }

    void sourceFailed(const std::string &identity) override { m_agent->sourceFailed(identity); }

    const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override
    {
      return m_agent->getCircularBuffer().checkDuplicate(obs);
    }

  protected:
    Agent *m_agent;
  };

  inline std::unique_ptr<pipeline::PipelineContract> Agent::makePipelineContract()
  {
    return std::make_unique<AgentPipelineContract>(this);
  }

  /// @brief The sinks interface to the `Agent`
  class AGENT_LIB_API AgentSinkContract : public sink::SinkContract
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
    DevicePtr getDefaultDevice() const override { return m_agent->getDefaultDevice(); }
    DataItemPtr getDataItemById(const std::string &id) const override
    {
      return m_agent->getDataItemById(id);
    }
    void addSource(source::SourcePtr source) override { m_agent->addSource(source); }

    // Asset information
    asset::AssetStorage *getAssetStorage() override { return m_agent->getAssetStorage(); }
    const PrinterMap &getPrinters() const override { return m_agent->getPrinters(); }

    void getDataItemsForPath(const DevicePtr device, const std::optional<std::string> &path,
                             FilterSet &filter,
                             const std::optional<std::string> &deviceType) const override
    {
      std::string dataPath = m_agent->devicesAndPath(path, device, deviceType);
      const auto &parser = m_agent->getXmlParser();
      parser->getDataItems(filter, dataPath);
    }

    buffer::CircularBuffer &getCircularBuffer() override { return m_agent->getCircularBuffer(); }

    configuration::HookManager<Agent> &getHooks(HookType type) override
    {
      using namespace sink;
      switch (type)
      {
        case BEFORE_START:
          return m_agent->beforeStartHooks();
          break;

        case AFTER_START:
          return m_agent->afterStartHooks();
          break;

        case BEFORE_STOP:
          return m_agent->beforeStopHooks();
          break;

        case BEFORE_DEVICE_XML_UPDATE:
          return m_agent->beforeDeviceXmlUpdateHooks();
          break;

        case AFTER_DEVICE_XML_UPDATE:
          return m_agent->afterDeviceXmlUpdateHooks();
          break;

        case BEFORE_INITIALIZE:
          return m_agent->beforeInitializeHooks();
          break;

        case AFTER_INITIALIZE:
          return m_agent->afterInitializeHooks();
          break;
      }

      LOG(error) << "getHooks: Bad hook manager type given to sink contract";
      throw std::runtime_error("getHooks: Bad hook manager type");

      // Never gets here.
      static configuration::HookManager<Agent> NullHooks;
      return NullHooks;
    }

  protected:
    Agent *m_agent;
  };

  inline sink::SinkContractPtr Agent::makeSinkContract()
  {
    return std::make_unique<AgentSinkContract>(this);
  }
}  // namespace mtconnect
