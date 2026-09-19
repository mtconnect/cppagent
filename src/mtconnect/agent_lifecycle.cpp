//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//

#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "mtconnect/agent.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/logging.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/printer/xml_printer.hpp"
#include "mtconnect/source/loopback_source.hpp"

using namespace std;

namespace mtconnect {
  using namespace device_model;
  using namespace data_item;
  using namespace entity;
  using namespace sink::rest_sink;
  namespace config = mtconnect::configuration;
  namespace fs = boost::filesystem;

  static const string g_unavailable("UNAVAILABLE");
  static const string g_available("AVAILABLE");

  void Agent::initialize(pipeline::PipelineContextPtr context)
  {
    NAMED_SCOPE("Agent::initialize");

    m_beforeInitializeHooks.exec(*this);

    m_pipelineContext = context;
    m_loopback =
        std::make_shared<source::LoopbackSource>("AgentSource", m_strand, context, m_options);

    auto devices = loadXMLDeviceFile(m_deviceXmlPath);
    if (!m_schemaVersion)
    {
      m_schemaVersion.emplace(StrDefaultSchemaVersion());
    }

    m_intSchemaVersion = IntSchemaVersion(*m_schemaVersion);
    for (auto& [k, pr] : m_printers)
      pr->setSchemaVersion(*m_schemaVersion);

    auto disableAgentDevice = GetOption<bool>(m_options, config::DisableAgentDevice);
    if (!(disableAgentDevice && *disableAgentDevice) && m_intSchemaVersion >= SCHEMA_VERSION(1, 7))
    {
      createAgentDevice();
    }

    // For the DeviceAdded event for each device
    for (auto device : devices)
      addDevice(device);

    if (m_versionDeviceXml && m_createUniqueIds)
      versionDeviceXml();

    loadCachedProbe();

    m_initialized = true;

    m_afterInitializeHooks.exec(*this);
  }

  void Agent::initialDataItemObservations()
  {
    NAMED_SCOPE("Agent::initialDataItemObservations");

    if (m_observationsInitialized)
      return;

    if (m_intSchemaVersion < SCHEMA_VERSION(2, 5) &&
        IsOptionSet(m_options, mtconnect::configuration::Validation))
    {
      m_validation = false;
      for (auto& printer : m_printers)
        printer.second->setValidation(false);
    }

    for (auto device : m_deviceIndex)
      initializeDataItems(device);

    if (m_agentDevice)
    {
      for (auto device : m_deviceIndex)
      {
        auto d = m_agentDevice->getDeviceDataItem("device_added");
        string uuid = *device->getUuid();

        entity::Properties props {{"VALUE", uuid}};
        if (m_intSchemaVersion >= SCHEMA_VERSION(2, 2))
        {
          const auto& hash = device->getProperty("hash");
          if (entity::ValueType(hash.index()) != entity::ValueType::EMPTY)
            props.insert_or_assign("hash", hash);
        }

        m_loopback->receive(d, props);
      }
    }

    m_observationsInitialized = true;
  }

  void Agent::start()
  {
    NAMED_SCOPE("Agent::start");

    if (m_started)
    {
      LOG(warning) << "Agent already started.";
      return;
    }

    try
    {
      m_beforeStartHooks.exec(*this);

      for (auto sink : m_sinks)
        sink->start();

      initialDataItemObservations();

      if (m_agentDevice)
      {
        auto d = m_agentDevice->getDeviceDataItem("agent_avail");
        m_loopback->receive(d, "AVAILABLE"s);
      }

      for (auto source : m_sources)
        source->start();

      m_afterStartHooks.exec(*this);
    }
    catch (std::runtime_error& e)
    {
      LOG(fatal) << "Cannot start server: " << e.what();
      throw FatalException(e.what());
    }

    m_started = true;
  }

  void Agent::stop()
  {
    NAMED_SCOPE("Agent::stop");

    if (!m_started)
    {
      LOG(warning) << "Agent already stopped.";
      return;
    }

    m_beforeStopHooks.exec(*this);

    LOG(info) << "Shutting down sources";
    for (auto source : m_sources)
      source->stop();

    LOG(info) << "Signaling observers to close sessions";
    for (auto di : m_dataItemMap)
    {
      auto ldi = di.second.lock();
      if (ldi)
        ldi->signalObservers(0);
    }

    LOG(info) << "Shutting down sinks";
    for (auto sink : m_sinks)
      sink->stop();

    LOG(info) << "Shutting down completed";
    m_started = false;
  }

  bool Agent::reloadDevices(const std::string& deviceFile)
  {
    try
    {
      // Load the configuration for the Agent
      auto devices = m_xmlParser->parseFile(
          deviceFile, dynamic_cast<printer::XmlPrinter*>(m_printers["xml"].get()));

      if (m_xmlParser->getSchemaVersion() &&
          IntSchemaVersion(*m_xmlParser->getSchemaVersion()) != m_intSchemaVersion)
      {
        LOG(info) << "Got version: " << *(m_xmlParser->getSchemaVersion());
        LOG(warning) << "Schema version does not match agent schema version, restarting the agent";
        return false;
      }

      bool changed = false;
      for (auto device : devices)
      {
        changed = receiveDevice(device, false) || changed;
      }
      if (changed)
        loadCachedProbe();

      return true;
    }
    catch (runtime_error& e)
    {
      LOG(fatal) << "Error loading xml configuration: " + deviceFile;
      LOG(fatal) << "Error detail: " << e.what();
      cerr << e.what() << endl;
      throw FatalException(e.what());
    }
    catch (exception& f)
    {
      LOG(fatal) << "Error loading xml configuration: " + deviceFile;
      LOG(fatal) << "Error detail: " << f.what();
      cerr << f.what() << endl;
      throw FatalException(f.what());
    }
  }

  void Agent::loadDeviceXml(const string& deviceXml, const optional<string> source)
  {
    try
    {
      auto printer = dynamic_cast<printer::XmlPrinter*>(m_printers["xml"].get());
      auto device = m_xmlParser->parseDevice(deviceXml, printer);
      if (device == nullptr)
      {
        LOG(error) << "Error loading device: " << deviceXml;
      }
      else
      {
        loadDevices({device}, source);
      }
    }
    catch (runtime_error& e)
    {
      LOG(error) << "Error loading device: " << deviceXml;
      LOG(error) << "Error detail: " << e.what();
      cerr << e.what() << endl;
    }
    catch (exception& f)
    {
      LOG(error) << "Error loading device: " << deviceXml;
      LOG(error) << "Error detail: " << f.what();
      cerr << f.what() << endl;
    }
  }

  void Agent::loadDevices(list<DevicePtr> devices, const optional<string> source, bool force)
  {
    if (!force && !IsOptionSet(m_options, config::EnableSourceDeviceModels))
    {
      LOG(warning) << "Device updates are disabled, skipping update";
      return;
    }

    auto callback = [=, this](config::AsyncContext& context) {
      try
      {
        bool changed = false;
        for (auto device : devices)
        {
          auto oldName = *device->getComponentName();
          auto oldDev = getDeviceByName(oldName);
          optional<string> oldUuid;
          if (oldDev)
          {
            oldUuid = *oldDev->getUuid();
          }

          auto uuid = *device->getUuid();
          auto name = *device->getComponentName();

          changed = receiveDevice(device, true) || changed;
          if (changed)
          {
            if (source)
            {
              auto s = findSource(*source);
              if (s)
              {
                s->setOptions({{config::Device, uuid}});
              }
            }

            for (auto src : m_sources)
            {
              auto adapter = std::dynamic_pointer_cast<source::adapter::Adapter>(src);
              if (adapter)
              {
                auto& options = adapter->getOptions();
                auto dev = GetOption<std::string>(options, config::Device);
                if (dev == oldName || dev == oldUuid)
                {
                  adapter->setOptions({{config::Device, uuid}});
                }
              }
            }
          }
        }

        if (changed)
          loadCachedProbe();
      }
      catch (FatalException& e)
      {
        throw e;
      }
      catch (runtime_error& e)
      {
        for (auto device : devices)
        {
          LOG(error) << "Error loading devices: " << *device->getComponentName();
        }
        LOG(error) << "Error detail: " << e.what();
        cerr << e.what() << endl;
      }
      catch (exception& f)
      {
        for (auto device : devices)
        {
          LOG(error) << "Error loading device: " << *device->getComponentName();
        }
        LOG(error) << "Error detail: " << f.what();
        cerr << f.what() << endl;
      }
    };

    // Gets around a race condition in the loading of adapaters and setting of
    // UUID.
    if (m_context.isRunning() && !m_context.isPauased())
      m_context.pause(callback);
    else
      callback(m_context);
  }

  void Agent::versionDeviceXml()
  {
    NAMED_SCOPE("Agent::versionDeviceXml");

    using namespace std::chrono;

    if (m_versionDeviceXml)
    {
      m_beforeDeviceXmlUpdateHooks.exec(*this);

      // update with a new version of the device.xml, saving the old one
      // with a date time stamp
      auto ext =
          date::format(".%Y-%m-%dT%H+%M+%SZ", date::floor<milliseconds>(system_clock::now()));
      fs::path file(m_deviceXmlPath);
      fs::path backup(m_deviceXmlPath + ext);
      if (!fs::exists(backup))
        fs::rename(file, backup);

      auto printer = getPrinter("xml");
      if (printer != nullptr)
      {
        std::list<DevicePtr> list;
        copy_if(m_deviceIndex.begin(), m_deviceIndex.end(), back_inserter(list),
                [](DevicePtr d) { return dynamic_cast<AgentDevice*>(d.get()) == nullptr; });
        auto probe = printer->printProbe(0, 0, 0, 0, 0, list, nullptr, true, true);

        ofstream devices(file.string());
        devices << probe;
        devices.close();

        m_afterDeviceXmlUpdateHooks.exec(*this);
      }
      else
      {
        LOG(error) << "Cannot find xml printer";
      }
    }
  }

  // ----------------------------------------------
  // Device management and Initialization
  // ----------------------------------------------

  std::list<device_model::DevicePtr> Agent::loadXMLDeviceFile(const std::string& configXmlPath)
  {
    NAMED_SCOPE("Agent::loadXMLDeviceFile");

    try
    {
      // Load the configuration for the Agent
      auto devices = m_xmlParser->parseFile(
          configXmlPath, dynamic_cast<printer::XmlPrinter*>(m_printers["xml"].get()));

      if (!m_schemaVersion && m_xmlParser->getSchemaVersion() &&
          !m_xmlParser->getSchemaVersion()->empty())
      {
        m_schemaVersion = m_xmlParser->getSchemaVersion();
        m_intSchemaVersion = IntSchemaVersion(*m_schemaVersion);
      }
      else if (!m_schemaVersion && !m_xmlParser->getSchemaVersion())
      {
        m_schemaVersion = StrDefaultSchemaVersion();
        m_intSchemaVersion = IntSchemaVersion(*m_schemaVersion);
      }

      return devices;
    }
    catch (runtime_error& e)
    {
      LOG(fatal) << "Error loading xml configuration: " + configXmlPath;
      LOG(fatal) << "Error detail: " << e.what();
      cerr << e.what() << endl;
      throw FatalException(e.what());
    }
    catch (exception& f)
    {
      LOG(fatal) << "Error loading xml configuration: " + configXmlPath;
      LOG(fatal) << "Error detail: " << f.what();
      cerr << f.what() << endl;
      throw FatalException(f.what());
    }

    return {};
  }

  void Agent::verifyDevice(DevicePtr device)
  {
    NAMED_SCOPE("Agent::verifyDevice");

    // Add the devices to the device map and create availability and
    // asset changed events if they don't exist
    // Make sure we have two device level data items:
    // 1. Availability
    // 2. AssetChanged
    if (!device->getAvailability())
    {
      // Create availability data item and add it to the device.
      entity::ErrorList errors;
      auto di = DataItem::make(
          {{"type", "AVAILABILITY"s}, {"id", device->getId() + "_avail"}, {"category", "EVENT"s}},
          errors);
      device->addDataItem(di, errors);
    }

    if (!device->getAssetChanged() && m_intSchemaVersion >= SCHEMA_VERSION(1, 2))
    {
      entity::ErrorList errors;
      // Create asset change data item and add it to the device.
      auto di = DataItem::make({{"type", "ASSET_CHANGED"s},
                                {"id", device->getId() + "_asset_chg"},
                                {"category", "EVENT"s}},
                               errors);
      device->addDataItem(di, errors);
    }

    if (device->getAssetChanged() && m_intSchemaVersion >= SCHEMA_VERSION(1, 5))
    {
      auto di = device->getAssetChanged();
      if (!di->isDiscrete())
        di->makeDiscrete();
    }

    if (!device->getAssetRemoved() && m_intSchemaVersion >= SCHEMA_VERSION(1, 3))
    {
      // Create asset removed data item and add it to the device.
      entity::ErrorList errors;
      auto di = DataItem::make({{"type", "ASSET_REMOVED"s},
                                {"id", device->getId() + "_asset_rem"},
                                {"category", "EVENT"s}},
                               errors);
      device->addDataItem(di, errors);
    }

    if (!device->getAssetAdded() && m_intSchemaVersion >= SCHEMA_VERSION(2, 6))
    {
      // Create asset removed data item and add it to the device.
      entity::ErrorList errors;
      auto di = DataItem::make({{"type", "ASSET_ADDED"s},
                                {"id", device->getId() + "_asset_add"},
                                {"discrete", true},
                                {"category", "EVENT"s}},
                               errors);
      device->addDataItem(di, errors);
    }

    if (!device->getAssetCount() && m_intSchemaVersion >= SCHEMA_VERSION(2, 0))
    {
      entity::ErrorList errors;
      auto di = DataItem::make({{"type", "ASSET_COUNT"s},
                                {"id", device->getId() + "_asset_count"},
                                {"category", "EVENT"s},
                                {"representation", "DATA_SET"s}},
                               errors);
      device->addDataItem(di, errors);
    }

    if (IsOptionSet(m_options, mtconnect::configuration::PreserveUUID))
    {
      device->setPreserveUuid(*GetOption<bool>(m_options, mtconnect::configuration::PreserveUUID));
    }
  }

  void Agent::initializeDataItems(DevicePtr device, std::optional<std::set<std::string>> skip)
  {
    NAMED_SCOPE("Agent::initializeDataItems");

    // Initialize the id mapping for the devices and set all data items to UNAVAILABLE
    for (auto item : device->getDeviceDataItems())
    {
      if (item.expired())
        continue;

      auto d = item.lock();
      if ((!skip || skip->count(d->getId()) > 0) && m_dataItemMap.count(d->getId()) > 0)
      {
        auto di = m_dataItemMap[d->getId()].lock();
        if (di && di != d)
        {
          stringstream msg;

          msg << "Duplicate DataItem id " << d->getId()
              << " for device: " << *device->getComponentName()
              << ". Try using configuration option CreateUniqueIds to resolve.";
          LOG(fatal) << msg.str();
          throw FatalException(msg.str());
        }
      }
      else
      {
        // Check for single valued constrained data items.
        const string* value = &g_unavailable;
        if (d->isCondition())
          value = &g_unavailable;
        else if (d->getConstantValue())
          value = &d->getConstantValue().value();

        m_loopback->receive(d, *value);
        m_dataItemMap[d->getId()] = d;
      }
    }
  }

  // Add the a device from a configuration file
  void Agent::addDevice(DevicePtr device)
  {
    NAMED_SCOPE("Agent::addDevice");

    // Check if device already exists
    string uuid = *device->getUuid();
    auto& idx = m_deviceIndex.get<ByUuid>();
    auto old = idx.find(uuid);
    if (old != idx.end())
    {
      // Update existing device
      stringstream msg;
      msg << "Device " << *device->getUuid() << " already exists. "
          << " Update not supported yet";
      throw msg.str();
    }
    else
    {
      m_deviceIndex.push_back(device);

      // TODO: Redo Resolve Reference  with entity
      // device->resolveReferences();
      verifyDevice(device);
      createUniqueIds(device);

      if (m_observationsInitialized)
      {
        initializeDataItems(device);

        // Check for single valued constrained data items.
        if (m_agentDevice && device != m_agentDevice)
        {
          entity::Properties props {{"VALUE", uuid}};
          if (m_intSchemaVersion >= SCHEMA_VERSION(2, 2))
          {
            const auto& hash = device->getProperty("hash");
            if (ValueType(hash.index()) != ValueType::EMPTY)
              props.insert_or_assign("hash", hash);
          }

          auto d = m_agentDevice->getDeviceDataItem("device_added");
          m_loopback->receive(d, props);
        }
      }
    }

    if (m_intSchemaVersion >= SCHEMA_VERSION(2, 2))
      device->addHash();

    for (auto& printer : m_printers)
      printer.second->setModelChangeTime(getCurrentTime(GMT_UV_SEC));
  }

  void Agent::deviceChanged(DevicePtr device, const std::string& uuid)
  {
    NAMED_SCOPE("Agent::deviceChanged");

    bool changed = false;
    string oldUuid = *device->getUuid();
    if (uuid != oldUuid)
    {
      changed = true;
      if (m_agentDevice)
      {
        auto d = m_agentDevice->getDeviceDataItem("device_removed");
        if (d)
          m_loopback->receive(d, oldUuid);
      }
    }

    if (changed)
    {
      // Create a new device
      auto xmlPrinter = dynamic_cast<printer::XmlPrinter*>(m_printers["xml"].get());
      auto newDevice = m_xmlParser->parseDevice(xmlPrinter->printDevice(device), xmlPrinter);

      newDevice->setUuid(uuid);

      m_loopback->receive(newDevice);
    }
  }

  void Agent::createUniqueIds(DevicePtr device)
  {
    if (m_createUniqueIds && !dynamic_pointer_cast<AgentDevice>(device))
    {
      std::unordered_map<std::string, std::string> idMap;

      device->createUniqueIds(idMap);
      device->updateReferences(idMap);

      // Update the data item map.
      for (auto& id : idMap)
      {
        auto di = device->getDeviceDataItem(id.second);
        if (auto it = m_dataItemMap.find(id.first); it != m_dataItemMap.end())
        {
          m_dataItemMap.erase(it);
          m_dataItemMap.emplace(id.second, di);
        }
      }
    }
  }

  void Agent::loadCachedProbe()
  {
    NAMED_SCOPE("Agent::loadCachedProbe");

    // Reload the document for path resolution
    auto xmlPrinter = dynamic_cast<printer::XmlPrinter*>(m_printers["xml"].get());
    m_xmlParser->loadDocument(xmlPrinter->printProbe(0, 0, 0, 0, 0, getDevices()));

    for (auto& printer : m_printers)
      printer.second->setModelChangeTime(getCurrentTime(GMT_UV_SEC));
  }

}  // namespace mtconnect
