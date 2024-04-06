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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <chrono>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/validator.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::asset;
using namespace mtconnect::device_model::data_item;
using namespace std;
using namespace date::literals;
using namespace std::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MockPipelineContract : public PipelineContract
{
public:
  MockPipelineContract(int32_t schemaVersion) : m_schemaVersion(schemaVersion) {}
  DevicePtr findDevice(const std::string &) override { return nullptr; }
  DataItemPtr findDataItem(const std::string &device, const std::string &name) override
  {
    return nullptr;
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override {}
  void deliverAsset(AssetPtr) override {}
  void deliverDevices(std::list<DevicePtr>) override {}
  int32_t getSchemaVersion() const override { return m_schemaVersion; }
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }

  int32_t m_schemaVersion;
};

class ObservationValidationTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(SCHEMA_VERSION(2, 5));
    m_validator = make_shared<Validator>(m_context);
    m_validator->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
    m_time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;

    ErrorList errors;
    m_dataItem =
        DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "EXECUTION"s}}, errors);
  }

  void TearDown() override
  {
    m_validator.reset();
    m_dataItem.reset();
    m_context.reset();
  }

  shared_ptr<Validator> m_validator;
  shared_ptr<PipelineContext> m_context;
  DataItemPtr m_dataItem;
  Timestamp m_time;
};

TEST_F(ObservationValidationTest, should_validate_value)
{
  ErrorList errors;
  auto event = Observation::make(m_dataItem, {{"VALUE", "READY"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}

TEST_F(ObservationValidationTest, unavailable_should_be_valid)
{
  ErrorList errors;
  auto event = Observation::make(m_dataItem, {{"VALUE", "UNAVAILABLE"s}}, m_time, errors);
  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}

TEST_F(ObservationValidationTest, should_detect_invalid_value)
{
  ErrorList errors;
  auto event = Observation::make(m_dataItem, {{"VALUE", "FLABOR"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("INVALID", quality);
}

TEST_F(ObservationValidationTest, should_not_validate_unknown_type)
{
  ErrorList errors;
  m_dataItem =
      DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "x:FLABOR"s}}, errors);

  auto event = Observation::make(m_dataItem, {{"VALUE", "FLABOR"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("UNVERIFIABLE", quality);
}

TEST_F(ObservationValidationTest, should_set_deprecated_flag_when_deprecated)
{
  ErrorList errors;
  m_dataItem =
      DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "EXECUTION"s}}, errors);

  auto event = Observation::make(m_dataItem, {{"VALUE", "PROGRAM_OPTIONAL_STOP"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);

  auto dep = evt->get<bool>("deprecated");
  ASSERT_TRUE(dep);
}

TEST_F(ObservationValidationTest, should_not_set_deprecated_flag_when_deprecated_version_greater)
{
  ErrorList errors;
  m_dataItem =
      DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "EXECUTION"s}}, errors);

  auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
  contract->m_schemaVersion = SCHEMA_VERSION(1, 3);

  auto event = Observation::make(m_dataItem, {{"VALUE", "PROGRAM_OPTIONAL_STOP"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);

  ASSERT_FALSE(evt->hasProperty("deprecated"));
}

TEST_F(ObservationValidationTest, should_not_validate_data_sets)
{
  ErrorList errors;
  m_dataItem = DataItem::make({{"id", "exec"s},
                               {"category", "EVENT"s},
                               {"type", "EXECUTION"s},
                               {"representation", "DATA_SET"s}},
                              errors);
  ASSERT_TRUE(m_dataItem->isDataSet());

  auto event =
      Observation::make(m_dataItem, {{"VALUE", DataSet({{"field", "value"s}})}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}

TEST_F(ObservationValidationTest, should_not_validate_tables)
{
  ErrorList errors;
  m_dataItem = DataItem::make({{"id", "exec"s},
                               {"category", "EVENT"s},
                               {"type", "EXECUTION"s},
                               {"representation", "TABLE"s}},
                              errors);
  ASSERT_TRUE(m_dataItem->isDataSet());

  auto event =
      Observation::make(m_dataItem, {{"VALUE", DataSet({{"field", "value"s}})}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}
