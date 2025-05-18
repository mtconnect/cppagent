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

#include <functional>
#include <list>
#include <string>

#include "mtconnect/config.hpp"

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      class DataItem;
    }
    class Device;
  }  // namespace device_model
  namespace asset {
    class Asset;
    using AssetPtr = std::shared_ptr<Asset>;
  }  // namespace asset
  namespace observation {
    class Observation;
  }
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;
  using DevicePtr = std::shared_ptr<device_model::Device>;
  using ObservationPtr = std::shared_ptr<observation::Observation>;
  using StringList = std::list<std::string>;

  namespace observation {
    class Observation;
    using ObservationPtr = std::shared_ptr<Observation>;
  }  // namespace observation
  namespace entity {
    class Entity;
    using EntityPtr = std::shared_ptr<Entity>;
  }  // namespace entity

  namespace pipeline {
    /// @brief The interface required by a pipeline to deliver and get information
    ///
    /// Provides the necessary methods for the pipeline to deliver entities and
    /// retrieve information about devices
    class AGENT_LIB_API PipelineContract
    {
    public:
      PipelineContract() = default;
      virtual ~PipelineContract() = default;

      using EachDataItem = std::function<void(const DataItemPtr di)>;

      /// @brief Find a device by name or uuid
      /// @param[in] device device name or uuid
      /// @return shared pointer to the device if found
      virtual DevicePtr findDevice(const std::string &device) = 0;
      /// @brief Find a data item for a device by name.
      /// @param[in] device name or uuid of the device
      /// @param[in] name name or id of the data item
      /// @return shared pointer to the data item if found
      virtual DataItemPtr findDataItem(const std::string &device, const std::string &name) = 0;
      /// @brief get the current schema version as an integer
      /// @returns the schema version as an integer [major * 100 + minor] as a 32bit integer.
      virtual int32_t getSchemaVersion() const = 0;
      /// @brief `true` if validation is turned on for the agent.
      /// @returns the validation state for the pipeline
      virtual bool isValidating() const = 0;
      /// @brief iterate through all the data items calling `fun` for each
      /// @param[in] fun The function or lambda to call
      virtual void eachDataItem(EachDataItem fun) = 0;
      /// @brief deliver an observation to the circular buffer and the sinks
      /// @param[in] obs a shared pointer to the observation
      virtual void deliverObservation(observation::ObservationPtr obs) = 0;
      /// @brief deliver an asset to the asset storage
      /// @param[in] asset the asset to deliver
      virtual void deliverAsset(asset::AssetPtr asset) = 0;
      /// @brief Deliver a list of device to the agent.
      /// @param[in] devices the new or changed devices
      virtual void deliverDevices(std::list<DevicePtr> devices) = 0;
      /// @brief Deliver a device to the agent.
      /// @param[in] device the new or changed device
      virtual void deliverDevice(DevicePtr device) = 0;
      /// @brief Deliver a command, remove or remova all
      /// @param[in]  command the command
      virtual void deliverAssetCommand(entity::EntityPtr command) = 0;
      /// @brief Deliver an agent related command
      /// @param[in]  command the command
      virtual void deliverCommand(entity::EntityPtr command) = 0;
      /// @brief Notify receiver of the status of a data source
      /// @param[in]  status the status of the source
      /// @param[in] devices a list of known devices
      /// @param[in] autoAvailable if the connection status should change availability
      virtual void deliverConnectStatus(entity::EntityPtr status, const StringList &devices,
                                        bool autoAvailable) = 0;
      /// @brief The source is no longer viable, do not try to reconnect
      /// @param[in] identity the identity of the source
      virtual void sourceFailed(const std::string &identity) = 0;

      /// @brief Check the observation with the current cache to determine if this is a
      /// duplicate
      /// @param[in] obs the observation to check
      /// @returns `obs` if it is not a duplicate, `nullptr` if it is. The observation
      /// may be modified if the observation needs to be subset.
      virtual const ObservationPtr checkDuplicate(const ObservationPtr &obs) const = 0;
    };
  }  // namespace pipeline
}  // namespace mtconnect
