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

#include <boost/function.hpp>
#include <boost/functional/factory.hpp>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "asset/asset_storage.hpp"
#include "buffer/circular_buffer.hpp"
#include "device_model/device.hpp"
#include "observation/observation.hpp"
#include "printer/printer.hpp"

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

  namespace sink {
    class SinkContract
    {
    public:
      virtual ~SinkContract() = default;
      virtual printer::Printer *getPrinter(const std::string &aType) const = 0;
      virtual const PrinterMap &getPrinters() const = 0;

      // Get device from device map
      virtual DevicePtr getDeviceByName(const std::string &name) const = 0;
      virtual DevicePtr findDeviceByUUIDorName(const std::string &idOrName) const = 0;
      virtual const std::list<DevicePtr> getDevices() const = 0;
      virtual DevicePtr defaultDevice() const = 0;
      virtual DataItemPtr getDataItemById(const std::string &id) const = 0;
      virtual void getDataItemsForPath(const DevicePtr device,
                                       const std::optional<std::string> &path,
                                       FilterSet &filter) const = 0;
      virtual void addSource(std::shared_ptr<source::Source> source) = 0;
      virtual buffer::CircularBuffer &getCircularBuffer() = 0;

      // Asset information
      virtual const asset::AssetStorage *getAssetStorage() = 0;

      std::shared_ptr<pipeline::PipelineContext> m_pipelineContext;
    };

    using SinkContractPtr = std::unique_ptr<SinkContract>;
    class Sink;
    using SinkPtr = std::shared_ptr<Sink>;
    using SinkFactoryFn = boost::function<SinkPtr(
        const std::string &name, boost::asio::io_context &io, SinkContractPtr &&contract,
        const ConfigOptions &options, const boost::property_tree::ptree &block)>;

    class Sink
    {
    public:
      Sink(const std::string &name, SinkContractPtr &&contract)
        : m_sinkContract(std::move(contract)), m_name(name)
      {}
      virtual ~Sink() = default;

      virtual void start() = 0;
      virtual void stop() = 0;

      virtual bool publish(observation::ObservationPtr &observation) = 0;
      virtual bool publish(asset::AssetPtr asset) = 0;
      virtual bool publish(device_model::DevicePtr device) { return false; }

      const auto &getName() const { return m_name; }

    protected:
      std::unique_ptr<SinkContract> m_sinkContract;
      std::string m_name;
    };

    class SinkFactory
    {
    public:
      void registerFactory(const std::string &name, SinkFactoryFn function)
      {
        m_factories.insert_or_assign(name, function);
      }

      void clear() { m_factories.clear(); }

      bool hasFactory(const std::string &name) { return m_factories.count(name) > 0; }

      SinkPtr make(const std::string &factoryName, const std::string &sinkName,
                   boost::asio::io_context &io, SinkContractPtr &&contract,
                   const ConfigOptions &options, const boost::property_tree::ptree &block);

    protected:
      std::map<std::string, SinkFactoryFn> m_factories;
    };

    using SinkList = std::list<SinkPtr>;
  }  // namespace sink
}  // namespace mtconnect
