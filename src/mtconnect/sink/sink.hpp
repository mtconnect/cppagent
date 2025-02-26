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

#include <boost/function.hpp>
#include <boost/functional/factory.hpp>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "mtconnect/asset/asset_storage.hpp"
#include "mtconnect/buffer/circular_buffer.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/configuration/hook_manager.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/printer//printer.hpp"

namespace mtconnect {
  namespace printer {
    class Printer;
  }
  namespace source {
    class Source;
  }
  using PrinterMap = std::map<std::string, std::unique_ptr<printer::Printer>>;
  namespace pipeline {
    class PipelineContext;
  }
  namespace buffer {
    class CircularBuffer;
  }
  class Agent;

  /// @brief The Sink namespace for outgoing data from the agent
  namespace sink {
    /// @brief Interface required by sinks
    class AGENT_LIB_API SinkContract
    {
    public:
      enum HookType
      {
        BEFORE_STOP,
        BEFORE_START,
        AFTER_START,
        BEFORE_DEVICE_XML_UPDATE,
        AFTER_DEVICE_XML_UPDATE,
        BEFORE_INITIALIZE,
        AFTER_INITIALIZE
      };

      virtual ~SinkContract() = default;
      /// @brief get the printer for a mime type. Current options: `xml` or `json`.
      /// @param[in] aType a string for the type
      /// @return A pointer to a printer for that type. `nullptr` if not found
      virtual printer::Printer *getPrinter(const std::string &aType) const = 0;
      /// @brief get the map of type/printer pairs
      /// @return a reference to the map (ownership is not transferred).
      virtual const PrinterMap &getPrinters() const = 0;

      /// @brief find a device by name
      /// @param[in] name the name of the device
      /// @return shared pointer to the device if found
      virtual DevicePtr getDeviceByName(const std::string &name) const = 0;
      /// @brief find a device by its uuid or name
      /// @param idOrName the uuid or name
      /// @return shared pointer to the device if found
      virtual DevicePtr findDeviceByUUIDorName(const std::string &idOrName) const = 0;
      /// @brief get a list of all the devices
      /// @return a list of shared device pointers
      virtual const std::list<DevicePtr> getDevices() const = 0;
      /// @brief get the default device
      /// @return the default device
      ///
      /// This is the first device that is not the agent device
      virtual DevicePtr getDefaultDevice() const = 0;
      /// @brief get a data item by its unique id
      /// @param[in] id a unique id
      /// @return shared pointer to the data item if found
      virtual DataItemPtr getDataItemById(const std::string &id) const = 0;
      /// @brief find all the data items for a given XPath
      /// @param[in] device optional device to search
      /// @param[in] path the xpath to search
      /// @param[out] filter the set of all data items matching path to use for filtering
      virtual void getDataItemsForPath(
          const DevicePtr device, const std::optional<std::string> &path, FilterSet &filter,
          const std::optional<std::string> &deviceType = std::nullopt) const = 0;
      /// @brief Add a source for this sink.
      ///
      /// This is used to create loopback sources for a sink
      ///
      /// @param source shared pointer to the source
      virtual void addSource(std::shared_ptr<source::Source> source) = 0;
      /// @brief Get the common circular buffer
      /// @return a reference to the circular buffer
      virtual buffer::CircularBuffer &getCircularBuffer() = 0;

      /// @brief Get a pointer to the asset storage
      /// @return a pointer to the asset storage.
      virtual const asset::AssetStorage *getAssetStorage() = 0;

      /// @brief Get a reference to the hook manager for the agent.
      /// @param[in] type the type manager to retrieve
      /// @return a reference to the hook manager
      virtual configuration::HookManager<Agent> &getHooks(HookType type) = 0;

      /// @brief Shared pointer to the pipeline context
      std::shared_ptr<pipeline::PipelineContext> m_pipelineContext;

      using FindFile = std::function<std::optional<std::filesystem::path>(const std::string &)>;
      /// @brief function to find a configuration file
      FindFile m_findConfigFile;

      /// @brief function to find a data file
      FindFile m_findDataFile;
    };

    using SinkContractPtr = std::unique_ptr<SinkContract>;
    class Sink;
    using SinkPtr = std::shared_ptr<Sink>;

    /// @brief The factory callback or lambda to create this sink. Used for plugins.
    using SinkFactoryFn = boost::function<SinkPtr(
        const std::string &name, boost::asio::io_context &io, SinkContractPtr &&contract,
        const ConfigOptions &options, const boost::property_tree::ptree &block)>;

    /// @brief Abstract Sink
    class AGENT_LIB_API Sink : public std::enable_shared_from_this<Sink>
    {
    public:
      Sink(const std::string &name, SinkContractPtr &&contract)
        : m_sinkContract(std::move(contract)), m_name(name)
      {}
      virtual ~Sink() = default;

      /// @brief The shared_from_this pointer for this object
      /// @return shared pointer
      SinkPtr getptr() const { return const_cast<Sink *>(this)->shared_from_this(); }

      /// @brief Start the sink
      virtual void start() = 0;
      /// @brief stop the sink
      virtual void stop() = 0;

      /// @brief Receive an observation
      /// @param observation shared pointer to the observation
      /// @return `true` if the publishing was successful
      virtual bool publish(observation::ObservationPtr &observation) = 0;
      /// @brief Receive an asset
      /// @param asset shared point to the asset
      /// @return `true` if successful
      virtual bool publish(asset::AssetPtr asset) = 0;
      /// @brief Receive a device
      /// @param device shared pointer to the device
      /// @return `true` if successful
      virtual bool publish(device_model::DevicePtr device) { return false; }

      /// @brief Get the name of the Sink. Sinks should have unique names.
      /// @return the name
      const auto &getName() const { return m_name; }

    protected:
      std::unique_ptr<SinkContract> m_sinkContract;
      std::string m_name;
    };

    /// @brief Factory to create sinks
    class AGENT_LIB_API SinkFactory
    {
    public:
      /// @brief Register and associate the name with the Sink factory functon
      /// @param name the name
      /// @param function the factory
      void registerFactory(const std::string &name, SinkFactoryFn function)
      {
        m_factories.insert_or_assign(name, function);
      }

      /// @brief Clear all factories
      void clear() { m_factories.clear(); }

      /// @brief Check if a sink factory exists
      /// @param name the name of the factory
      /// @return `true` if the factory exits.
      bool hasFactory(const std::string &name) { return m_factories.count(name) > 0; }

      /// @brief Create a sink for a given name
      /// @param factoryName The name of the factory
      /// @param sinkName The sink to be created
      /// @param io a reference to the boost::asio io_context
      /// @param contract The SinkContract to give to the Sink
      /// @param options Configuration options for the sink
      /// @param block Additional configuration options for the Sink as a boost property tree.
      ///        These options need to be interpreted by the sink
      /// @return A shared pointer to the sink.
      SinkPtr make(const std::string &factoryName, const std::string &sinkName,
                   boost::asio::io_context &io, SinkContractPtr &&contract,
                   const ConfigOptions &options, const boost::property_tree::ptree &block);

    protected:
      std::map<std::string, SinkFactoryFn> m_factories;
    };

    using SinkList = std::list<SinkPtr>;
  }  // namespace sink
}  // namespace mtconnect
