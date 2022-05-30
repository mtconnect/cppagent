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

#include "source/loopback_source.hpp"

#include "configuration/config_options.hpp"
#include "device_model/device.hpp"
#include "entity/xml_parser.hpp"
#include "pipeline/convert_sample.hpp"
#include "pipeline/deliver.hpp"
#include "pipeline/delta_filter.hpp"
#include "pipeline/duplicate_filter.hpp"
#include "pipeline/period_filter.hpp"
#include "pipeline/timestamp_extractor.hpp"
#include "pipeline/upcase_value.hpp"

using namespace std;

namespace mtconnect::source {
  using namespace observation;
  using namespace asset;
  using namespace pipeline;
  void LoopbackPipeline::build(const ConfigOptions &options)
  {
    clear();
    TransformPtr next = m_start;

    next->bind(make_shared<DeliverAsset>(m_context));
    next->bind(make_shared<DeliverAssetCommand>(m_context));

    if (IsOptionSet(m_options, configuration::UpcaseDataItemValue))
      next = next->bind(make_shared<UpcaseValue>());

    // Filter dups, by delta, and by period
    next = next->bind(make_shared<DuplicateFilter>(m_context));
    next = next->bind(make_shared<DeltaFilter>(m_context));
    next = next->bind(make_shared<PeriodFilter>(m_context, m_strand));

    // Convert values
    if (IsOptionSet(m_options, configuration::ConversionRequired))
      next = next->bind(make_shared<ConvertSample>());

    // Deliver
    next->bind(make_shared<DeliverObservation>(m_context));
    applySplices();
  }

  SequenceNumber_t LoopbackSource::receive(DataItemPtr dataItem, entity::Properties props,
                                           std::optional<Timestamp> timestamp)
  {
    entity::ErrorList errors;

    Timestamp ts = timestamp ? *timestamp : chrono::system_clock::now();
    auto observation = observation::Observation::make(dataItem, props, ts, errors);
    if (observation && errors.empty())
    {
      return receive(observation);
    }
    else
    {
      LOG(error) << "Cannot add observation: ";
      for (auto &e : errors)
      {
        LOG(error) << "Cannot add observation: " << e->what();
      }
    }

    return 0;
  }

  SequenceNumber_t LoopbackSource::receive(DataItemPtr dataItem, const std::string &value,
                                           std::optional<Timestamp> timestamp)
  {
    if (dataItem->isCondition())
      return receive(dataItem, {{"level", value}}, timestamp);
    else
      return receive(dataItem, {{"VALUE", value}}, timestamp);
  }

  SequenceNumber_t LoopbackSource::receive(const std::string &data)
  {
    auto ent = make_shared<Entity>("Data", Properties {{"VALUE", data}, {"source", getIdentity()}});
    auto res = m_pipeline.run(ent);
    if (auto obs = std::dynamic_pointer_cast<observation::Observation>(res))
    {
      return obs->getSequence();
    }
    else
    {
      return 0;
    }
  }

  AssetPtr LoopbackSource::receiveAsset(DevicePtr device, const std::string &document,
                                        const std::optional<std::string> &id,
                                        const std::optional<std::string> &type,
                                        const std::optional<std::string> &time,
                                        entity::ErrorList &errors)
  {
    // Parse the asset
    auto entity = entity::XmlParser::parse(asset::Asset::getRoot(), document, "1.7", errors);
    if (!entity)
    {
      LOG(warning) << "Asset could not be parsed";
      LOG(warning) << document;
      for (auto &e : errors)
        LOG(warning) << e->what();
      return nullptr;
    }

    auto asset = dynamic_pointer_cast<asset::Asset>(entity);

    if (type && asset->getType() != *type)
    {
      stringstream msg;
      msg << "Asset types do not match: "
          << "Parsed type: " << asset->getType() << " does not match " << *type;
      LOG(warning) << msg.str();
      LOG(warning) << document;
      errors.emplace_back(make_unique<entity::EntityError>(msg.str()));
      return asset;
    }

    if (!id && !asset->hasProperty("assetId"))
    {
      stringstream msg;
      msg << "Asset does not have an assetId and assetId not given";
      LOG(warning) << msg.str();
      LOG(warning) << document;
      errors.emplace_back(make_unique<entity::EntityError>(msg.str()));
      return asset;
    }

    if (id)
      asset->setAssetId(*id);

    if (time)
      asset->setProperty("timestamp", *time);

    auto ad = asset->getDeviceUuid();
    if (!ad)
      asset->setProperty("deviceUuid", *device->getUuid());

    receive(asset);

    return asset;
  }

  void LoopbackSource::removeAsset(const std::optional<std::string> device, const std::string &id)
  {
    auto ac = make_shared<AssetCommand>("AssetCommand", Properties {});
    ac->m_timestamp = chrono::system_clock::now();
    ac->setValue("RemoveAsset"s);
    ac->setProperty("assetId", id);
    if (device)
      ac->setProperty("device", *device);

    m_pipeline.run(ac);
  }

}  // namespace mtconnect::source
