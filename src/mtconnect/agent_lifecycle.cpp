//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//

#include "mtconnect/agent.hpp"

#include "mtconnect/logging.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/source/loopback_source.hpp"

using namespace std;

namespace mtconnect {
  using namespace device_model;

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

  Agent::~Agent()
  {
    m_xmlParser.reset();
    m_sinks.clear();
    m_sources.clear();
    m_agentDevice = nullptr;
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
}  // namespace mtconnect
