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

#include <memory>
#include <list>

#include "observation/observation.hpp"
#include "asset/asset.hpp"
#include "device_model/device.hpp"
#include "printer.hpp"

namespace mtconnect
{
  class SinkContract
  {
  public:
    virtual Printer *getPrinter(const std::string &aType) const = 0;
    
    // Get device from device map
    virtual DevicePtr getDeviceByName(const std::string &name) const = 0;
    virtual DevicePtr findDeviceByUUIDorName(const std::string &idOrName) const = 0;
    virtual const std::list<DevicePtr> &getDevices() const = 0;
    virtual DevicePtr defaultDevice() const = 0;
    virtual DataItemPtr getDataItem(const std::string &name) const = 0;
  };
  
  using SinkContractPtr = std::unique_ptr<SinkContract>;
  
  class Sink
  {
  public:
    Sink(SinkContractPtr &&contract)
    : m_sinkContract(std::move(contract))
    {
    }
    virtual ~Sink() {}

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual uint64_t publish(observation::ObservationPtr &observation) = 0;
    virtual bool publish(AssetPtr asset) = 0;
    virtual bool removeAsset(DevicePtr device, const std::string &id,
                     const std::optional<Timestamp> time = std::nullopt) = 0;
    virtual bool removeAllAssets(const std::optional<std::string> device,
                         const std::optional<std::string> type,
                         const std::optional<Timestamp> time, AssetList &list) = 0;
    
  protected:
    std::unique_ptr<SinkContract> m_sinkContract;
  };

  using SinkList = std::list<std::unique_ptr<Sink>>;
}  // namespace mtconnect
