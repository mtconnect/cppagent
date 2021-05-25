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

#include <list>
#include <map>
#include <memory>
#include <string>

#include "asset/asset_storage.hpp"
#include "device_model/device.hpp"
#include "observation/observation.hpp"
#include "printer.hpp"

namespace mtconnect
{
  class Printer;
  using PrinterMap = std::map<std::string, std::unique_ptr<Printer>>;
  class SinkContract
  {
  public:
    virtual ~SinkContract() = default;
    virtual Printer *getPrinter(const std::string &aType) const = 0;
    virtual const PrinterMap &getPrinters() const = 0;

    // Get device from device map
    virtual DevicePtr getDeviceByName(const std::string &name) const = 0;
    virtual DevicePtr findDeviceByUUIDorName(const std::string &idOrName) const = 0;
    virtual const std::list<DevicePtr> &getDevices() const = 0;
    virtual DevicePtr defaultDevice() const = 0;
    virtual DataItemPtr getDataItemById(const std::string &id) const = 0;
    virtual void getDataItemsForPath(const DevicePtr device, const std::optional<std::string> &path,
                                     FilterSet &filter) const = 0;

    // Asset information
    virtual const asset::AssetStorage *getAssetStorage() = 0;
  };

  using SinkContractPtr = std::unique_ptr<SinkContract>;

  class Sink
  {
  public:
    Sink(const std::string &name, SinkContractPtr &&contract)
      : m_sinkContract(std::move(contract)), m_name(name)
    {
    }
    virtual ~Sink() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual uint64_t publish(observation::ObservationPtr &observation) = 0;
    virtual bool publish(asset::AssetPtr asset) = 0;

    const auto &getName() const { return m_name; }

  protected:
    std::unique_ptr<SinkContract> m_sinkContract;
    std::string m_name;
  };

  using SinkPtr = std::shared_ptr<Sink>;
  using SinkList = std::list<SinkPtr>;
}  // namespace mtconnect
