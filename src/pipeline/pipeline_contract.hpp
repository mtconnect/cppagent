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

namespace mtconnect
{
  namespace device_model
  {
    namespace data_item
    {
      class DataItem;
    }
  }  // namespace device_model
  class Device;
  class Asset;
  using AssetPtr = std::shared_ptr<Asset>;
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;

  namespace observation
  {
    class Observation;
    using ObservationPtr = std::shared_ptr<Observation>;
  }  // namespace observation
  namespace entity
  {
    class Entity;
    using EntityPtr = std::shared_ptr<Entity>;
  }  // namespace entity

  namespace pipeline
  {
    class PipelineContract
    {
    public:
      PipelineContract() = default;
      virtual ~PipelineContract() = default;

      using EachDataItem = std::function<void(const DataItemPtr di)>;

      virtual Device *findDevice(const std::string &device) = 0;
      virtual DataItemPtr findDataItem(const std::string &device, const std::string &name) = 0;
      virtual void eachDataItem(EachDataItem fun) = 0;
      virtual void deliverObservation(observation::ObservationPtr) = 0;
      virtual void deliverAsset(AssetPtr) = 0;
      virtual void deliverAssetCommand(entity::EntityPtr) = 0;
      virtual void deliverCommand(entity::EntityPtr) = 0;
      virtual void deliverConnectStatus(entity::EntityPtr, const StringList &devices,
                                        bool autoAvailable) = 0;
    };
  }  // namespace pipeline
}  // namespace mtconnect
